const fs   = require('fs');
const path = require('path');
const { ALL_CATEGORIES } = require('./subjects');

const DATA_FILE  = path.join(__dirname, 'data.json');
const DIFFICULTIES = ['easy', 'medium', 'hard'];

function loadData() {
  if (!fs.existsSync(DATA_FILE))
    return { wakeups: [], subjects: {}, difficulty: 'easy', enabledCategories: ALL_CATEGORIES };
  try {
    const d = JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
    if (!d.difficulty) d.difficulty = 'easy';
    if (!d.enabledCategories || d.enabledCategories.length === 0)
      d.enabledCategories = ALL_CATEGORIES;
    return d;
  } catch {
    return { wakeups: [], subjects: {}, difficulty: 'easy', enabledCategories: ALL_CATEGORIES };
  }
}

function saveData(data) {
  fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
}

function today() {
  return new Date().toISOString().split('T')[0];
}

function calculateStreak(wakeups) {
  if (!wakeups.length) return 0;
  const dates     = [...new Set(wakeups.map(w => w.date))].sort().reverse();
  const todayStr  = today();
  const yesterday = new Date(Date.now() - 86400000).toISOString().split('T')[0];
  if (dates[0] !== todayStr && dates[0] !== yesterday) return 0;
  let streak   = 0;
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

module.exports = { DATA_FILE, DIFFICULTIES, loadData, saveData, today, calculateStreak };
