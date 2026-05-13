const express = require('express');
const OpenAI  = require('openai');
const fs      = require('fs');
const path    = require('path');

const { SUBJECT_CATEGORIES, ALL_CATEGORIES, getActiveSubjects, pickSubject } = require('./subjects');
const { DIFFICULTIES, loadData, saveData, calculateStreak } = require('./data');

const router = express.Router();
const openai  = new OpenAI({ apiKey: process.env.OPENAI_API_KEY });

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

router.get('/question', async (req, res) => {
  const data           = loadData();
  const difficulty     = data.difficulty;
  const activeSubjects = getActiveSubjects(data.enabledCategories);
  const subject        = pickSubject(data.subjects, activeSubjects);
  const gradeHint      = difficulty === 'easy' ? 'grades 1-2' : difficulty === 'medium' ? 'grades 3-4' : 'grades 4-5';

  try {
    const completion = await openai.chat.completions.create({
      model: 'gpt-4o-mini',
      messages: [
        { role: 'system', content: SYSTEM_PROMPT },
        { role: 'user',   content: `Generate a ${difficulty} difficulty (${gradeHint}) quiz question about: ${subject}.` },
      ],
      temperature: 0.4,
    });

    const raw      = completion.choices[0].message.content.trim();
    const question = JSON.parse(raw);
    question.subject    = subject;
    question.difficulty = difficulty;
    res.json(question);
  } catch (err) {
    console.error('OpenAI error:', err.message);
    res.status(500).json({ error: 'failed to generate question', detail: err.message });
  }
});

router.get('/report/correct', (req, res) => {
  const subject = req.query.subject || 'unknown';
  const data    = loadData();
  if (!data.subjects[subject]) data.subjects[subject] = { correct: 0, attempts: 0 };
  data.subjects[subject].correct++;
  data.subjects[subject].attempts++;
  saveData(data);
  res.send('ok');
});

router.get('/report/wakeup', (req, res) => {
  const correct = parseInt(req.query.correct) || 0;
  const wrong   = parseInt(req.query.wrong)   || 0;
  const data    = loadData();
  const now     = new Date();

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

router.get('/stats', (req, res) => {
  const data   = loadData();
  const streak = calculateStreak(data.wakeups);
  res.json({ streak, difficulty: data.difficulty });
});

router.post('/settings', (req, res) => {
  const { enabledCategories } = req.body;
  if (!Array.isArray(enabledCategories) || enabledCategories.length === 0)
    return res.status(400).json({ error: 'at least one category must be enabled' });
  const valid = enabledCategories.filter(c => SUBJECT_CATEGORIES[c]);
  if (valid.length === 0)
    return res.status(400).json({ error: 'no valid categories provided' });
  const data = loadData();
  data.enabledCategories = valid;
  saveData(data);
  res.json({ ok: true });
});

router.get('/portal', (req, res) => {
  const data          = loadData();
  const streak        = calculateStreak(data.wakeups);
  const recent        = [...data.wakeups].reverse().slice(0, 10);
  const totalSessions = data.wakeups.length;

  const totalCorrect = data.wakeups.reduce((s, w) => s + w.correct, 0);
  const totalWrong   = data.wakeups.reduce((s, w) => s + w.wrong,   0);
  const accuracy     = (totalCorrect + totalWrong) > 0
    ? Math.round((totalCorrect / (totalCorrect + totalWrong)) * 100) : 0;

  const bestSubject = Object.entries(data.subjects)
    .filter(([, s]) => s.attempts >= 2)
    .sort((a, b) => (b[1].correct / b[1].attempts) - (a[1].correct / a[1].attempts))[0];

  const statusMsg = accuracy >= 85 && streak >= 5
    ? 'Your little one is on fire! Keep it up!'
    : accuracy >= 70 || streak >= 3
    ? 'Great progress — things are clicking!'
    : totalSessions === 0
    ? 'No sessions yet. Time to get learning!'
    : 'Still warming up — every session counts!';

  const diffColor = { easy: '#10b981', medium: '#f59e0b', hard: '#ef4444' };

  const subjectRows = Object.entries(data.subjects)
    .sort((a, b) => (b[1].correct / b[1].attempts) - (a[1].correct / a[1].attempts))
    .map(([name, s]) => {
      const pct   = s.attempts ? Math.round((s.correct / s.attempts) * 100) : 0;
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
      <td>${w.difficulty ? `<span class="diff-pill" style="background:${dc}20;color:${dc};border:1px solid ${dc}40">${w.difficulty}</span>` : '&mdash;'}</td>
      <td>${perfect ? '<span class="perfect-pill">Perfect!</span>' : ''}</td>
    </tr>`;
  }).join('');

  const enabledSet      = new Set(data.enabledCategories || ALL_CATEGORIES);
  const categoryToggles = ALL_CATEGORIES.map(cat => {
    const isOn  = enabledSet.has(cat);
    const count = SUBJECT_CATEGORIES[cat].length;
    return `<label class="cat-item ${isOn ? 'cat-on' : 'cat-off'}">
      <input type="checkbox" class="cat-check" value="${cat}" ${isOn ? 'checked' : ''} onchange="saveCategories()">
      <div class="cat-info">
        <span class="cat-name">${cat}</span>
        <span class="cat-sub">${count} subject${count !== 1 ? 's' : ''}</span>
      </div>
      <div class="tog-track"><div class="tog-thumb"></div></div>
    </label>`;
  }).join('');

  const subjectTable = subjectRows
    ? `<table><thead><tr><th>Subject</th><th>Progress</th><th>Accuracy</th><th>Score</th></tr></thead><tbody>${subjectRows}</tbody></table>`
    : '<div class="empty">No questions answered yet — start a session!</div>';

  const sessionTable = sessionRows
    ? `<table><thead><tr><th>Date</th><th>Time</th><th>Score</th><th>Level</th><th></th></tr></thead><tbody>${sessionRows}</tbody></table>`
    : '<div class="empty">No sessions yet — get learning!</div>';

  const template = fs.readFileSync(path.join(__dirname, 'public', 'portal.html'), 'utf8');
  const html = template
    .replace('{{STATUS_MSG}}',        statusMsg)
    .replace('{{STREAK}}',            streak)
    .replace('{{TOTAL_SESSIONS}}',    totalSessions)
    .replace('{{ACCURACY}}',          accuracy)
    .replace('{{DIFF_COLOR}}',        diffColor[data.difficulty])
    .replace('{{DIFFICULTY}}',        data.difficulty.charAt(0).toUpperCase() + data.difficulty.slice(1))
    .replace('{{BEST_SUBJECT}}',      bestSubject ? bestSubject[0].charAt(0).toUpperCase() + bestSubject[0].slice(1) : 'N/A')
    .replace('{{CATEGORY_TOGGLES}}',  categoryToggles)
    .replace('{{RECENT_COUNT}}',      Math.min(recent.length, 10))
    .replace('{{SUBJECT_TABLE}}',     subjectTable)
    .replace('{{SESSION_TABLE}}',     sessionTable);

  res.send(html);
});

module.exports = router;
