#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

class SnakeGame {
public:
  enum State {
    PLAYING,
    PAUSED,
    GAME_OVER
  };

  SnakeGame(Adafruit_GC9A01A& display);
  void init();
  void update(bool left, bool right, bool select);
  void render();
  bool isActive() const { return state == PLAYING; }

private:
  Adafruit_GC9A01A& tft;

  static const int GRID_W = 20;
  static const int GRID_H = 20;
  static const int CELL_SIZE = 8;
  static const int GRID_X = 40;
  static const int GRID_Y = 40;
  static const int MAX_LENGTH = 180;

  int8_t snakeX[MAX_LENGTH];
  int8_t snakeY[MAX_LENGTH];
  int snakeLength;

  int8_t dir; // 0=Up, 1=Right, 2=Down, 3=Left
  int8_t foodX;
  int8_t foodY;

  int score;
  int highScore;
  unsigned long moveInterval;
  unsigned long lastMoveTime;

  State state;
  bool needsFullRedraw;

  void spawnFood();
  void drawCell(int gx, int gy, uint16_t color);
  void drawFood();
  void drawScore();
  void drawPauseOverlay();
  void drawGameOver();
};

#endif

