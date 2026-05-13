#pragma once

const Question FALLBACK_Q[] PROGMEM = {
  { "What is 7 x 8?",      {"54","56","58","64"},                   "multiplication and division", 1 },
  { "Capital of France?",  {"Berlin","Madrid","Paris","Rome"},       "world capitals",             2 },
  { "sqrt(144) = ?",       {"10","11","12","13"},                    "addition and subtraction",   2 },
  { "2 to the power 10?",  {"512","1024","2048","256"},              "multiplication and division", 1 },
  { "Largest planet?",     {"Earth","Saturn","Jupiter","Neptune"},   "the solar system and planets",2 },
};
const int NUM_FALLBACK = sizeof(FALLBACK_Q) / sizeof(FALLBACK_Q[0]);

void loadFallbackQuestion() {
  int idx = questionNumber % NUM_FALLBACK;
  memcpy_P(&currentQ, &FALLBACK_Q[idx], sizeof(Question));
}

bool fetchQuestion() {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("fetch: no WiFi"); return false; }
  HTTPClient http;
  String url = QUESTION_URL + String("?difficulty=") + difficulty;
  Serial.println("fetch: " + url);
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();
  Serial.println("fetch: HTTP " + String(code));
  if (code != 200) { http.end(); return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) return false;

  strlcpy(currentQ.text,    doc["question"] | "",  sizeof(currentQ.text));
  strlcpy(currentQ.subject, doc["subject"]  | "",  sizeof(currentQ.subject));
  for (int i = 0; i < 4; i++)
    strlcpy(currentQ.opts[i], doc["options"][i] | "", sizeof(currentQ.opts[i]));
  currentQ.correct = doc["correct_index"] | 0;
  return true;
}

void loadQuestion() {
  if (!fetchQuestion()) loadFallbackQuestion();
}
