#ifndef SIMON_GAME_H
#define SIMON_GAME_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

class SimonGame {
public:
  enum State {
    STARTING,
    PLAYING_SEQUENCE,
    WAITING_PLAYER,
    CORRECT_FEEDBACK,
    WRONG_FEEDBACK,
    GAME_OVER
  };
  bool wantsToExit() const {
    return exitRequested;
  }
  void resetExit() {
    exitRequested = false;
  }
  bool isGameOver() const {
    return state == GAME_OVER;
  }

  SimonGame(Adafruit_GC9A01A& display);
  void init();
  void update(bool left, bool right, bool select);
  void render();
  bool isActive() const { return state != GAME_OVER; }

private:
  Adafruit_GC9A01A& tft;

  static const int MAX_STEPS = 64;
  uint8_t sequence[MAX_STEPS];
  int currentLevel;
  int playerStep;
  int bestScore;

  State state;

  int activeFlashColor; // -1 = none, 0..3 = flashing quadrant
  unsigned long stateTimer;
  int sequenceStep;

  int pendingInput;
  unsigned long pendingInputTime;

  bool exitRequested = false;


  void drawPad(int index, bool highlighted);
  void drawCenterConsole();
  void drawGameOver();
  void playFlash(int color);
  void checkPlayerInput(int color);
};

#endif

