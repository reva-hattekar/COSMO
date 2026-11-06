#ifndef CHESS_APP_H
#define CHESS_APP_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "ChessEngine.h"

enum ChessAppState {
  CHESS_SELECT_PIECE,
  CHESS_SELECT_DEST,
  CHESS_AI_THINKING,
  CHESS_AI_PREVIEW,
  CHESS_MENU,
  CHESS_GAME_OVER
};

class ChessApp {
public:
  ChessApp(Adafruit_GC9A01A& display);

  void init();
  void update(bool left, bool right, bool select);
  bool isActive() const;
  bool wantsToExit() const;
  void resetExit();

private:
  Adafruit_GC9A01A& tft;
  ChessEngine engine;

  ChessAppState state;
  int8_t cursorSq;
  int8_t selectedSq;

  // Legal destinations for selected piece
  ChessMove currentMoves[32];
  int currentMoveCount;
  int currentMoveIndex;

  // AI preview tracking
  ChessMove pendingAIMove;
  unsigned long aiPreviewStart;

  // Move tracking for visual feedback
  int8_t lastMoveFrom;
  int8_t lastMoveTo;

  // Settings
  bool modeVsAI; // true = PvC, false = 2-player local
  AIDifficulty aiDifficulty;
  GameStatus gameStatus;

  // Menu navigation
  int menuIndex;
  static const int MENU_ITEM_COUNT = 6;

  bool exitRequested;

  // UI rendering
  void renderAll();
  void drawBoard();
  void drawSquare(int8_t sq);
  void drawPiece(int8_t sq, int8_t piece);
  void drawTopStatus();
  void drawBottomBar();
  void drawMenu();

  // Helper conversions
  int getSquareX(int8_t sq) const;
  int getSquareY(int8_t sq) const;
  void centerText(const char* text, int y, const GFXfont* font, uint16_t color);
  void squareToAlgebraic(int8_t sq, char* out) const;
};

#endif // CHESS_APP_H
