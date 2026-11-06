#include "RemindersApp.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>

extern const char* GITHUB_ROOT_CA;

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
    active(false),
    initialized(false),
    needsRedraw(true),
    syncing(false),
    syncDone(false),
    stagingResult(SYNC_RES_NONE),
    stagingCount(0),
    stagingSyncState(REMINDERS_SYNC_NEVER),
    syncTaskHandle(NULL) {
  memset(items, 0, sizeof(items));
  memset(stagingItems, 0, sizeof(stagingItems));
  snprintf(statusMessage, sizeof(statusMessage), "Initializing...");
  strncpy(backendUrl, "https://cosmo-pcju.onrender.com/api/google/reminders", sizeof(backendUrl) - 1);
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
  active = true;
  viewMode = REMINDERS_VIEW_CARD;
  currentIndex = 0;
  initialized = true;

  // Always load from cache first for instant offline readiness
  loadCachedFeed();

  // Initial immediate render of cached or empty state
  render();

  // If Wi-Fi is active and never synced or cache older than 10 mins, attempt fresh background sync
  if (WiFi.status() == WL_CONNECTED && (syncState == REMINDERS_SYNC_NEVER || millis() - lastSyncAttempt >= 600000)) {
    syncFeed();
  }
}

bool RemindersApp::isActive() const {
  return active && !exitRequested;
}

bool RemindersApp::wantsToExit() const {
  return exitRequested;
}

void RemindersApp::resetExit() {
  exitRequested = false;
  active = false;
}

void RemindersApp::maintain(bool wifiConnected) {
  // Handle completion of background sync task safely on main thread
  if (syncDone) {
    syncDone = false;
    Serial.printf("[REMINDERS] Background sync completed with result code: %d\n", (int)stagingResult);

    if (stagingResult == SYNC_RES_OK) {
      saveCachedFeed(stagingPayload);
      memcpy(items, stagingItems, sizeof(items));
      itemCount = stagingCount;
      if (currentIndex >= itemCount) currentIndex = 0;
      syncState = REMINDERS_SYNC_OK_LIVE;
      lastSyncTimestamp = time(nullptr);
      snprintf(statusMessage, sizeof(statusMessage), "Live Sync OK");
      needsRedraw = true;
    } else if (stagingResult == SYNC_RES_UNAUTH) {
      syncState = REMINDERS_SYNC_UNAUTH;
      snprintf(statusMessage, sizeof(statusMessage), "Google account not linked");
      needsRedraw = true;
    } else if (stagingResult == SYNC_RES_FAIL) {
      if (itemCount == 0) {
        loadCachedFeed();
      }
      if (itemCount > 0) {
        syncState = REMINDERS_SYNC_OK_CACHED;
        snprintf(statusMessage, sizeof(statusMessage), "Sync failed (cached)");
      } else {
        syncState = REMINDERS_SYNC_FAILED;
        snprintf(statusMessage, sizeof(statusMessage), "Sync failed");
      }
      needsRedraw = true;
    }
  }

  // Redraw if Reminders screen is active on display and needs update
  if (active && needsRedraw) {
    needsRedraw = false;
    render();
  }

  // Periodic 10-minute background sync
  unsigned long now = millis();
  if (wifiConnected && !syncing && (lastSyncAttempt == 0 || now - lastSyncAttempt >= 600000)) {
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
    } else if (syncState == REMINDERS_SYNC_FAILED || syncState == REMINDERS_SYNC_NEVER || syncState == REMINDERS_SYNC_UNAUTH) {
      // Retry sync on select if empty, unauthenticated, or failed
      if (WiFi.status() == WL_CONNECTED && !syncing) {
        syncFeed(true);
        render();
      }
    }
  }
}

void RemindersApp::syncTaskTrampoline(void* param) {
  RemindersApp* self = static_cast<RemindersApp*>(param);
  if (self) {
    self->runSyncTask();
  }
}

void RemindersApp::runSyncTask() {
  Serial.printf("[REMINDERS] Non-blocking background sync starting: %s\n", backendUrl);

  WiFiClientSecure client;
  time_t now = time(nullptr);
  if (now >= 1700000000) {
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    client.setInsecure();
  }
  client.setTimeout(12);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  http.setConnectTimeout(8000);
  http.setUserAgent("COSMO-ESP32-Reminders");
  http.addHeader("Accept", "application/json");
  http.setReuse(false);

  if (!http.begin(client, backendUrl)) {
    Serial.println("[REMINDERS] ERROR: HTTPClient.begin failed!");
    client.stop();
    stagingResult = SYNC_RES_FAIL;
    syncing = false;
    syncDone = true;
    syncTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }

  int httpCode = http.GET();
  Serial.printf("[REMINDERS] HTTP status code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    client.stop();

    ReminderEntry parsedItems[MAX_REMINDERS];
    int parsedCount = 0;
    RemindersSyncState parsedState = REMINDERS_SYNC_FAILED;

    if (parseRemindersJson(payload, parsedItems, parsedCount, parsedState)) {
      memcpy(stagingItems, parsedItems, sizeof(stagingItems));
      stagingCount = parsedCount;
      stagingSyncState = parsedState;
      stagingPayload = payload;
      stagingResult = SYNC_RES_OK;
    } else if (parsedState == REMINDERS_SYNC_UNAUTH) {
      stagingSyncState = REMINDERS_SYNC_UNAUTH;
      stagingResult = SYNC_RES_UNAUTH;
    } else {
      stagingResult = SYNC_RES_FAIL;
    }
  } else if (httpCode == HTTP_CODE_UNAUTHORIZED || httpCode == HTTP_CODE_FORBIDDEN) {
    http.end();
    client.stop();
    stagingSyncState = REMINDERS_SYNC_UNAUTH;
    stagingResult = SYNC_RES_UNAUTH;
  } else {
    Serial.printf("[REMINDERS] HTTP request failed with code %d\n", httpCode);
    http.end();
    client.stop();
    stagingResult = SYNC_RES_FAIL;
  }

  syncing = false;
  syncDone = true;
  syncTaskHandle = NULL;
  vTaskDelete(NULL);
}

bool RemindersApp::syncFeed(bool force) {
  if (WiFi.status() != WL_CONNECTED) {
    if (syncState == REMINDERS_SYNC_NEVER) {
      loadCachedFeed();
    }
    return false;
  }

  if (syncing) {
    Serial.println("[REMINDERS] Sync already in progress, skipping.");
    return false;
  }

  lastSyncAttempt = millis();
  syncing = true;
  syncDone = false;

  BaseType_t res = xTaskCreatePinnedToCore(
    syncTaskTrampoline,
    "remSyncTask",
    8192,
    this,
    1,
    &syncTaskHandle,
    0
  );

  if (res != pdPASS) {
    Serial.println("[REMINDERS] ERROR: Failed to create background sync task!");
    syncing = false;
    return false;
  }

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

  ReminderEntry parsedItems[MAX_REMINDERS];
  int parsedCount = 0;
  RemindersSyncState parsedState = REMINDERS_SYNC_FAILED;

  if (parseRemindersJson(payload, parsedItems, parsedCount, parsedState)) {
    memcpy(items, parsedItems, sizeof(items));
    itemCount = parsedCount;
    if (currentIndex >= itemCount) currentIndex = 0;
    syncState = REMINDERS_SYNC_OK_CACHED;
    snprintf(statusMessage, sizeof(statusMessage), "Loaded from cache");
    return true;
  }

  return false;
}

bool RemindersApp::saveCachedFeed(const String& json) {
  if (json.length() < 10) return false;

  File f = LittleFS.open(REMINDERS_CACHE_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("[REMINDERS] ERROR: Failed to open cache file for writing.");
    return false;
  }
  f.print(json);
  f.close();
  Serial.printf("[REMINDERS] Successfully saved %u bytes to %s\n", (unsigned int)json.length(), REMINDERS_CACHE_FILE);
  return true;
}

bool RemindersApp::parseRemindersJson(
  const String& json,
  ReminderEntry* targetItems,
  int& targetCount,
  RemindersSyncState& targetState
) {
  targetCount = 0;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("[REMINDERS] JSON parse error: %s\n", err.c_str());
    targetState = REMINDERS_SYNC_FAILED;
    return false;
  }

  const char* status = doc["status"] | "";
  if (strcmp(status, "unauthenticated") == 0) {
    targetState = REMINDERS_SYNC_UNAUTH;
    return false;
  }

  if (strcmp(status, "ok") != 0 && strcmp(status, "partial") != 0) {
    Serial.printf("[REMINDERS] JSON status not ok/partial: %s\n", status);
    targetState = REMINDERS_SYNC_FAILED;
    return false;
  }

  int count = 0;

  // Calendar Events
  JsonArray events = doc["events"].as<JsonArray>();
  for (JsonObject ev : events) {
    if (count >= MAX_REMINDERS) break;
    const char* title = ev["title"] | "";
    if (strlen(title) == 0) continue;

    targetItems[count].kind = REMINDER_KIND_EVENT;
    strncpy(targetItems[count].title, title, sizeof(targetItems[count].title) - 1);
    targetItems[count].title[sizeof(targetItems[count].title) - 1] = '\0';

    const char* t = ev["time"] | "";
    strncpy(targetItems[count].timeOrDue, t, sizeof(targetItems[count].timeOrDue) - 1);
    targetItems[count].timeOrDue[sizeof(targetItems[count].timeOrDue) - 1] = '\0';

    strncpy(targetItems[count].listOrCal, "Calendar", sizeof(targetItems[count].listOrCal) - 1);
    targetItems[count].listOrCal[sizeof(targetItems[count].listOrCal) - 1] = '\0';

    targetItems[count].completed = false;
    targetItems[count].overdue = false;
    targetItems[count].allDay = ev["allDay"] | false;
    count++;
  }

  // Google Tasks
  JsonArray tasks = doc["tasks"].as<JsonArray>();
  for (JsonObject tk : tasks) {
    if (count >= MAX_REMINDERS) break;
    const char* title = tk["title"] | "";
    if (strlen(title) == 0) continue;

    targetItems[count].kind = REMINDER_KIND_TASK;
    strncpy(targetItems[count].title, title, sizeof(targetItems[count].title) - 1);
    targetItems[count].title[sizeof(targetItems[count].title) - 1] = '\0';

    const char* dueStr = tk["dueFormatted"] | tk["due"] | "";
    strncpy(targetItems[count].timeOrDue, dueStr, sizeof(targetItems[count].timeOrDue) - 1);
    targetItems[count].timeOrDue[sizeof(targetItems[count].timeOrDue) - 1] = '\0';

    const char* lt = tk["listTitle"] | "Tasks";
    strncpy(targetItems[count].listOrCal, lt, sizeof(targetItems[count].listOrCal) - 1);
    targetItems[count].listOrCal[sizeof(targetItems[count].listOrCal) - 1] = '\0';

    targetItems[count].completed = tk["completed"] | false;
    targetItems[count].overdue = tk["overdue"] | false;
    targetItems[count].allDay = false;
    count++;
  }

  targetCount = count;
  targetState = REMINDERS_SYNC_OK_LIVE;
  Serial.printf("[REMINDERS] Parsed %d reminder items (status=%s, events=%u, tasks=%u)\n",
                targetCount, status, (unsigned int)events.size(), (unsigned int)tasks.size());
  return true;
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
  if (syncing) {
    statusDotColor = REM_YELLOW;
    statusTxt = "SYNC";
  } else if (syncState == REMINDERS_SYNC_OK_LIVE) {
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

    tft.setCursor(34, y);
    tft.print(word);
    if (remaining.length() > 0 && lines < 2) {
      int nextSplit = remaining.indexOf(' ');
      String nextWord = (nextSplit == -1) ? remaining : remaining.substring(0, nextSplit);
      String combined = word + " " + nextWord;
      int16_t x1, y1;
      uint16_t w, h;
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

  if (syncing) {
    centerString("SYNCING...", 128, &FreeSansBold9pt7b, REM_YELLOW);
    centerString("Fetching Google feed", 148, &FreeSans9pt7b, REM_GREY);
  } else {
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

  if (syncing) {
    drawBadge(64, 160, 112, 18, "SYNCING...", REM_YELLOW, REM_BG);
  } else {
    drawBadge(64, 160, 112, 18, "SELECT: RETRY", REM_YELLOW, REM_BG);
  }
}
