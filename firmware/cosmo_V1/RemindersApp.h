#ifndef REMINDERS_APP_H
#define REMINDERS_APP_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <time.h>

#define MAX_REMINDERS 16

enum ReminderKind {
  REMINDER_KIND_EVENT,
  REMINDER_KIND_TASK
};

struct ReminderEntry {
  ReminderKind kind;
  char title[48];
  char timeOrDue[24];
  char listOrCal[24];
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

class RemindersApp {
public:
  RemindersApp(Adafruit_GC9A01A& display);

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
  bool initialized;
  bool needsRedraw;

  void render();
  void renderCardView();
  void renderDetailView();
  void renderEmptyState();
  void renderUnauthState();

  void parseRemindersJson(const String& json);
  void centerString(const char* text, int y, const GFXfont* font, uint16_t color);
  void drawBadge(int x, int y, int w, int h, const char* label, uint16_t bgColor, uint16_t textColor);
  void extractJsonField(const String& json, const String& key, char* dest, size_t destSize);
};

#endif // REMINDERS_APP_H
