require('dotenv').config();
const express = require('express');
const OpenAI  = require('openai');
const fs      = require('fs');
const path    = require('path');

const app    = express();
const openai = new OpenAI({ apiKey: process.env.OPENAI_API_KEY });
const PORT   = 3000;
const DATA_FILE = path.join(__dirname, 'data.json');

// ── Persistence ───────────────────────────────────────────────────
const DIFFICULTIES = ['easy', 'medium', 'hard'];

function loadData() {
  if (!fs.existsSync(DATA_FILE)) return { wakeups: [], subjects: {}, difficulty: 'easy' };
  try {
    const d = JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
    if (!d.difficulty) d.difficulty = 'easy';
    return d;
  }
  catch { return { wakeups: [], subjects: {}, difficulty: 'easy' }; }
}

function pickSubject(subjectStats) {
  const weights = SUBJECTS.map(name => {
    const s = subjectStats[name];
    if (!s || s.attempts < 3) return 1.5;
    const accuracy = s.correct / s.attempts;
    return Math.max(0.2, 2 - accuracy * 2);
  });
  const total = weights.reduce((a, b) => a + b, 0);
  let r = Math.random() * total;
  for (let i = 0; i < SUBJECTS.length; i++) {
    r -= weights[i];
    if (r <= 0) return SUBJECTS[i];
  }
  return SUBJECTS[SUBJECTS.length - 1];
}

function saveData(data) {
  fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
}

function today() {
  return new Date().toISOString().split('T')[0];
}

function calculateStreak(wakeups) {
  if (!wakeups.length) return 0;
  const dates = [...new Set(wakeups.map(w => w.date))].sort().reverse();
  const todayStr = today();
  const yesterday = new Date(Date.now() - 86400000).toISOString().split('T')[0];
  if (dates[0] !== todayStr && dates[0] !== yesterday) return 0;
  let streak = 0;
  let expected = dates[0];
  for (const date of dates) {
    if (date === expected) {
      streak++;
      const d = new Date(expected + 'T12:00:00Z');
      d.setUTCDate(d.getUTCDate() - 1);
      expected = d.toISOString().split('T')[0];
    } else break;
  }
  return streak;
}

// ── Subjects ──────────────────────────────────────────────────────
const SUBJECTS = [
  'addition and subtraction',
  'multiplication and division',
  'fractions and decimals',
  'basic geometry shapes',
  'telling time and calendars',
  'US geography and states',
  'world capitals',
  'animals and habitats',
  'the solar system and planets',
  'human body and health',
  'plant life cycles',
  'weather and seasons',
  'US history and presidents',
  'world history landmarks',
  'grammar and parts of speech',
  'vocabulary and word meanings',
  'famous scientists and inventors',
  'basic economics and money',
  'community helpers and jobs',
  'colors, art, and music basics',
];

const SYSTEM_PROMPT = `You are a quiz question generator for an elementary school educational alarm clock.
Return ONLY valid JSON with this exact structure, no markdown, no extra text:
{
  "question": "...",
  "options": ["A text", "B text", "C text", "D text"],
  "correct_index": 0,
  "difficulty": "easy"
}
Rules:
- correct_index is 0-3
- options array must have exactly 4 items
- keep questions short enough to fit on a small screen (max 80 chars)
- keep each option under 20 chars
- questions must be appropriate for elementary school students (grades 1-5)`;

// ── Routes ────────────────────────────────────────────────────────
app.get('/question', async (req, res) => {
  const data       = loadData();
  const difficulty = data.difficulty;
  const subject    = pickSubject(data.subjects);

  const gradeHint = difficulty === 'easy' ? 'grades 1-2' : difficulty === 'medium' ? 'grades 3-4' : 'grades 4-5';

  try {
    const completion = await openai.chat.completions.create({
      model: 'gpt-4o-mini',
      messages: [
        { role: 'system', content: SYSTEM_PROMPT },
        { role: 'user',   content: `Generate a ${difficulty} difficulty (${gradeHint}) quiz question about: ${subject}.` }
      ],
      temperature: 1.0,
    });

    const raw = completion.choices[0].message.content.trim();
    const question = JSON.parse(raw);
    question.subject    = subject;
    question.difficulty = difficulty;
    res.json(question);
  } catch (err) {
    console.error('OpenAI error:', err.message);
    res.status(500).json({ error: 'failed to generate question', detail: err.message });
  }
});

app.get('/report/correct', (req, res) => {
  const subject = req.query.subject || 'unknown';
  const data = loadData();
  if (!data.subjects[subject]) data.subjects[subject] = { correct: 0, attempts: 0 };
  data.subjects[subject].correct++;
  data.subjects[subject].attempts++;
  saveData(data);
  res.send('ok');
});

app.get('/report/wakeup', (req, res) => {
  const correct = parseInt(req.query.correct) || 0;
  const wrong   = parseInt(req.query.wrong)   || 0;
  const data = loadData();
  const now  = new Date();

  // Adjust difficulty: <3 correct → easier, >3 → harder, ==3 → same
  let diffIdx = DIFFICULTIES.indexOf(data.difficulty);
  if (correct < 3)      diffIdx = Math.max(0, diffIdx - 1);
  else if (correct > 3) diffIdx = Math.min(2, diffIdx + 1);
  data.difficulty = DIFFICULTIES[diffIdx];

  data.wakeups.push({
    date:       now.toISOString().split('T')[0],
    time:       now.toTimeString().slice(0, 5),
    correct,
    wrong,
    difficulty: data.difficulty,
  });
  saveData(data);
  res.send('ok');
});

app.get('/stats', (req, res) => {
  const data   = loadData();
  const streak = calculateStreak(data.wakeups);
  res.json({ streak, difficulty: data.difficulty });
});

app.get('/portal', (req, res) => {
  const data         = loadData();
  const streak       = calculateStreak(data.wakeups);
  const recent       = [...data.wakeups].reverse().slice(0, 10);
  const totalSessions = data.wakeups.length;

  const totalCorrect = data.wakeups.reduce((s, w) => s + w.correct, 0);
  const totalWrong   = data.wakeups.reduce((s, w) => s + w.wrong,   0);
  const accuracy     = (totalCorrect + totalWrong) > 0
    ? Math.round((totalCorrect / (totalCorrect + totalWrong)) * 100) : 0;

  const bestSubject = Object.entries(data.subjects)
    .filter(([, s]) => s.attempts >= 2)
    .sort((a, b) => (b[1].correct / b[1].attempts) - (a[1].correct / a[1].attempts))[0];

  const statusMsg = accuracy >= 85 && streak >= 5
    ? "Your little one is on fire! Keep it up!"
    : accuracy >= 70 || streak >= 3
    ? "Great progress — things are clicking!"
    : totalSessions === 0
    ? "No sessions yet. Time to get learning!"
    : "Still warming up — every session counts!";

  const diffColor = { easy: '#10b981', medium: '#f59e0b', hard: '#ef4444' };

  const subjectRows = Object.entries(data.subjects)
    .sort((a, b) => (b[1].correct / b[1].attempts) - (a[1].correct / a[1].attempts))
    .map(([name, s]) => {
      const pct = s.attempts ? Math.round((s.correct / s.attempts) * 100) : 0;
      const color = pct >= 80 ? '#10b981' : pct >= 50 ? '#f59e0b' : '#ef4444';
      return `<tr>
        <td class="subj-name">${name.charAt(0).toUpperCase() + name.slice(1)}</td>
        <td class="bar-cell">
          <div class="bar-track"><div class="bar-fill" style="width:${pct}%;background:${color}"></div></div>
        </td>
        <td class="pct-cell" style="color:${color}">${pct}%</td>
        <td class="score-cell">${s.correct}/${s.attempts}</td>
      </tr>`;
    }).join('');

  const sessionRows = recent.map(w => {
    const perfect = w.wrong === 0 && w.correct > 0;
    const total   = w.correct + w.wrong;
    const dc      = diffColor[w.difficulty] || '#94a3b8';
    return `<tr>
      <td>${w.date}</td>
      <td>${w.time}</td>
      <td><span class="score-pill">${w.correct}/${total}</span></td>
      <td>${w.difficulty ? `<span class="diff-pill" style="background:${dc}20;color:${dc};border:1px solid ${dc}40">${w.difficulty}</span>` : '—'}</td>
      <td>${perfect ? '<span class="perfect-pill">Perfect!</span>' : ''}</td>
    </tr>`;
  }).join('');

  res.send(`<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Edugotchi — Parent Dashboard</title>
  <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@400;500;600;700;800&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      font-family: 'Poppins', system-ui, sans-serif;
      background: #f0f4ff;
      color: #1e1b4b;
      min-height: 100vh;
    }

    header {
      background: linear-gradient(135deg, #6366f1 0%, #a855f7 50%, #ec4899 100%);
      padding: 36px 32px 32px;
      color: white;
      text-align: center;
    }
    .logo { font-size: 2.8rem; font-weight: 800; letter-spacing: -1px; }
    .logo span { opacity: 0.75; font-weight: 500; font-size: 1rem; display: block; margin-top: 2px; letter-spacing: 0; }
    .status-badge {
      display: inline-block; margin-top: 14px;
      background: rgba(255,255,255,0.2); backdrop-filter: blur(4px);
      border: 1px solid rgba(255,255,255,0.3);
      padding: 8px 20px; border-radius: 99px;
      font-size: 0.9rem; font-weight: 500;
    }

    main { max-width: 960px; margin: 0 auto; padding: 32px 16px 60px; }

    .stat-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 16px;
      margin-bottom: 28px;
    }
    .stat-card {
      background: white; border-radius: 20px;
      padding: 24px 20px; text-align: center;
      box-shadow: 0 2px 12px rgba(99,102,241,0.08);
      border: 1px solid rgba(99,102,241,0.08);
    }
    .stat-icon { font-size: 1.8rem; margin-bottom: 8px; }
    .stat-num  { font-size: 3rem; font-weight: 800; line-height: 1; }
    .stat-label { font-size: 0.72rem; font-weight: 600; color: #94a3b8; margin-top: 6px; text-transform: uppercase; letter-spacing: 0.08em; }
    .stat-sub  { font-size: 0.85rem; font-weight: 600; color: #64748b; margin-top: 6px; }

    .c-purple { color: #6366f1; }
    .c-pink   { color: #ec4899; }
    .c-green  { color: #10b981; }
    .c-amber  { color: #f59e0b; }

    .card {
      background: white; border-radius: 20px;
      padding: 28px; margin-bottom: 20px;
      box-shadow: 0 2px 12px rgba(99,102,241,0.08);
      border: 1px solid rgba(99,102,241,0.08);
    }
    .card h2 {
      font-size: 1rem; font-weight: 700; color: #1e1b4b;
      margin-bottom: 20px; display: flex; align-items: center; gap: 8px;
    }
    .card h2 .pill {
      font-size: 0.7rem; font-weight: 600; color: #6366f1;
      background: #ede9fe; padding: 2px 10px; border-radius: 99px; margin-left: 4px;
    }

    table { width: 100%; border-collapse: collapse; }
    th {
      text-align: left; padding: 8px 12px;
      font-size: 0.7rem; font-weight: 700; color: #94a3b8;
      text-transform: uppercase; letter-spacing: 0.08em;
      border-bottom: 2px solid #f1f5f9;
    }
    td { padding: 12px 12px; border-bottom: 1px solid #f8fafc; font-size: 0.9rem; font-weight: 500; color: #334155; }
    tr:last-child td { border-bottom: none; }
    tr:hover td { background: #fafbff; }

    .subj-name { font-weight: 600; color: #1e1b4b; }
    .bar-cell  { width: 200px; }
    .bar-track { background: #f1f5f9; border-radius: 99px; height: 8px; }
    .bar-fill  { height: 8px; border-radius: 99px; transition: width 0.5s ease; }
    .pct-cell  { font-weight: 700; width: 56px; }
    .score-cell { color: #94a3b8; }

    .score-pill {
      display: inline-block; background: #ede9fe; color: #6366f1;
      font-weight: 700; font-size: 0.85rem;
      padding: 2px 12px; border-radius: 99px;
    }
    .diff-pill {
      display: inline-block; font-size: 0.75rem; font-weight: 600;
      padding: 2px 10px; border-radius: 99px; text-transform: capitalize;
    }
    .perfect-pill {
      display: inline-block; background: linear-gradient(135deg,#fde68a,#fbbf24);
      color: #92400e; font-size: 0.75rem; font-weight: 700;
      padding: 2px 12px; border-radius: 99px;
    }

    .empty { text-align: center; color: #cbd5e1; padding: 40px 0; font-size: 0.95rem; font-weight: 500; }
    footer { text-align: center; color: #cbd5e1; font-size: 0.8rem; margin-top: 8px; font-weight: 500; }
  </style>
</head>
<body>
  <header>
    <div class="logo">Edugotchi <span>Parent Dashboard</span></div>
    <div class="status-badge">${statusMsg}</div>
  </header>

  <main>
    <div class="stat-grid">
      <div class="stat-card">
        <div class="stat-icon">🔥</div>
        <div class="stat-num c-amber">${streak}</div>
        <div class="stat-label">Day Streak</div>
      </div>
      <div class="stat-card">
        <div class="stat-icon">✅</div>
        <div class="stat-num c-purple">${totalSessions}</div>
        <div class="stat-label">Sessions</div>
      </div>
      <div class="stat-card">
        <div class="stat-icon">🎯</div>
        <div class="stat-num c-green">${accuracy}%</div>
        <div class="stat-label">Accuracy</div>
      </div>
      <div class="stat-card">
        <div class="stat-icon">📈</div>
        <div class="stat-label" style="margin-bottom:6px">Current Level</div>
        <div class="stat-sub" style="font-size:1.1rem;font-weight:700;color:${diffColor[data.difficulty]}">${data.difficulty.charAt(0).toUpperCase()+data.difficulty.slice(1)}</div>
        <div class="stat-label" style="margin-top:6px">Best Subject</div>
        <div class="stat-sub">${bestSubject ? bestSubject[0].charAt(0).toUpperCase()+bestSubject[0].slice(1) : 'N/A'}</div>
      </div>
    </div>

    <div class="card">
      <h2>Subject Performance <span class="pill">by accuracy</span></h2>
      ${subjectRows ? `<table>
        <thead><tr><th>Subject</th><th>Progress</th><th>Accuracy</th><th>Score</th></tr></thead>
        <tbody>${subjectRows}</tbody>
      </table>` : '<div class="empty">No questions answered yet — start a session!</div>'}
    </div>

    <div class="card">
      <h2>Session History <span class="pill">last ${Math.min(recent.length,10)}</span></h2>
      ${sessionRows ? `<table>
        <thead><tr><th>Date</th><th>Time</th><th>Score</th><th>Level</th><th></th></tr></thead>
        <tbody>${sessionRows}</tbody>
      </table>` : '<div class="empty">No sessions yet — get learning!</div>'}
    </div>
  </main>
  <footer>Edugotchi v1.0 &nbsp;·&nbsp; Refresh to update</footer>
</body>
</html>`);
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT}`);
  console.log(`Parent portal: http://localhost:${PORT}/portal`);
});
