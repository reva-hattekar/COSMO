#ifndef AKINATOR_APP_H
#define AKINATOR_APP_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

class AkinatorApp {
public:
  enum State {
    STATE_IDLE,
    STATE_LOADING,
    STATE_QUESTION,
    STATE_GUESS,
    STATE_WON,
    STATE_LOST,
    STATE_ERROR
  };

  AkinatorApp(Adafruit_GC9A01A& display);
  void init(const char* serverUrl);
  void update(bool left, bool right, bool select);
  void render();
  bool isActive() const {
    return state == STATE_QUESTION || state == STATE_LOADING || state == STATE_GUESS;
  }
  void reset();

private:
  Adafruit_GC9A01A& tft;
  String serverBaseUrl;
  String sessionId;
  String currentQuestion;
  String guessName;
  String guessDesc;
  int currentStep;
  float progressPct;
  int selectedAnswer;
  State state;
  String errorMessage;

  void startGame();
  void sendAnswer(int answerIdx);
  void sendContinue();
  void sendWin();
  void parseGameResponse(const String& json);
  void drawQuestionScreen();
  void drawGuessScreen();
  void drawStatusScreen(const char* title, const char* msg, uint16_t color, const char* actionMsg = nullptr);
  void drawCenteredWrapped(const char* text, int startY, int maxLines, int lineSpacing, const GFXfont* font, uint16_t color);
};

#endif

