#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

class TetrisGame {
public:
  enum State {
    PLAYING,
    GAME_OVER
  };

  TetrisGame(Adafruit_GC9A01A& display);
  void init();
  void update(bool left, bool right, bool select);
  void render();
  bool isActive() const { return state == PLAYING; }

private:
  Adafruit_GC9A01A& tft;

  static const int BOARD_W = 10;
  static const int BOARD_H = 20;
  static const int CELL_SIZE = 8;
  static const int BOARD_X = 80;
  static const int BOARD_Y = 40;

  uint16_t board[BOARD_H][BOARD_W]; // 0 = empty, else 16-bit color

  int currentPiece;
  int currentRotation;
  int pieceX;
  int pieceY;

  int nextPiece;

  int score;
  int linesCleared;
  int level;

  unsigned long dropInterval;
  unsigned long lastDropTime;
  unsigned long selectPressStartTime;
  bool selectIsHeld;
  bool hardDropTriggered;

  State state;

  void spawnNewPiece();
  bool checkCollision(int piece, int rotation, int px, int py);
  void lockPiece();
  void checkLines();
  void drawBlock(int bx, int by, uint16_t color);
  void drawPiece(int piece, int rotation, int px, int py, uint16_t color);
  void drawBoard();
  void drawSidebar();
  void drawNextPiecePreview();
  void drawGameOver();
};

#endif

