#ifndef REMINDERS_APP_H
#define REMINDERS_APP_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <time.h>
#include <ArduinoJson.h>

#define MAX_REMINDERS 16

enum ReminderKind {
  REMINDER_KIND_EVENT,
  REMINDER_KIND_TASK
};

struct ReminderEntry {
  ReminderKind kind;
  char title[64];
  char timeOrDue[32];
  char listOrCal[32];
  bool completed;
  bool overdue;
  bool allDay;
};

enum RemindersSyncState {
  REMINDERS_SYNC_NEVER,
  REMINDERS_SYNC_OK_LIVE,
  REMINDERS_SYNC_OK_CACHED,
  REMINDERS_SYNC_FAILED,
  REMINDERS_SYNC_UNAUTH
};

enum RemindersViewMode {
  REMINDERS_VIEW_CARD,
  REMINDERS_VIEW_DETAIL
};

enum SyncTaskResult {
  SYNC_RES_NONE,
  SYNC_RES_OK,
  SYNC_RES_UNAUTH,
  SYNC_RES_FAIL
};

class RemindersApp {
public:
  RemindersApp(Adafruit_GC9A01A& display);
  ~RemindersApp();

  void init();
  void update(bool left, bool right, bool select);
  void maintain(bool wifiConnected);

  void setBackendUrl(const char* url);
  bool syncFeed(bool force = false);
  bool loadCachedFeed();
  bool saveCachedFeed(const String& json);

  bool isActive() const;
  bool wantsToExit() const;
  void resetExit();
  bool isSyncing() const { return syncing; }

private:
  Adafruit_GC9A01A& tft;
  char backendUrl[128];

  ReminderEntry items[MAX_REMINDERS];
  int itemCount;
  int currentIndex;

  RemindersSyncState syncState;
  RemindersViewMode viewMode;

  unsigned long lastSyncAttempt;
  time_t lastSyncTimestamp;
  char statusMessage[64];
  bool exitRequested;
  bool active;
  bool initialized;
  bool needsRedraw;

  // Background non-blocking synchronization & FreeRTOS safety
  volatile bool syncing;
  volatile bool syncDone;
  SemaphoreHandle_t dataMutex;
  SyncTaskResult stagingResult;
  ReminderEntry stagingItems[MAX_REMINDERS];
  int stagingCount;
  RemindersSyncState stagingSyncState;
  String stagingPayload;
  TaskHandle_t syncTaskHandle;

  static void syncTaskTrampoline(void* param);
  void runSyncTask();

  void render();
  void renderCardView();
  void renderDetailView();
  void renderEmptyState();
  void renderUnauthState();

  bool parseRemindersJson(const String& json, ReminderEntry* targetItems, int& targetCount, RemindersSyncState& targetState);
  void centerString(const char* text, int y, const GFXfont* font, uint16_t color);
  void drawBadge(int x, int y, int w, int h, const char* label, uint16_t bgColor, uint16_t textColor);
};

#endif // REMINDERS_APP_H
