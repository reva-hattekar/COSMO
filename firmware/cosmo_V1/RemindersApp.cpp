#include "RemindersApp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>

static const char* REMINDERS_CACHE_FILE = "/reminders.json";

// Colors (RGB565)
#define REM_BG       0x0000
#define REM_WHITE    0xFFFF
#define REM_CYAN     0x07FF
#define REM_YELLOW   0xFFE0
#define REM_GREY     0x632C
#define REM_DARKGREY 0x2124
#define REM_CARD_BG  0x10A2
#define REM_CARD_BRD 0x2965
#define REM_GREEN    0x07E0
#define REM_RED      0xF800
#define REM_ORANGE   0xFD20
#define REM_BLUE     0x001F

RemindersApp::RemindersApp(Adafruit_GC9A01A& display)
  : tft(display),
    itemCount(0),
    currentIndex(0),
    syncState(REMINDERS_SYNC_NEVER),
    viewMode(REMINDERS_VIEW_CARD),
    lastSyncAttempt(0),
    lastSyncTimestamp(0),
    exitRequested(false),
    initialized(false),
    needsRedraw(true) {
  memset(items, 0, sizeof(items));
  snprintf(statusMessage, sizeof(statusMessage), "Initializing...");
  strncpy(backendUrl, "http://192.168.1.100:3000/api/google/reminders", sizeof(backendUrl) - 1);
  backendUrl[sizeof(backendUrl) - 1] = '\0';
}

void RemindersApp::setBackendUrl(const char* url) {
  if (url && strlen(url) > 0) {
    strncpy(backendUrl, url, sizeof(backendUrl) - 1);
    backendUrl[sizeof(backendUrl) - 1] = '\0';
  }
}

void RemindersApp::init() {
  exitRequested = false;
  viewMode = REMINDERS_VIEW_CARD;
  currentIndex = 0;
  initialized = true;

  // Always load from cache first for instant offline readiness
  loadCachedFeed();

  // If Wi-Fi is active and never synced or cache older than 10 mins, attempt fresh sync
  if (WiFi.status() == WL_CONNECTED && (syncState == REMINDERS_SYNC_NEVER || millis() - lastSyncAttempt >= 600000)) {
    syncFeed();
  }

  render();
}

bool RemindersApp::isActive() const {
  return initialized && !exitRequested;
}

bool RemindersApp::wantsToExit() const {
  return exitRequested;
}

void RemindersApp::resetExit() {
  exitRequested = false;
}

void RemindersApp::maintain(bool wifiConnected) {
  unsigned long now = millis();
  // 10 minute background sync interval
  if (wifiConnected && (lastSyncAttempt == 0 || now - lastSyncAttempt >= 600000)) {
    syncFeed();
  }
}

void RemindersApp::update(bool left, bool right, bool select) {
  if (left) {
    if (itemCount > 1) {
      currentIndex = (currentIndex - 1 + itemCount) % itemCount;
      render();
    }
  } else if (right) {
    if (itemCount > 1) {
      currentIndex = (currentIndex + 1) % itemCount;
      render();
    }
  } else if (select) {
    if (itemCount > 0) {
      viewMode = (viewMode == REMINDERS_VIEW_CARD ? REMINDERS_VIEW_DETAIL : REMINDERS_VIEW_CARD);
      render();
    } else if (syncState == REMINDERS_SYNC_FAILED || syncState == REMINDERS_SYNC_NEVER) {
      // Retry sync on select if empty or failed
      if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(REM_BG);
        centerString("SYNCING...", 120, &FreeSansBold9pt7b, REM_YELLOW);
        syncFeed(true);
        render();
      }
    }
  }
}

bool RemindersApp::syncFeed(bool force) {
  if (WiFi.status() != WL_CONNECTED) {
    if (syncState == REMINDERS_SYNC_NEVER) {
      loadCachedFeed();
    }
    return false;
  }

  lastSyncAttempt = millis();
  Serial.printf("[REMINDERS] Connecting to backend: %s\n", backendUrl);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(6000);
  http.setUserAgent("COSMO-ESP32-Reminders");
  http.setReuse(false);

  if (!http.begin(backendUrl)) {
    Serial.println("[REMINDERS] ERROR: HTTPClient.begin failed!");
    snprintf(statusMessage, sizeof(statusMessage), "Connection failed");
    if (itemCount == 0) loadCachedFeed();
    syncState = REMINDERS_SYNC_FAILED;
    return false;
  }

  int httpCode = http.GET();
  Serial.printf("[REMINDERS] HTTP status code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    snprintf(statusMessage, sizeof(statusMessage), "HTTP Error: %d", httpCode);
    http.end();
    if (itemCount == 0) loadCachedFeed();
    syncState = REMINDERS_SYNC_FAILED;
    return false;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() < 10) {
    snprintf(statusMessage, sizeof(statusMessage), "Empty payload");
    if (itemCount == 0) loadCachedFeed();
    syncState = REMINDERS_SYNC_FAILED;
    return false;
  }

  // Parse and cache
  parseRemindersJson(payload);
  saveCachedFeed(payload);

  syncState = REMINDERS_SYNC_OK_LIVE;
  lastSyncTimestamp = time(nullptr);
  snprintf(statusMessage, sizeof(statusMessage), "Live Sync OK");

  return true;
}

bool RemindersApp::loadCachedFeed() {
  if (!LittleFS.exists(REMINDERS_CACHE_FILE)) {
    snprintf(statusMessage, sizeof(statusMessage), "No local cache");
    return false;
  }

  File f = LittleFS.open(REMINDERS_CACHE_FILE, FILE_READ);
  if (!f) {
    snprintf(statusMessage, sizeof(statusMessage), "Cache open failed");
    return false;
  }

  String payload = f.readString();
  f.close();

  if (payload.length() < 10) {
    return false;
  }

  parseRemindersJson(payload);
  syncState = REMINDERS_SYNC_OK_CACHED;
  snprintf(statusMessage, sizeof(statusMessage), "Loaded from cache");
  return true;
}

bool RemindersApp::saveCachedFeed(const String& json) {
  File f = LittleFS.open(REMINDERS_CACHE_FILE, FILE_WRITE);
  if (!f) return false;
  f.print(json);
  f.close();
  return true;
}

void RemindersApp::extractJsonField(const String& json, const String& key, char* dest, size_t destSize) {
  dest[0] = '\0';
  String searchKey = "\"" + key + "\":\"";
  int startIdx = json.indexOf(searchKey);
  if (startIdx == -1) {
    // Try without quotes around value (e.g. boolean / number)
    searchKey = "\"" + key + "\":";
    startIdx = json.indexOf(searchKey);
    if (startIdx == -1) return;
    int valStart = startIdx + searchKey.length();
    while (valStart < json.length() && (json[valStart] == ' ' || json[valStart] == '"')) valStart++;
    int valEnd = valStart;
    while (valEnd < json.length() && json[valEnd] != ',' && json[valEnd] != '}' && json[valEnd] != '"') valEnd++;
    String val = json.substring(valStart, valEnd);
    val.trim();
    strncpy(dest, val.c_str(), destSize - 1);
    dest[destSize - 1] = '\0';
    return;
  }

  int valStart = startIdx + searchKey.length();
  int valEnd = json.indexOf('"', valStart);
  if (valEnd == -1) return;

  String val = json.substring(valStart, valEnd);
  // Unescape basic sequences
  val.replace("\\\"", "\"");
  val.replace("\\n", " ");
  val.trim();

  strncpy(dest, val.c_str(), destSize - 1);
  dest[destSize - 1] = '\0';
}

void RemindersApp::parseRemindersJson(const String& json) {
  itemCount = 0;
  currentIndex = 0;

  // Check status
  char statusVal[32] = {0};
  extractJsonField(json, "status", statusVal, sizeof(statusVal));

  if (strcmp(statusVal, "unauthenticated") == 0) {
    syncState = REMINDERS_SYNC_UNAUTH;
    snprintf(statusMessage, sizeof(statusMessage), "Google account not linked");
    return;
  }

  // Parse Calendar Events array
  int evArrStart = json.indexOf("\"events\":[");
  if (evArrStart != -1) {
    int evArrEnd = json.indexOf("]", evArrStart);
    if (evArrEnd != -1) {
      String evStr = json.substring(evArrStart + 9, evArrEnd);
      int pos = 0;
      while (pos < evStr.length() && itemCount < MAX_REMINDERS) {
        int objStart = evStr.indexOf('{', pos);
        if (objStart == -1) break;
        int objEnd = evStr.indexOf('}', objStart);
        if (objEnd == -1) break;

        String itemObj = evStr.substring(objStart, objEnd + 1);
        ReminderEntry& entry = items[itemCount];
        entry.kind = REMINDER_KIND_EVENT;

        extractJsonField(itemObj, "title", entry.title, sizeof(entry.title));
        extractJsonField(itemObj, "time", entry.timeOrDue, sizeof(entry.timeOrDue));
        snprintf(entry.listOrCal, sizeof(entry.listOrCal), "Calendar");

        entry.completed = false;
        entry.overdue = false;
        entry.allDay = itemObj.indexOf("\"allDay\":true") != -1;

        if (strlen(entry.title) > 0) {
          itemCount++;
        }
        pos = objEnd + 1;
      }
    }
  }

  // Parse Tasks array
  int tskArrStart = json.indexOf("\"tasks\":[");
  if (tskArrStart != -1) {
    int tskArrEnd = json.indexOf("]", tskArrStart);
    if (tskArrEnd != -1) {
      String tskStr = json.substring(tskArrStart + 8, tskArrEnd);
      int pos = 0;
      while (pos < tskStr.length() && itemCount < MAX_REMINDERS) {
        int objStart = tskStr.indexOf('{', pos);
        if (objStart == -1) break;
        int objEnd = tskStr.indexOf('}', objStart);
        if (objEnd == -1) break;

        String itemObj = tskStr.substring(objStart, objEnd + 1);
        ReminderEntry& entry = items[itemCount];
        entry.kind = REMINDER_KIND_TASK;

        extractJsonField(itemObj, "title", entry.title, sizeof(entry.title));
        extractJsonField(itemObj, "dueFormatted", entry.timeOrDue, sizeof(entry.timeOrDue));
        extractJsonField(itemObj, "listTitle", entry.listOrCal, sizeof(entry.listOrCal));

        entry.completed = itemObj.indexOf("\"completed\":true") != -1;
        entry.overdue = itemObj.indexOf("\"overdue\":true") != -1;
        entry.allDay = false;

        if (strlen(entry.title) > 0) {
          itemCount++;
        }
        pos = objEnd + 1;
      }
    }
  }

  Serial.printf("[REMINDERS] Parsed %d reminder items (%s)\n", itemCount, statusVal);
}

void RemindersApp::centerString(const char* text, int y, const GFXfont* font, uint16_t color) {
  tft.setFont(font);
  tft.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, y);
  tft.print(text);
}

void RemindersApp::drawBadge(int x, int y, int w, int h, const char* label, uint16_t bgColor, uint16_t textColor) {
  tft.fillRoundRect(x, y, w, h, 4, bgColor);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(textColor);
  int textW = strlen(label) * 6;
  tft.setCursor(x + (w - textW) / 2, y + (h - 8) / 2);
  tft.print(label);
}

void RemindersApp::render() {
  tft.fillScreen(REM_BG);

  // Outer Circular Bezel Accent (GC9A01 safe perimeter)
  tft.drawCircle(120, 120, 118, REM_CARD_BRD);
  tft.drawCircle(120, 120, 117, REM_DARKGREY);

  // Top Header: Date and Sync status indicator
  struct tm timeinfo;
  char dateStr[24] = "TODAY";
  if (getLocalTime(&timeinfo, 20)) {
    strftime(dateStr, sizeof(dateStr), "%a, %b %d", &timeinfo);
    for (int i = 0; dateStr[i]; i++) dateStr[i] = toupper((unsigned char)dateStr[i]);
  }
  centerString(dateStr, 28, &FreeSansBold9pt7b, REM_WHITE);

  // Status indicator dot
  uint16_t statusDotColor = REM_GREY;
  const char* statusTxt = "OFFLINE";
  if (syncState == REMINDERS_SYNC_OK_LIVE) {
    statusDotColor = REM_GREEN;
    statusTxt = "LIVE";
  } else if (syncState == REMINDERS_SYNC_OK_CACHED) {
    statusDotColor = REM_ORANGE;
    statusTxt = "CACHED";
  } else if (syncState == REMINDERS_SYNC_UNAUTH) {
    statusDotColor = REM_RED;
    statusTxt = "UNLINKED";
  } else if (syncState == REMINDERS_SYNC_FAILED) {
    statusDotColor = REM_RED;
    statusTxt = "FAIL";
  }

  // Draw status dot & text at top right of header
  tft.fillCircle(196, 24, 3, statusDotColor);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(statusDotColor);
  tft.setCursor(182, 32);
  tft.print(statusTxt);

  // Branch rendering
  if (syncState == REMINDERS_SYNC_UNAUTH) {
    renderUnauthState();
  } else if (itemCount == 0) {
    renderEmptyState();
  } else if (viewMode == REMINDERS_VIEW_CARD) {
    renderCardView();
  } else {
    renderDetailView();
  }

  // Bottom Navigation Footer (Safe area Y = 205-226)
  if (itemCount > 1) {
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(REM_CYAN);
    tft.setCursor(44, 212);
    tft.print("< PREV");
    tft.setCursor(154, 212);
    tft.print("NEXT >");
  }

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(REM_GREY);
  tft.setCursor(92, 224);
  tft.print("L+R: EXIT");
}

void RemindersApp::renderCardView() {
  if (currentIndex < 0 || currentIndex >= itemCount) currentIndex = 0;
  ReminderEntry& item = items[currentIndex];

  // Main Card Container (safe circular region)
  tft.fillRoundRect(24, 46, 192, 142, 10, REM_CARD_BG);
  tft.drawRoundRect(24, 46, 192, 142, 10, REM_CARD_BRD);

  // Header row inside card: Type badge + Index pill
  if (item.kind == REMINDER_KIND_EVENT) {
    drawBadge(34, 54, 70, 16, "CALENDAR", REM_CYAN, REM_BG);
  } else {
    drawBadge(34, 54, 50, 16, "TASK", REM_YELLOW, REM_BG);
  }

  // Counter (e.g. "1 / 4")
  char counterBuf[16];
  snprintf(counterBuf, sizeof(counterBuf), "%d / %d", currentIndex + 1, itemCount);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(REM_GREY);
  tft.setCursor(174, 58);
  tft.print(counterBuf);

  // Time / Due Date row
  tft.setFont(&FreeSansBold9pt7b);
  if (item.kind == REMINDER_KIND_EVENT) {
    tft.setTextColor(REM_CYAN);
    tft.setCursor(34, 88);
    tft.print(item.timeOrDue[0] ? item.timeOrDue : "ALL DAY");
  } else {
    tft.setTextColor(item.overdue ? REM_RED : REM_YELLOW);
    tft.setCursor(34, 88);
    if (item.completed) {
      tft.setTextColor(REM_GREEN);
      tft.print("COMPLETED");
    } else if (item.overdue) {
      tft.print("OVERDUE");
    } else {
      tft.print(item.timeOrDue[0] ? item.timeOrDue : "NO DUE DATE");
    }
  }

  // Title: Auto-wrapped 1-3 lines in clean font
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(REM_WHITE);

  String remaining = item.title;
  int y = 112;
  int lines = 0;

  while (remaining.length() > 0 && lines < 3) {
    int split = remaining.indexOf(' ');
    String word;
    if (split == -1) {
      word = remaining;
      remaining = "";
    } else {
      word = remaining.substring(0, split);
      remaining = remaining.substring(split + 1);
    }

    String testLine = (lines == 0 && word == item.title) ? word : "";
    // Simple line width check
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(word.c_str(), 0, y, &x1, &y1, &w, &h);

    tft.setCursor(34, y);
    // Draw word/phrase cleanly
    tft.print(word);
    if (remaining.length() > 0 && lines < 2) {
      int nextSplit = remaining.indexOf(' ');
      String nextWord = (nextSplit == -1) ? remaining : remaining.substring(0, nextSplit);
      String combined = word + " " + nextWord;
      tft.getTextBounds(combined.c_str(), 0, y, &x1, &y1, &w, &h);
      if (w < 168) {
        tft.print(" ");
        tft.print(nextWord);
        remaining = (nextSplit == -1) ? "" : remaining.substring(nextSplit + 1);
      }
    }
    y += 20;
    lines++;
  }

  // Source list / calendar subtitle at bottom of card
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(REM_GREY);
  tft.setCursor(34, 172);
  tft.print(item.listOrCal);

  // Action pill
  tft.setTextColor(REM_YELLOW);
  tft.setCursor(132, 172);
  tft.print("SELECT: VIEW");
}

void RemindersApp::renderDetailView() {
  if (currentIndex < 0 || currentIndex >= itemCount) currentIndex = 0;
  ReminderEntry& item = items[currentIndex];

  tft.fillRoundRect(20, 44, 200, 148, 12, REM_CARD_BG);
  tft.drawRoundRect(20, 44, 200, 148, 12, item.kind == REMINDER_KIND_EVENT ? REM_CYAN : REM_YELLOW);

  // Type & Counter
  char headerBuf[32];
  snprintf(headerBuf, sizeof(headerBuf), "%s  (%d/%d)",
           (item.kind == REMINDER_KIND_EVENT ? "CALENDAR EVENT" : "TASK ITEM"),
           currentIndex + 1, itemCount);
  centerString(headerBuf, 62, &FreeSansBold9pt7b, item.kind == REMINDER_KIND_EVENT ? REM_CYAN : REM_YELLOW);

  // Time / Due
  char timeBuf[40];
  if (item.kind == REMINDER_KIND_EVENT) {
    snprintf(timeBuf, sizeof(timeBuf), "Time: %s", item.timeOrDue[0] ? item.timeOrDue : "All Day");
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "Due: %s", item.timeOrDue[0] ? item.timeOrDue : "None");
  }
  centerString(timeBuf, 84, &FreeSans9pt7b, REM_WHITE);

  // Full Title wrapped
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(REM_WHITE);
  int y = 108;
  String fullTitle = item.title;
  while (fullTitle.length() > 0 && y <= 150) {
    int maxLen = 18;
    String line = fullTitle.substring(0, min((int)fullTitle.length(), maxLen));
    if (fullTitle.length() > maxLen) {
      int space = line.lastIndexOf(' ');
      if (space > 5) {
        line = line.substring(0, space);
      }
    }
    fullTitle = fullTitle.substring(line.length());
    fullTitle.trim();

    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(line.c_str(), 0, y, &x1, &y1, &w, &h);
    tft.setCursor((240 - w) / 2, y);
    tft.print(line);
    y += 18;
  }

  // Back button prompt
  drawBadge(68, 168, 104, 18, "SELECT: RETURN", REM_CARD_BRD, REM_WHITE);
}

void RemindersApp::renderEmptyState() {
  tft.fillRoundRect(24, 52, 192, 136, 12, REM_CARD_BG);
  tft.drawRoundRect(24, 52, 192, 136, 12, REM_CARD_BRD);

  // Calendar outline illustration
  tft.drawRoundRect(96, 68, 48, 42, 4, REM_YELLOW);
  tft.fillRoundRect(96, 68, 48, 10, 4, REM_YELLOW);
  tft.fillCircle(106, 73, 2, REM_BG);
  tft.fillCircle(134, 73, 2, REM_BG);
  tft.drawFastHLine(100, 88, 40, REM_GREY);
  tft.drawFastHLine(100, 96, 40, REM_GREY);

  centerString("ALL CAUGHT UP!", 128, &FreeSansBold9pt7b, REM_WHITE);
  centerString("No pending reminders", 148, &FreeSans9pt7b, REM_GREY);

  if (syncState == REMINDERS_SYNC_FAILED) {
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(REM_RED);
    tft.setCursor(68, 166);
    tft.print("SELECT TO RETRY");
  }
}

void RemindersApp::renderUnauthState() {
  tft.fillRoundRect(24, 52, 192, 136, 12, REM_CARD_BG);
  tft.drawRoundRect(24, 52, 192, 136, 12, REM_YELLOW);

  centerString("GOOGLE LINK", 76, &FreeSansBold9pt7b, REM_YELLOW);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(REM_WHITE);
  tft.setCursor(40, 98);
  tft.print("Visit COSMO backend:");
  tft.setTextColor(REM_CYAN);
  tft.setCursor(40, 114);
  tft.print("/api/google/auth");

  tft.setTextColor(REM_GREY);
  tft.setCursor(40, 132);
  tft.print("Sign in with Google to");
  tft.setCursor(40, 144);
  tft.print("sync Calendar & Tasks.");

  drawBadge(64, 160, 112, 18, "SELECT: RETRY", REM_YELLOW, REM_BG);
}
