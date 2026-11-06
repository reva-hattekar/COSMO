#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <time.h>
#include <math.h>

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

struct Button {
  int pin;
  bool stableState;
  bool lastReading;
  unsigned long lastChange;
};

// ======================================
// CONFIGURATION
// ======================================

#define DEVICE_NAME "COSMO"

#if __has_include("secrets.h")
#include "secrets.h"
#else
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif

const long GMT_OFFSET = 19800;
const int DAYLIGHT_OFFSET = 0;

// ======================================
// DISPLAY
// ======================================

#define TFT_CS  5
#define TFT_DC  32
#define TFT_RST 4

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

const int SCREEN_W = 240;
const int SCREEN_H = 240;

// ======================================
// ONLINE WALLPAPER CONFIGURATION
// ======================================

#define COSMO_WALLPAPER_URL "https://raw.githubusercontent.com/reva-hattekar/COSMO/main/assets/wallpaper_1.png"
const char* WALLPAPER_CACHE_PATH = "/wallpaper.jpg";

bool wallpaperLoaded = false;
bool wallpaperAttempted = false;
uint32_t tjpgCallbackCount = 0;

static const char* GITHUB_ROOT_CA =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMHSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  tjpgCallbackCount++;
  if (y >= SCREEN_H) return 0;
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

#include "SnakeGame.h"
#include "TetrisGame.h"
#include "SimonGame.h"
#include "ChessApp.h"

SnakeGame snake(tft);
TetrisGame tetris(tft);
SimonGame simon(tft);
ChessApp chess(tft);

// ======================================
// BUTTONS
// ======================================

#define BTN_LEFT   33
#define BTN_RIGHT  25
#define BTN_SELECT 14

Button leftButton   = {BTN_LEFT, HIGH, HIGH, 0};
Button rightButton  = {BTN_RIGHT, HIGH, HIGH, 0};
Button selectButton = {BTN_SELECT, HIGH, HIGH, 0};

const unsigned long DEBOUNCE_MS = 25;

bool leftPressed = false;
bool rightPressed = false;
bool selectPressed = false;

// ======================================
// COLOURS
// ======================================

#define BG       0x0000
#define WHITE    0xFFFF
#define CYAN     0x07FF
#define BLUE     0x001F
#define PURPLE   0x801F
#define GREY     0x8410
#define YELLOW   0xFFE0
#define DARKBLUE 0x000D

// ======================================
// APPLICATIONS
// ======================================

const char* appNames[] = {
  "MAGIC 8 BALL",
  "CHESS",
  "SNAKE",
  "TETRIS",
  "SIMON SAYS",
  "REMINDERS"
};

const int APP_COUNT = 6;

int appIndex = 0;

enum ScreenState {
  HOME,
  LAUNCHER,
  APP
};

ScreenState screen = HOME;

// ======================================
// MAGIC 8 BALL
// ======================================

const char* answers[] = {
  "It is certain.",
  "Without a doubt.",
  "You may rely on it.",
  "Absolutely!",
  "Signs point to yes.",
  "Most likely.",
  "Outlook good.",
  "Yes, definitely.",
  "As I see it, yes.",
  "Better not tell you.",
  "Ask again later.",
  "Cannot predict now.",
  "Concentrate and ask.",
  "Reply hazy, try again.",
  "Don't count on it.",
  "My reply is no.",
  "Very doubtful.",
  "Outlook not good.",
  "My sources say no.",
  "Absolutely not."
};

const int ANSWER_COUNT = 20;

// ======================================
// TIMERS
// ======================================

unsigned long lastInteraction = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastWiFiAttempt = 0;

const unsigned long IDLE_TIMEOUT = 10000;
const unsigned long WIFI_RETRY = 10000;

// ======================================
// OFF-SCREEN CAROUSEL CANVAS
// ======================================

// 180 x 90 x 2 bytes = 32,400 bytes.

const int CANVAS_W = 200;
const int CANVAS_H = 130;

GFXcanvas16 carouselCanvas(CANVAS_W, CANVAS_H);

bool canvasAvailable = false;

const int CANVAS_X = 20;
const int CANVAS_Y = 32;
// ======================================
// CAROUSEL ANIMATION
// ======================================

bool animating = false;

int animationDirection = 0;

unsigned long animationStart = 0;
unsigned long lastAnimationFrame = 0;

const unsigned long ANIMATION_DURATION = 320;
const unsigned long FRAME_INTERVAL = 25;

const float CAROUSEL_SPACING = 77.0f;
// ======================================
// BUTTON HANDLING
// ======================================

bool updateButton(Button& button) {

  bool reading = digitalRead(button.pin);

  if (reading != button.lastReading) {
    button.lastChange = millis();
    button.lastReading = reading;
  }

  if (
    millis() - button.lastChange >= DEBOUNCE_MS &&
    reading != button.stableState
  ) {

    button.stableState = reading;

    if (button.stableState == LOW) {
      return true;
    }
  }

  return false;
}

const unsigned long REPEAT_INITIAL_DELAY = 400; // 400 ms initial hold before repeat
const unsigned long REPEAT_INTERVAL      = 120; // 120 ms repeat interval

unsigned long leftHoldStart = 0;
unsigned long leftNextRepeat = 0;
unsigned long rightHoldStart = 0;
unsigned long rightNextRepeat = 0;

bool leftRepeated = false;
bool rightRepeated = false;

bool bothHeld() {
  return digitalRead(BTN_LEFT) == LOW &&
         digitalRead(BTN_RIGHT) == LOW;
}

void updateButtons() {
  leftPressed = updateButton(leftButton);
  rightPressed = updateButton(rightButton);
  selectPressed = updateButton(selectButton);

  leftRepeated = false;
  rightRepeated = false;

  // Auto-repeat for Left / Right navigation (disabled if both held)
  if (!bothHeld()) {
    if (digitalRead(BTN_LEFT) == LOW) {
      if (leftHoldStart == 0) {
        leftHoldStart = millis();
        leftNextRepeat = leftHoldStart + REPEAT_INITIAL_DELAY;
      } else if (millis() >= leftNextRepeat) {
        leftRepeated = true;
        leftNextRepeat = millis() + REPEAT_INTERVAL;
      }
    } else {
      leftHoldStart = 0;
    }

    if (digitalRead(BTN_RIGHT) == LOW) {
      if (rightHoldStart == 0) {
        rightHoldStart = millis();
        rightNextRepeat = rightHoldStart + REPEAT_INITIAL_DELAY;
      } else if (millis() >= rightNextRepeat) {
        rightRepeated = true;
        rightNextRepeat = millis() + REPEAT_INTERVAL;
      }
    } else {
      rightHoldStart = 0;
    }
  } else {
    leftHoldStart = 0;
    rightHoldStart = 0;
  }
}

void waitForButtonRelease() {
  while (digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
    delay(15);
  }
  delay(30);
  leftButton.stableState = HIGH;
  leftButton.lastReading = HIGH;
  rightButton.stableState = HIGH;
  rightButton.lastReading = HIGH;
  selectButton.stableState = HIGH;
  selectButton.lastReading = HIGH;
  leftPressed = false;
  rightPressed = false;
  selectPressed = false;
  leftRepeated = false;
  rightRepeated = false;
  leftHoldStart = 0;
  rightHoldStart = 0;
}

// ======================================
// TEXT HELPERS
// ======================================

void centerText(
  const char* text,
  int y,
  const GFXfont* font,
  uint16_t colour
) {

  tft.setFont(font);
  tft.setTextColor(colour);

  int16_t x1, y1;
  uint16_t w, h;

  tft.getTextBounds(
    text,
    0,
    y,
    &x1,
    &y1,
    &w,
    &h
  );

  tft.setCursor((SCREEN_W - w) / 2, y);
  tft.print(text);
}

// ======================================
// WALLPAPER & CLOCK
// ======================================

bool checkAndUpdateWallpaper(const char* url, const char* path) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.println("[WALLPAPER] Checking cloud wallpaper for updates...");

  WiFiClientSecure client;
  time_t now = time(nullptr);
  if (now >= 1700000000) {
    client.setCACert(GITHUB_ROOT_CA);
  } else {
    client.setInsecure();
  }

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  http.setConnectTimeout(6000);
  http.setUserAgent("COSMO-ESP32");
  http.addHeader("Accept", "image/jpeg, */*");

  if (!http.begin(client, url)) {
    Serial.println("[WALLPAPER] ERROR: HTTPClient.begin failed during update check.");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[WALLPAPER] Update check: server returned HTTP %d (%s).\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  int remoteSize = http.getSize();
  String etag = http.header("ETag");

  // Check if we already have this wallpaper cached
  size_t currentSize = 0;
  if (LittleFS.exists(path)) {
    File curF = LittleFS.open(path, FILE_READ);
    if (curF) {
      currentSize = curF.size();
      curF.close();
    }
  }

  // Check cached etag
  String cachedEtag = "";
  if (LittleFS.exists("/wallpaper.etag")) {
    File etagF = LittleFS.open("/wallpaper.etag", FILE_READ);
    if (etagF) {
      cachedEtag = etagF.readString();
      cachedEtag.trim();
      etagF.close();
    }
  }

  // If local file exists, is valid (> 1000 bytes), and matches remote size and ETag, skip download
  if (currentSize > 1000 && remoteSize > 0 && (size_t)remoteSize == currentSize && (etag.length() == 0 || etag == cachedEtag)) {
    Serial.printf("[WALLPAPER] Cache is up to date (%u bytes). No download needed.\n", (unsigned int)currentSize);
    http.end();
    return false;
  }

  Serial.println("[WALLPAPER] ========================================");
  Serial.printf("[WALLPAPER] Wallpaper update available (remote: %d bytes vs local: %u bytes). Downloading...\n",
                remoteSize, (unsigned int)currentSize);

  // Staging file in LittleFS to protect existing cache
  const char* tempPath = "/wallpaper.tmp";
  File file = LittleFS.open(tempPath, FILE_WRITE);
  if (!file) {
    Serial.println("[WALLPAPER] ERROR: Failed to create staging file /wallpaper.tmp in LittleFS!");
    http.end();
    return false;
  }

  int bytesWritten = http.writeToStream(&file);
  file.flush();
  file.close();
  http.end();

  Serial.printf("[WALLPAPER] Actual downloaded byte count: %d bytes\n", bytesWritten);

  if (bytesWritten < 1000) {
    Serial.printf("[WALLPAPER] ERROR: Download incomplete (%d bytes). Preserving existing cache.\n", bytesWritten);
    LittleFS.remove(tempPath);
    return false;
  }

  // Verify staging file
  File verifyFile = LittleFS.open(tempPath, FILE_READ);
  if (!verifyFile) {
    Serial.println("[WALLPAPER] ERROR: Could not open staging file for verification!");
    return false;
  }
  size_t storedSize = verifyFile.size();
  uint8_t header[2] = {0, 0};
  verifyFile.read(header, 2);
  verifyFile.close();

  Serial.printf("[WALLPAPER] File write result: Stored size = %u bytes\n", (unsigned int)storedSize);

  if (header[0] != 0xFF || header[1] != 0xD8) {
    Serial.printf("[WALLPAPER] JPEG validation result: FAILED (magic bytes 0x%02X 0x%02X, expected 0xFF 0xD8). Preserving cache.\n", header[0], header[1]);
    LittleFS.remove(tempPath);
    return false;
  }

  uint16_t jw = 0, jh = 0;
  JRESULT jres = TJpgDec.getFsJpgSize(&jw, &jh, tempPath, LittleFS);
  Serial.printf("[WALLPAPER] JPEG validation result: %s (code=%d, dimensions=%ux%u)\n", 
                (jres == JDR_OK ? "VALID JPEG" : "CORRUPT JPEG"), (int)jres, jw, jh);

  if (jres != JDR_OK) {
    Serial.printf("[WALLPAPER] ERROR: TJpg_Decoder header parse failed with code %d. Preserving cache.\n", (int)jres);
    LittleFS.remove(tempPath);
    return false;
  }

  // Atomically promote to final cache path
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
  }
  if (!LittleFS.rename(tempPath, path)) {
    Serial.printf("[WALLPAPER] ERROR: Failed to rename %s to %s!\n", tempPath, path);
    return false;
  }

  // Save new ETag if provided
  if (etag.length() > 0) {
    File etagF = LittleFS.open("/wallpaper.etag", FILE_WRITE);
    if (etagF) {
      etagF.print(etag);
      etagF.close();
    }
  }

  Serial.printf("[WALLPAPER] Final file size: %s is %u bytes. Successfully cached!\n", path, (unsigned int)storedSize);
  Serial.println("[WALLPAPER] ========================================");
  wallpaperLoaded = true;
  return true;
}

bool downloadWallpaper(const char* url, const char* path) {
  return checkAndUpdateWallpaper(url, path);
}

bool renderWallpaper() {
  tjpgCallbackCount = 0;
  Serial.println("[WALLPAPER] Starting render from LittleFS...");

  if (!LittleFS.exists(WALLPAPER_CACHE_PATH)) {
    Serial.printf("[WALLPAPER] Render FAILED: File %s not found in LittleFS.\n", WALLPAPER_CACHE_PATH);
    return false;
  }

  File f = LittleFS.open(WALLPAPER_CACHE_PATH, FILE_READ);
  if (!f) {
    Serial.printf("[WALLPAPER] Render FAILED: Cannot open %s for reading.\n", WALLPAPER_CACHE_PATH);
    return false;
  }
  size_t fSize = f.size();
  f.close();

  Serial.printf("[WALLPAPER] Cached file size: %u bytes\n", (unsigned int)fSize);
  if (fSize < 1000) {
    Serial.printf("[WALLPAPER] Render FAILED: File size %u too small. Purging corrupted file.\n", (unsigned int)fSize);
    LittleFS.remove(WALLPAPER_CACHE_PATH);
    return false;
  }

  uint16_t w = 0, h = 0;
  JRESULT szRes = TJpgDec.getFsJpgSize(&w, &h, WALLPAPER_CACHE_PATH, LittleFS);
  if (szRes != JDR_OK) {
    Serial.printf("[WALLPAPER] Render FAILED: getFsJpgSize error %d. Purging corrupted file.\n", (int)szRes);
    LittleFS.remove(WALLPAPER_CACHE_PATH);
    return false;
  }

  Serial.printf("[WALLPAPER] Decoded dimensions: %ux%u. Decoding JPEG blocks directly to GC9A01...\n", w, h);

  JRESULT decRes = TJpgDec.drawFsJpg(0, 0, WALLPAPER_CACHE_PATH, LittleFS);
  Serial.printf("[WALLPAPER] JPEG decoder result: %d (%s)\n", (int)decRes, (decRes == JDR_OK ? "JDR_OK" : "DECODE_ERROR"));
  Serial.printf("[WALLPAPER] Number of drawing callback invocations: %u\n", tjpgCallbackCount);

  if (decRes == JDR_OK && tjpgCallbackCount > 0) {
    Serial.println("[WALLPAPER] SUCCESS: Wallpaper rendered completely on display.");
    return true;
  }

  Serial.printf("[WALLPAPER] Render FAILED: decRes=%d, callbacks=%u. Falling back.\n", (int)decRes, tjpgCallbackCount);
  return false;
}

void drawClock() {

  struct tm timeInfo;

  if (getLocalTime(&timeInfo, 10)) {
    char timeString[8];
    strftime(timeString, sizeof(timeString), "%H:%M", &timeInfo);
    centerText(timeString, 146, &FreeSansBold9pt7b, WHITE);

    char dateString[20];
    strftime(dateString, sizeof(dateString), "%a, %b %d", &timeInfo);
    for (int i = 0; dateString[i]; i++) {
      dateString[i] = toupper((unsigned char)dateString[i]);
    }
    centerText(dateString, 170, &FreeSans9pt7b, WHITE);
  }

  lastClockUpdate = millis();
}

void drawHome() {
  Serial.println("[HOME] Drawing home screen...");
  bool rendered = false;

  // 1. Try rendering from existing LittleFS cache
  if (LittleFS.exists(WALLPAPER_CACHE_PATH)) {
    rendered = renderWallpaper();
    if (!rendered) {
      Serial.println("[HOME] Existing wallpaper cache failed to render. Will attempt fresh download if Wi-Fi available.");
    }
  }

  // 2. If not rendered and Wi-Fi is connected, download and render
  if (!rendered && WiFi.status() == WL_CONNECTED) {
    Serial.println("[HOME] No valid wallpaper cached. Downloading from cloud...");
    if (downloadWallpaper(COSMO_WALLPAPER_URL, WALLPAPER_CACHE_PATH)) {
      rendered = renderWallpaper();
    }
  }

  // 3. Fallback or success handling
  if (!rendered) {
    wallpaperLoaded = false;
    Serial.print("[HOME] Drawing dark fallback background. Exact reason: ");
    if (WiFi.status() != WL_CONNECTED && !LittleFS.exists(WALLPAPER_CACHE_PATH)) {
      Serial.println("Wi-Fi not connected and no local cache exists.");
    } else if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Download or JPEG decoding encountered an error.");
    } else {
      Serial.println("Local cache corrupted and Wi-Fi unavailable.");
    }

    tft.fillScreen(BG);
    tft.drawCircle(120, 120, 114, DARKBLUE);
    tft.drawCircle(120, 120, 100, 0x10A2);
  } else {
    wallpaperLoaded = true;
  }


  drawClock();
}

// ======================================
// APP COLOURS
// ======================================

uint16_t getAppColour(int index) {

  switch (index) {

    case 0: return BLUE;     // Magic 8 Ball
    case 1: return 0x2965;   // Chess (Slate Blue)
    case 2: return 0x07E0;   // Snake (Green)
    case 3: return CYAN;     // Tetris (Cyan)
    case 4: return DARKBLUE; // Simon Says (Dark Blue)
    case 5: return YELLOW;   // Reminders (Yellow)

    default: return 0x2965;
  }
}

// ======================================
// COLOR DIMMING (RGB565)
// ======================================

uint16_t dimColor(uint16_t color, float brightness) {
  if (brightness >= 0.99f) return color;
  if (brightness <= 0.01f) return 0;

  uint16_t r = (color >> 11) & 0x1F;
  uint16_t g = (color >> 5) & 0x3F;
  uint16_t b = color & 0x1F;

  r = (uint16_t)(r * brightness);
  g = (uint16_t)(g * brightness);
  b = (uint16_t)(b * brightness);

  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;

  return (r << 11) | (g << 5) | b;
}

// ======================================
// APP ICON DRAWING
// ======================================

void drawAppIcon(
  Adafruit_GFX& g,
  int index,
  int x,
  int y,
  int radius,
  float brightness = 1.0f
) {

  index = (index + APP_COUNT) % APP_COUNT;

  uint16_t baseColour = dimColor(getAppColour(index), brightness);
  uint16_t whiteColour = dimColor(WHITE, brightness);
  uint16_t darkBlueColour = dimColor(DARKBLUE, brightness);
  uint16_t blueColour = dimColor(BLUE, brightness);
  uint16_t bgColour = dimColor(BG, brightness);

  g.fillCircle(x, y, radius, baseColour);
  g.drawCircle(x, y, radius, whiteColour);

  if (index == 0) {
    // Magic 8 Ball
    g.fillCircle(
      x, y,
      radius - 6,
      darkBlueColour
    );

    g.setFont(&FreeSansBold12pt7b);
    g.setTextColor(whiteColour);
    g.setCursor(x - 8, y + 9);
    g.print("8");
  }

  else if (index == 1) {
    // Chess: Stylized King piece with crown & base
    uint16_t goldColour = dimColor(0xFEA0, brightness);
    g.drawFastVLine(x, y - 9, 4, goldColour);
    g.drawFastHLine(x - 2, y - 7, 5, goldColour);
    g.fillRoundRect(x - 6, y - 5, 12, 5, 2, goldColour);
    g.fillRoundRect(x - 4, y, 8, 5, 2, whiteColour);
    g.fillRoundRect(x - 7, y + 5, 14, 4, 1, goldColour);
  }

  else if (index == 2) {
    // Snake
    int s = radius / 3;
    g.fillRoundRect(x - s, y - s / 2, s * 2, s, 3, dimColor(0x03E0, brightness));
    g.fillCircle(x + s - 1, y, s / 2 + 1, dimColor(0x7FE0, brightness)); // head
    g.fillCircle(x - s, y + s / 2, s / 3 + 1, dimColor(0xF800, brightness)); // red apple
  }

  else if (index == 3) {
    // Tetris: Classic T-tetromino
    int b = radius / 3;
    uint16_t tetrisPurple = dimColor(PURPLE, brightness);
    g.fillRect(x - b * 3 / 2, y - b, b, b, tetrisPurple);
    g.fillRect(x - b / 2, y - b, b, b, tetrisPurple);
    g.fillRect(x + b / 2, y - b, b, b, tetrisPurple);
    g.fillRect(x - b / 2, y, b, b, tetrisPurple);

    g.drawRect(x - b * 3 / 2, y - b, b, b, whiteColour);
    g.drawRect(x - b / 2, y - b, b, b, whiteColour);
    g.drawRect(x + b / 2, y - b, b, b, whiteColour);
    g.drawRect(x - b / 2, y, b, b, whiteColour);
  }

  else if (index == 4) {
    // Simon Says: 4 colored pads
    int q = radius / 2;
    g.fillRoundRect(x - q - 2, y - q - 2, q, q, 3, dimColor(0x07E0, brightness)); // Green
    g.fillRoundRect(x + 2, y - q - 2, q, q, 3, dimColor(0xF800, brightness));     // Red
    g.fillRoundRect(x - q - 2, y + 2, q, q, 3, dimColor(0xFFE0, brightness));     // Yellow
    g.fillRoundRect(x + 2, y + 2, q, q, 3, dimColor(0x051F, brightness));         // Blue
    g.fillCircle(x, y, q / 3 + 1, bgColour);
  }

  else if (index == 5) {
    // Reminders
    g.drawRoundRect(
      x - radius / 2,
      y - radius / 2,
      radius,
      radius,
      4,
      bgColour
    );

    g.drawLine(
      x - 7, y - 5,
      x + 7, y - 5,
      bgColour
    );

    g.drawLine(
      x - 7, y + 4,
      x + 7, y + 4,
      bgColour
    );
  }
}

// ======================================
// EASING
// ======================================

float easeInOutCubic(float t) {

  if (t < 0.5f) {
    return 4.0f * t * t * t;
  }

  float p = -2.0f * t + 2.0f;

  return 1.0f - (p * p * p) / 2.0f;
}

// ======================================
// DRAW CAROUSEL INTO CANVAS
// ======================================

void renderCarousel(float progress) {

  carouselCanvas.fillScreen(BG);

  float offset = 0;

  if (animating) {

    float remaining = 1.0f - progress;

    offset =
      animationDirection *
      remaining *
      CAROUSEL_SPACING;
  }

  for (int slot = -1; slot <= 1; slot++) {

    int index =
      (appIndex + slot + APP_COUNT) % APP_COUNT;

    float x =
      100.0f +
      slot * CAROUSEL_SPACING +
      offset;

    float distance = fabs(x - 100.0f);

    float scale = distance / CAROUSEL_SPACING;

    if (scale > 1.0f) {
      scale = 1.0f;
    }

    // Centre icon: radius 55.
    // Side icons: radius 22.

    int radius =
      55 - (int)(33.0f * scale);

    if (x < -radius || x > CANVAS_W + radius) {
      continue;
    }

    // Center icon has scale = 0 -> brightness = 1.0 (100% full brightness)
    // Side icons have scale = 1 -> brightness = 0.40 (40% brightness, recessed depth)
    float brightness = 1.0f - (0.60f * scale);
    if (brightness < 0.40f) brightness = 0.40f;
    if (brightness > 1.0f) brightness = 1.0f;

    drawAppIcon(
      carouselCanvas,
      index,
      (int)x,
      65,
      radius,
      brightness
    );
  }
}
// ======================================
// TRANSFER CANVAS TO TFT
// ======================================

void presentCarousel() {

  if (!canvasAvailable) {
    return;
  }

  tft.drawRGBBitmap(
    CANVAS_X,
    CANVAS_Y,
    carouselCanvas.getBuffer(),
    CANVAS_W,
    CANVAS_H
  );
}

// ======================================
// CAROUSEL DETAILS
// ======================================

void drawCarouselDetails() {

  tft.fillRect(43, 164, 154, 26, BG);

  centerText(
  appNames[appIndex],
  181,
  &FreeSans9pt7b,
  WHITE
);
    

  tft.fillRect(82, 190, 76, 17, BG);

  for (int i = 0; i < APP_COUNT; i++) {

    uint16_t colour =
      (i == appIndex) ? CYAN : GREY;

    int radius =
      (i == appIndex) ? 3 : 2;

    tft.fillCircle(
      92 + i * 11,
      198,
      radius,
      colour
    );
  }
}

// ======================================
// DRAW LAUNCHER
// ======================================

void drawLauncher() {

  tft.fillScreen(BG);

  animating = false;

  renderCarousel(1.0f);
  presentCarousel();

  drawCarouselDetails();
}

// ======================================
// START ANIMATION
// ======================================

void moveCarousel(int direction) {

  if (animating) {
    return;
  }

  animationDirection = direction;

  appIndex =
    (appIndex + direction + APP_COUNT) % APP_COUNT;

  animating = true;

  animationStart = millis();
  lastAnimationFrame = 0;

  lastInteraction = millis();

  drawCarouselDetails();

  renderCarousel(0.0f);
  presentCarousel();
}

// ======================================
// UPDATE ANIMATION
// ======================================

void updateCarouselAnimation() {

  if (!animating) {
    return;
  }

  unsigned long now = millis();

  if (
    now - lastAnimationFrame <
    FRAME_INTERVAL
  ) {
    return;
  }

  lastAnimationFrame = now;

  float progress =
    (float)(now - animationStart) /
    (float)ANIMATION_DURATION;

  if (progress >= 1.0f) {

    progress = 1.0f;

    animating = false;

    renderCarousel(1.0f);
    presentCarousel();

    return;
  }

  float eased = easeInOutCubic(progress);

  renderCarousel(eased);
  presentCarousel();
}

// ======================================
// MAGIC 8 BALL
// ======================================

void drawMagicScreen() {

  tft.fillScreen(BG);

  centerText(
    "MAGIC 8 BALL",
    53,
    &FreeSansBold12pt7b,
    CYAN
  );

  tft.drawCircle(120, 110, 55, WHITE);
  tft.drawCircle(120, 110, 51, GREY);

  centerText(
    "8",
    119,
    &FreeSansBold12pt7b,
    WHITE
  );

  tft.fillRoundRect(
    61, 153,
    118, 32,
    8,
    BLUE
  );

  centerText(
    "PRESS SELECT",
    174,
    &FreeSans9pt7b,
    WHITE
  );
}

// ======================================
// MAGIC 8 BALL THINKING
// ======================================

void thinkingAnimation() {

  for (int i = 0; i < 4; i++) {

    tft.fillScreen(BG);

    tft.drawCircle(120, 108, 55, CYAN);
    tft.drawCircle(120, 108, 43, BLUE);

    centerText(
      "8",
      117,
      &FreeSansBold12pt7b,
      WHITE
    );

    centerText(
      "THINKING...",
      175,
      &FreeSans9pt7b,
      CYAN
    );

    delay(220);
  }
}

// ======================================
// MAGIC 8 BALL ANSWER
// ======================================

void showAnswer(const char* answer) {

  tft.fillScreen(BG);

  tft.drawCircle(120, 108, 63, CYAN);
  tft.drawCircle(120, 108, 57, BLUE);

  tft.fillRoundRect(
    48, 67,
    144, 83,
    10,
    BLUE
  );

  String remaining = answer;
  String line = "";

  int y = 98;
  int lines = 0;

  while (
    remaining.length() > 0 &&
    lines < 3
  ) {

    int split = remaining.indexOf(' ');

    String word;

    if (split == -1) {
      word = remaining;
      remaining = "";
    } else {
      word = remaining.substring(0, split);
      remaining = remaining.substring(split + 1);
    }

    String testLine =
      line.length() == 0
      ? word
      : line + " " + word;

    tft.setFont(&FreeSans9pt7b);

    int16_t x1, y1;
    uint16_t w, h;

    tft.getTextBounds(
      testLine.c_str(),
      0, 0,
      &x1, &y1,
      &w, &h
    );

    if (w > 124 && line.length() > 0) {

      centerText(
        line.c_str(),
        y,
        &FreeSans9pt7b,
        WHITE
      );

      y += 19;
      lines++;

      line = word;

    } else {

      line = testLine;
    }
  }

  if (line.length() > 0 && lines < 3) {

    centerText(
      line.c_str(),
      y,
      &FreeSans9pt7b,
      WHITE
    );
  }

  centerText(
    "SELECT: ASK AGAIN",
    184,
    &FreeSans9pt7b,
    CYAN
  );
}

// ======================================
// PLACEHOLDER APPS
// ======================================

void drawPlaceholder(
  const char* title,
  const char* subtitle
) {

  tft.fillScreen(BG);

  centerText(
    title,
    98,
    &FreeSansBold12pt7b,
    CYAN
  );

  centerText(
    subtitle,
    130,
    &FreeSans9pt7b,
    WHITE
  );

  centerText(
    "IN DEVELOPMENT",
    160,
    &FreeSans9pt7b,
    PURPLE
  );
}

// ======================================
// OPEN APP
// ======================================

void openApp() {

  screen = APP;
  lastInteraction = millis();

  switch (appIndex) {

    case 0:
      drawMagicScreen();
      break;

    case 1:
      chess.resetExit();
      chess.init();
      break;

    case 2:
      snake.init();
      break;

    case 3:
      tetris.init();
      break;

    case 4:
      simon.resetExit();
      simon.init();
      break;

    case 5:
      drawPlaceholder("REMINDERS", "DAILY TASKS");
      break;
  }
}

// ======================================
// NAVIGATION
// ======================================

void goHome() {

  screen = HOME;
  animating = false;

  lastInteraction = millis();

  drawHome();
}

void goLauncher() {

  screen = LAUNCHER;
  animating = false;

  lastInteraction = millis();

  drawLauncher();
}

// ======================================
// WIFI
// ======================================

void connectWiFi() {

  Serial.println("Starting WiFi...");

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 12000
  ) {

    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());

    configTime(
      GMT_OFFSET,
      DAYLIGHT_OFFSET,
      "pool.ntp.org",
      "time.google.com"
    );

  } else {

    Serial.println("WiFi connection failed.");
  }

  lastWiFiAttempt = millis();
}

void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (
    millis() - lastWiFiAttempt >=
    WIFI_RETRY
  ) {

    lastWiFiAttempt = millis();

    Serial.println("Reconnecting WiFi...");

    WiFi.reconnect();
  }
}

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(115200);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  Serial.println("Starting COSMO...");

  tft.begin();
  tft.setRotation(2);
  tft.setTextWrap(false);

  tft.fillScreen(BG);

  // Check that the canvas successfully allocated memory.
  canvasAvailable =
    carouselCanvas.getBuffer() != nullptr;

  if (!canvasAvailable) {

    Serial.println(
      "ERROR: Carousel canvas allocation failed!"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Carousel canvas ready.");

  if (!LittleFS.begin(true)) {
    Serial.println("[STORAGE] LittleFS mount failed, formatting...");
  } else {
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("[STORAGE] LittleFS mount SUCCESS. Total: %u KB, Used: %u KB\n",
                  (unsigned int)(totalBytes / 1024), (unsigned int)(usedBytes / 1024));
  }

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutput);

  randomSeed(esp_random());

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[NTP] Waiting for clock synchronization...");
    unsigned long ntpStart = millis();
    while (time(nullptr) < 1700000000 && millis() - ntpStart < 2500) {
      delay(100);
    }
    if (time(nullptr) >= 1700000000) {
      Serial.println("[NTP] Clock synchronized successfully.");
    } else {
      Serial.println("[NTP] Clock sync timed out; continuing (TLS fallback active).");
    }

    // Check remote wallpaper for any cloud updates
    checkAndUpdateWallpaper(COSMO_WALLPAPER_URL, WALLPAPER_CACHE_PATH);
  }

  lastInteraction = millis();

  drawHome();

  Serial.println("COSMO READY!");
}

// ======================================
// MAIN LOOP
// ======================================

void loop() {

  maintainWiFi();

  updateButtons();

  // ==================================
  // RETURN HOME / EXIT APP: BOTH BUTTONS
  // ==================================

  if (screen == LAUNCHER && bothHeld()) {
    waitForButtonRelease();
    goHome();
    return;
  }

  if (screen == APP) {
    if (appIndex == 4) {
      // Simon Says: ONLY exit on Game Over screen if Left + Right is pressed
      if (simon.wantsToExit()) {
        simon.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
    } else {
      // Chess (1), Magic 8 Ball (0), Snake (2), Tetris (3), Reminders (5)
      if (bothHeld()) {
        if (appIndex == 1) chess.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (appIndex == 1 && chess.wantsToExit()) {
        chess.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
    }
  }

  // ==================================
  // HOME
  // ==================================

  if (screen == HOME) {

    static unsigned long lastWallpaperCheck = 0;
    if (WiFi.status() == WL_CONNECTED && (millis() - lastWallpaperCheck >= 30000 || !wallpaperLoaded)) {
      lastWallpaperCheck = millis();
      if (checkAndUpdateWallpaper(COSMO_WALLPAPER_URL, WALLPAPER_CACHE_PATH)) {
        drawHome();
        return;
      }
    }

    if (selectPressed) {

      goLauncher();
      return;
    }

    if (
      millis() - lastClockUpdate >= 1000
    ) {

      drawClock();
    }

    return;
  }

  // ==================================
  // LAUNCHER
  // ==================================

  if (screen == LAUNCHER) {

    if (animating) {

      updateCarouselAnimation();
      return;
    }

    if (leftPressed) {

      moveCarousel(-1);
      return;
    }

    if (rightPressed) {

      moveCarousel(1);
      return;
    }

    if (selectPressed) {

      lastInteraction = millis();

      openApp();
      return;
    }
  }

  // ==================================
  // APPS
  // ==================================

  if (screen == APP) {

    // Magic 8 Ball (0)
    if (
      appIndex == 0 &&
      selectPressed
    ) {

      lastInteraction = millis();

      thinkingAnimation();

      int index = random(ANSWER_COUNT);

      showAnswer(answers[index]);

      return;
    }

    // Chess (1)
    if (appIndex == 1) {
      bool leftAct = leftPressed || leftRepeated;
      bool rightAct = rightPressed || rightRepeated;
      chess.update(leftAct, rightAct, selectPressed);
      if (chess.wantsToExit()) {
        chess.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (leftAct || rightAct || selectPressed || chess.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Snake (2)
    if (appIndex == 2) {
      snake.update(leftPressed, rightPressed, selectPressed);
      if (leftPressed || rightPressed || selectPressed || snake.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Tetris (3)
    if (appIndex == 3) {
      tetris.update(leftPressed, rightPressed, selectPressed);
      if (leftPressed || rightPressed || selectPressed || tetris.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Simon Says (4)
    if (appIndex == 4) {
      simon.update(leftPressed, rightPressed, selectPressed);
      if (simon.wantsToExit()) {
        simon.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (leftPressed || rightPressed || selectPressed || simon.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Placeholder: Reminders (5)
    if (appIndex == 5) {
      if (
        leftPressed ||
        rightPressed ||
        selectPressed
      ) {
        lastInteraction = millis();
      }
    }
  }

  // ==================================
  // IDLE TIMEOUT
  // ==================================

  bool inActiveGame = (screen == APP && (
    (appIndex == 1 && chess.isActive()) ||
    (appIndex == 2 && snake.isActive()) ||
    (appIndex == 3 && tetris.isActive()) ||
    (appIndex == 4 && simon.isActive())
  ));

  if (
    screen != HOME &&
    !inActiveGame &&
    millis() - lastInteraction >= IDLE_TIMEOUT
  ) {

    goHome();
  }
}