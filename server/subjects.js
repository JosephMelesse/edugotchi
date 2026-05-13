const SUBJECT_CATEGORIES = {
  'Mathematics': [
    'addition and subtraction',
    'multiplication and division',
    'fractions and decimals',
    'basic geometry shapes',
    'telling time and calendars',
  ],
  'Geography': [
    'US geography and states',
    'world capitals',
  ],
  'Science & Nature': [
    'animals and habitats',
    'the solar system and planets',
    'human body and health',
    'plant life cycles',
    'weather and seasons',
    'famous scientists and inventors',
  ],
  'History & Social Studies': [
    'US history and presidents',
    'world history landmarks',
    'basic economics and money',
    'community helpers and jobs',
  ],
  'English & Language Arts': [
    'grammar and parts of speech',
    'vocabulary and word meanings',
  ],
  'Arts & Culture': [
    'colors, art, and music basics',
  ],
};

const SUBJECTS      = Object.values(SUBJECT_CATEGORIES).flat();
const ALL_CATEGORIES = Object.keys(SUBJECT_CATEGORIES);

function getActiveSubjects(enabledCategories) {
  return (enabledCategories || ALL_CATEGORIES).flatMap(cat => SUBJECT_CATEGORIES[cat] || []);
}

function pickSubject(subjectStats, activeSubjects) {
  if (!activeSubjects || activeSubjects.length === 0) activeSubjects = SUBJECTS;
  const weights = activeSubjects.map(name => {
    const s = subjectStats[name];
    if (!s || s.attempts < 3) return 1.5;
    const accuracy = s.correct / s.attempts;
    return Math.max(0.2, 2 - accuracy * 2);
  });
  const total = weights.reduce((a, b) => a + b, 0);
  let r = Math.random() * total;
  for (let i = 0; i < activeSubjects.length; i++) {
    r -= weights[i];
    if (r <= 0) return activeSubjects[i];
  }
  return activeSubjects[activeSubjects.length - 1];
}

module.exports = { SUBJECT_CATEGORIES, SUBJECTS, ALL_CATEGORIES, getActiveSubjects, pickSubject };
