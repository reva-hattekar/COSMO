#include "AkinatorApp.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

#define COL_BG       0x0000
#define COL_WHITE    0xFFFF
#define COL_CYAN     0x07FF
#define COL_BLUE     0x001F
#define COL_PURPLE   0x801F
#define COL_GREY     0x8410
#define COL_YELLOW   0xFFE0
#define COL_GREEN    0x07E0
#define COL_RED      0xF800
#define COL_DARKGREY 0x2104
#define COL_PILL     0x0842

static const char* ANSWER_LABELS[5] = {
  "YES",
  "NO",
  "DON'T KNOW",
  "PROBABLY",
  "PROBABLY NOT"
};

static const uint16_t ANSWER_COLORS[5] = {
  COL_GREEN,
  COL_RED,
  COL_YELLOW,
  COL_CYAN,
  COL_PURPLE
};

static String extractJsonString(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":\"";
  int idx = json.indexOf(searchKey);
  if (idx == -1) {
    searchKey = "\"" + key + "\": \"";
    idx = json.indexOf(searchKey);
  }
  if (idx == -1) return "";
  idx += searchKey.length();
  int endIdx = json.indexOf("\"", idx);
  if (endIdx == -1) return "";
  return json.substring(idx, endIdx);
}

static bool extractJsonBool(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":";
  int idx = json.indexOf(searchKey);
  if (idx == -1) return false;
  idx += searchKey.length();
  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '\t')) idx++;
  return (json.startsWith("true", idx));
}

static int extractJsonInt(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":";
  int idx = json.indexOf(searchKey);
  if (idx == -1) return 0;
  idx += searchKey.length();
  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '\t')) idx++;
  return json.substring(idx).toInt();
}

static float extractJsonFloat(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":";
  int idx = json.indexOf(searchKey);
  if (idx == -1) return 0.0f;
  idx += searchKey.length();
  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '\t')) idx++;
  return json.substring(idx).toFloat();
}

AkinatorApp::AkinatorApp(Adafruit_GC9A01A& display)
  : tft(display),
    currentStep(0),
    progressPct(0.0f),
    selectedAnswer(0),
    state(STATE_IDLE) {}

void AkinatorApp::init(const char* serverUrl) {
  serverBaseUrl = serverUrl;
  reset();
  startGame();
}

void AkinatorApp::reset() {
  sessionId = "";
  currentQuestion = "";
  guessName = "";
  guessDesc = "";
  currentStep = 0;
  progressPct = 0.0f;
  selectedAnswer = 0;
  state = STATE_IDLE;
  errorMessage = "";
}

void AkinatorApp::drawCenteredWrapped(const char* text, int startY, int maxLines, int lineSpacing, const GFXfont* font, uint16_t color) {
  tft.setFont(font);
  tft.setTextColor(color);

  String remaining = text;
  String line = "";
  int y = startY;
  int linesCount = 0;

  while (remaining.length() > 0 && linesCount < maxLines) {
    int split = remaining.indexOf(' ');
    String word;
    if (split == -1) {
      word = remaining;
      remaining = "";
    } else {
      word = remaining.substring(0, split);
      remaining = remaining.substring(split + 1);
    }

    String testLine = (line.length() == 0) ? word : line + " " + word;
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(testLine.c_str(), 0, 0, &x1, &y1, &w, &h);

    if (w > 180 && line.length() > 0) {
      tft.getTextBounds(line.c_str(), 0, y, &x1, &y1, &w, &h);
      tft.setCursor((240 - w) / 2, y);
      tft.print(line);
      y += lineSpacing;
      linesCount++;
      line = word;
    } else {
      line = testLine;
    }
  }

  if (line.length() > 0 && linesCount < maxLines) {
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(line.c_str(), 0, y, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, y);
    tft.print(line);
  }
}

void AkinatorApp::drawQuestionScreen() {
  tft.fillScreen(COL_BG);
  tft.drawCircle(120, 120, 114, COL_DARKGREY);

  // Top header pill: Q# and Progress
  tft.fillRoundRect(50, 18, 140, 24, 6, COL_PILL);
  tft.drawRoundRect(50, 18, 140, 24, 6, COL_DARKGREY);

  char hdr[32];
  snprintf(hdr, sizeof(hdr), "Q%d (%d%%)", currentStep + 1, (int)progressPct);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_CYAN);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(hdr, 0, 35, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 35);
  tft.print(hdr);

  // Question text (Word wrapped)
  drawCenteredWrapped(currentQuestion.c_str(), 72, 4, 20, &FreeSans9pt7b, COL_WHITE);

  // Answer pill container
  tft.fillRoundRect(30, 150, 180, 42, 8, COL_PILL);
  tft.drawRoundRect(30, 150, 180, 42, 8, ANSWER_COLORS[selectedAnswer]);

  // Left and Right arrows
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_GREY);
  tft.setCursor(38, 177);
  tft.print("<");
  tft.setCursor(194, 177);
  tft.print(">");

  // Active answer text
  const char* ans = ANSWER_LABELS[selectedAnswer];
  tft.setTextColor(ANSWER_COLORS[selectedAnswer]);
  tft.getTextBounds(ans, 0, 177, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 177);
  tft.print(ans);

  // Bottom action hint
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_GREY);
  tft.setCursor(76, 204);
  tft.print("SELECT: CONFIRM");
}

void AkinatorApp::drawGuessScreen() {
  tft.fillScreen(COL_BG);
  tft.drawCircle(120, 120, 114, COL_PURPLE);

  tft.fillRoundRect(50, 20, 140, 24, 6, COL_PILL);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_YELLOW);
  int16_t x1, y1;
  uint16_t w, h;
  const char* title = "I THINK OF...";
  tft.getTextBounds(title, 0, 37, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 37);
  tft.print(title);

  // Guess Character Name (Large bold)
  drawCenteredWrapped(guessName.c_str(), 75, 2, 22, &FreeSansBold12pt7b, COL_WHITE);

  // Description underneath
  if (guessDesc.length() > 0) {
    drawCenteredWrapped(guessDesc.c_str(), 125, 2, 18, &FreeSans9pt7b, COL_CYAN);
  }

  // Question: Is this correct?
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_GREY);
  tft.setCursor(68, 165);
  tft.print("IS THIS CORRECT?");

  // Button assignments
  tft.setCursor(44, 185);
  tft.setTextColor(COL_RED);
  tft.print("LEFT: NO");

  tft.setCursor(146, 185);
  tft.setTextColor(COL_GREEN);
  tft.print("RIGHT: YES");
}

void AkinatorApp::drawStatusScreen(const char* title, const char* msg, uint16_t color, const char* actionMsg) {
  tft.fillScreen(COL_BG);
  tft.drawCircle(120, 120, 114, color);

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(title, 0, 95, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 95);
  tft.print(title);

  if (msg && strlen(msg) > 0) {
    drawCenteredWrapped(msg, 125, 3, 19, &FreeSans9pt7b, COL_WHITE);
  }

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_GREY);
  if (actionMsg) {
    tft.setCursor(65, 185);
    tft.print(actionMsg);
  } else {
    tft.setCursor(65, 185);
    tft.print("SELECT: CONTINUE");
  }
}

void AkinatorApp::render() {
  switch (state) {
    case STATE_LOADING:
      drawStatusScreen("AKINATOR", "CONNECTING TO ENGINE...", COL_CYAN, "PLEASE WAIT...");
      break;
    case STATE_QUESTION:
      drawQuestionScreen();
      break;
    case STATE_GUESS:
      drawGuessScreen();
      break;
    case STATE_WON:
      drawStatusScreen("I WIN!", "GREAT GAME! I GUESSED IT!", COL_GREEN, "SELECT: PLAY AGAIN");
      break;
    case STATE_LOST:
      drawStatusScreen("YOU WIN!", "BRAVO! YOU DEFEATED ME!", COL_YELLOW, "SELECT: PLAY AGAIN");
      break;
    case STATE_ERROR:
      drawStatusScreen("ERROR", errorMessage.c_str(), COL_RED, "SELECT: RETRY");
      break;
    default:
      break;
  }
}

void AkinatorApp::parseGameResponse(const String& json) {
  String sid = extractJsonString(json, "sessionId");
  if (sid.length() > 0) sessionId = sid;

  bool won = extractJsonBool(json, "won");
  bool lost = extractJsonBool(json, "lost");
  String status = extractJsonString(json, "status");

  if (won || status == "guess") {
    state = STATE_GUESS;
    int guessIdx = json.indexOf("\"guess\":");
    if (guessIdx != -1) {
      String sub = json.substring(guessIdx);
      guessName = extractJsonString(sub, "name");
      guessDesc = extractJsonString(sub, "description");
    }
    if (guessName.length() == 0) guessName = "Your Character";
    render();
    return;
  }

  if (lost || status == "lost") {
    state = STATE_LOST;
    render();
    return;
  }

  String q = extractJsonString(json, "question");
  if (q.length() > 0) {
    currentQuestion = q;
    currentStep = extractJsonInt(json, "step");
    progressPct = extractJsonFloat(json, "progress");
    selectedAnswer = 0;
    state = STATE_QUESTION;
    render();
    return;
  }

  // Fallback
  state = STATE_ERROR;
  errorMessage = "INVALID RESPONSE";
  render();
}

void AkinatorApp::startGame() {
  if (WiFi.status() != WL_CONNECTED) {
    state = STATE_ERROR;
    errorMessage = "WIFI NOT CONNECTED";
    render();
    return;
  }

  state = STATE_LOADING;
  render();

  WiFiClient client;
  HTTPClient http;
  String url = serverBaseUrl + "/api/game/start";

  if (!http.begin(client, url)) {
    state = STATE_ERROR;
    errorMessage = "HTTP INIT FAILED";
    render();
    return;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{}");

  if (code == 200 || code == 201) {
    String payload = http.getString();
    http.end();
    parseGameResponse(payload);
  } else {
    http.end();
    state = STATE_ERROR;
    char buf[32];
    snprintf(buf, sizeof(buf), "SERVER CODE %d", code);
    errorMessage = buf;
    render();
  }
}

void AkinatorApp::sendAnswer(int answerIdx) {
  state = STATE_LOADING;
  render();

  WiFiClient client;
  HTTPClient http;
  String url = serverBaseUrl + "/api/game/" + sessionId + "/answer";

  if (!http.begin(client, url)) {
    state = STATE_ERROR;
    errorMessage = "HTTP INIT FAILED";
    render();
    return;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  char body[32];
  snprintf(body, sizeof(body), "{\"answerIndex\":%d}", answerIdx);
  int code = http.POST(body);

  if (code == 200) {
    String payload = http.getString();
    http.end();
    parseGameResponse(payload);
  } else {
    http.end();
    state = STATE_ERROR;
    char buf[32];
    snprintf(buf, sizeof(buf), "HTTP ERROR %d", code);
    errorMessage = buf;
    render();
  }
}

void AkinatorApp::sendContinue() {
  state = STATE_LOADING;
  render();

  WiFiClient client;
  HTTPClient http;
  String url = serverBaseUrl + "/api/game/" + sessionId + "/continue";

  if (!http.begin(client, url)) {
    state = STATE_ERROR;
    errorMessage = "HTTP INIT FAILED";
    render();
    return;
  }

  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{}");

  if (code == 200) {
    String payload = http.getString();
    http.end();
    parseGameResponse(payload);
  } else {
    http.end();
    state = STATE_ERROR;
    errorMessage = "CONTINUE FAILED";
    render();
  }
}

void AkinatorApp::sendWin() {
  WiFiClient client;
  HTTPClient http;
  String url = serverBaseUrl + "/api/game/" + sessionId + "/win";
  if (http.begin(client, url)) {
    http.setTimeout(6000);
    http.POST("{}");
    http.end();
  }
  state = STATE_WON;
  render();
}

void AkinatorApp::update(bool left, bool right, bool select) {
  if (state == STATE_QUESTION) {
    if (left) {
      selectedAnswer = (selectedAnswer - 1 + 5) % 5;
      drawQuestionScreen();
    } else if (right) {
      selectedAnswer = (selectedAnswer + 1) % 5;
      drawQuestionScreen();
    } else if (select) {
      sendAnswer(selectedAnswer);
    }
    return;
  }

  if (state == STATE_GUESS) {
    if (left) {
      // Left = No, continue playing
      sendContinue();
    } else if (right) {
      // Right = Yes, Akinator was right
      sendWin();
    } else if (select) {
      // Select = Restart
      init(serverBaseUrl.c_str());
    }
    return;
  }

  if (state == STATE_WON || state == STATE_LOST || state == STATE_ERROR) {
    if (select) {
      init(serverBaseUrl.c_str());
    }
    return;
  }
}

