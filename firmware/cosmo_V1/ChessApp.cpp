#include "ChessApp.h"

// Colors
#define COL_BG          0x0000
#define COL_LIGHT_SQ    0xD69A
#define COL_DARK_SQ     0x31A6
#define COL_SELECTED    0x07FF
#define COL_CURSOR      0xFFE0
#define COL_LEGAL_DOT   0x07E0
#define COL_CAPTURE     0xF800
#define COL_CHECK       0xF800
#define COL_AI_FROM     0xFEA0
#define COL_AI_TO       0x07FF
#define COL_LAST_MOVE   0x4A69
#define COL_WHITE_PIECE 0xFFFF
#define COL_BLACK_PIECE 0x18C3
#define COL_TEXT_GREY   0x9CF3
#define COL_CARD_BG     0x0842
#define COL_CARD_BORDER 0x2104

// 12x12 Piece Bitmaps (12 rows of 12 bits stored as uint16_t)
static const uint16_t pawnBmp[12] = {
  0x0F00, 0x0F00, 0x0600, 0x0F00, 0x0F00, 0x0600, 0x0F00, 0x1F80, 0x1F80, 0x3FC0, 0x3FC0, 0x7FE0
};

static const uint16_t knightBmp[12] = {
  0x0F00, 0x1F80, 0x3FC0, 0x7FE0, 0xFFF0, 0xF3F0, 0x03F0, 0x07F0, 0x0FF0, 0x1FF0, 0x3FF0, 0x7FE0
};

static const uint16_t bishopBmp[12] = {
  0x0600, 0x0F00, 0x1F80, 0x33C0, 0x3FC0, 0x3FC0, 0x1F80, 0x0F00, 0x1F80, 0x3FC0, 0x7FE0, 0xFFF0
};

static const uint16_t rookBmp[12] = {
  0x9990, 0x9990, 0xFFF0, 0x7FE0, 0x3FC0, 0x3FC0, 0x3FC0, 0x3FC0, 0x7FE0, 0xFFF0, 0xFFF0, 0xFFF0
};

static const uint16_t queenBmp[12] = {
  0x9290, 0xDB90, 0x7FE0, 0x3FC0, 0x1F80, 0x3FC0, 0x7FE0, 0x3FC0, 0x1F80, 0x3FC0, 0x7FE0, 0xFFF0
};

static const uint16_t kingBmp[12] = {
  0x0600, 0x1F80, 0x0600, 0x3FC0, 0x7FE0, 0xFFF0, 0xFFF0, 0x7FE0, 0x3FC0, 0x7FE0, 0xFFF0, 0xFFF0
};

ChessApp::ChessApp(Adafruit_GC9A01A& display)
  : tft(display),
    state(CHESS_SELECT_PIECE),
    cursorSq(12), // Start at e2
    selectedSq(-1),
    currentMoveCount(0),
    currentMoveIndex(0),
    aiPreviewStart(0),
    lastMoveFrom(-1),
    lastMoveTo(-1),
    modeVsAI(true),
    aiDifficulty(DIFF_MEDIUM),
    gameStatus(STATUS_ACTIVE),
    menuIndex(0),
    exitRequested(false)
{
}

void ChessApp::init() {
  engine.reset();
  state = CHESS_SELECT_PIECE;
  cursorSq = 12; // Square e2
  selectedSq = -1;
  currentMoveCount = 0;
  currentMoveIndex = 0;
  aiPreviewStart = 0;
  lastMoveFrom = -1;
  lastMoveTo = -1;
  gameStatus = STATUS_ACTIVE;
  menuIndex = 0;
  exitRequested = false;

  renderAll();
}

bool ChessApp::isActive() const {
  return (state != CHESS_GAME_OVER);
}

bool ChessApp::wantsToExit() const {
  return exitRequested;
}

void ChessApp::resetExit() {
  exitRequested = false;
}

int ChessApp::getSquareX(int8_t sq) const {
  int file = sq % 8;
  return 44 + (file * 19);
}

int ChessApp::getSquareY(int8_t sq) const {
  int rank = sq / 8;
  // White is at bottom (rank 0 -> bottom), Black at top (rank 7 -> top)
  return 44 + ((7 - rank) * 19);
}

void ChessApp::squareToAlgebraic(int8_t sq, char* out) const {
  if (sq < 0 || sq >= 64) {
    out[0] = '-'; out[1] = '-'; out[2] = '\0';
    return;
  }
  out[0] = 'A' + (sq % 8);
  out[1] = '1' + (sq / 8);
  out[2] = '\0';
}

void ChessApp::centerText(const char* text, int y, const GFXfont* font, uint16_t color) {
  tft.setFont(font);
  tft.setTextColor(color);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, y);
  tft.print(text);
}

void ChessApp::drawPiece(int8_t sq, int8_t piece) {
  if (piece == 0) return;

  int x0 = getSquareX(sq) + 3;
  int y0 = getSquareY(sq) + 3;
  int pType = abs(piece);
  bool isWhite = (piece > 0);

  const uint16_t* bmp = nullptr;
  switch (pType) {
    case PIECE_PAWN:   bmp = pawnBmp;   break;
    case PIECE_KNIGHT: bmp = knightBmp; break;
    case PIECE_BISHOP: bmp = bishopBmp; break;
    case PIECE_ROOK:   bmp = rookBmp;   break;
    case PIECE_QUEEN:  bmp = queenBmp;  break;
    case PIECE_KING:   bmp = kingBmp;   break;
    default: return;
  }

  // Draw 12x12 piece sprite
  for (int r = 0; r < 12; r++) {
    uint16_t rowBits = bmp[r];
    for (int c = 0; c < 12; c++) {
      if ((rowBits >> (15 - c)) & 1) {
        if (isWhite) {
          tft.drawPixel(x0 + c, y0 + r, COL_WHITE_PIECE);
        } else {
          tft.drawPixel(x0 + c, y0 + r, COL_BLACK_PIECE);
        }
      }
    }
  }

  // Outline contrast border for black pieces so they pop on dark squares
  if (!isWhite) {
    for (int r = 0; r < 12; r++) {
      uint16_t rowBits = bmp[r];
      for (int c = 0; c < 12; c++) {
        if ((rowBits >> (15 - c)) & 1) {
          bool edge = false;
          if (r == 0 || !((bmp[r-1] >> (15 - c)) & 1)) edge = true;
          if (r == 11 || !((bmp[r+1] >> (15 - c)) & 1)) edge = true;
          if (c == 0 || !((rowBits >> (15 - (c-1))) & 1)) edge = true;
          if (c == 11 || !((rowBits >> (15 - (c+1))) & 1)) edge = true;
          if (edge) {
            tft.drawPixel(x0 + c, y0 + r, 0xFFFF);
          }
        }
      }
    }
  } else {
    // White piece subtle bottom/right border for depth
    for (int r = 0; r < 12; r++) {
      uint16_t rowBits = bmp[r];
      for (int c = 0; c < 12; c++) {
        if ((rowBits >> (15 - c)) & 1) {
          if (r == 11 || (r < 11 && !((bmp[r+1] >> (15 - c)) & 1))) {
            tft.drawPixel(x0 + c, y0 + r, 0x6B4D);
          }
        }
      }
    }
  }
}

void ChessApp::drawSquare(int8_t sq) {
  int x = getSquareX(sq);
  int y = getSquareY(sq);
  int rank = sq / 8;
  int file = sq % 8;

  bool isLight = ((rank + file) % 2) != 0;
  uint16_t baseColor = isLight ? COL_LIGHT_SQ : COL_DARK_SQ;

  // Selected square tint
  if (sq == selectedSq) {
    baseColor = 0x0215; // Dark cyan/blue
  }

  tft.fillRect(x, y, 19, 19, baseColor);

  // Subtle outline on previous move squares (from & to)
  if (sq == lastMoveFrom || sq == lastMoveTo) {
    tft.drawRect(x, y, 19, 19, COL_LAST_MOVE);
  }

  // Check if this square is King in check
  int8_t piece = engine.getPiece(sq);
  if (piece != 0 && abs(piece) == PIECE_KING && ((piece > 0) ? engine.isInCheck(COLOR_WHITE) : engine.isInCheck(COLOR_BLACK))) {
    tft.drawRect(x, y, 19, 19, COL_CHECK);
    tft.drawRect(x + 1, y + 1, 17, 17, COL_CHECK);
  }

  // Draw piece
  drawPiece(sq, piece);

  // Legal destination markers
  if (state == CHESS_SELECT_DEST) {
    for (int i = 0; i < currentMoveCount; i++) {
      if (currentMoves[i].to == sq) {
        if (piece == 0) {
          tft.fillCircle(x + 9, y + 9, 3, COL_LEGAL_DOT);
        } else {
          // Capture target: red corner brackets
          tft.drawRect(x, y, 19, 19, COL_CAPTURE);
          tft.drawRect(x + 1, y + 1, 17, 17, COL_CAPTURE);
        }
        break;
      }
    }
  }

  // AI Move Preview Highlight
  if (state == CHESS_AI_PREVIEW) {
    if (sq == pendingAIMove.from) {
      tft.drawRect(x, y, 19, 19, COL_AI_FROM);
      tft.drawRect(x + 1, y + 1, 17, 17, COL_AI_FROM);
    } else if (sq == pendingAIMove.to) {
      tft.drawRect(x, y, 19, 19, COL_AI_TO);
      tft.drawRect(x + 1, y + 1, 17, 17, COL_AI_TO);
    }
  }

  // Highlight selected piece square
  if (sq == selectedSq) {
    tft.drawRect(x, y, 19, 19, COL_SELECTED);
    tft.drawRect(x + 1, y + 1, 17, 17, COL_SELECTED);
  }

  // Highlight cursor square
  if (sq == cursorSq) {
    tft.drawRect(x, y, 19, 19, COL_CURSOR);
    tft.drawRect(x + 1, y + 1, 17, 17, COL_CURSOR);
  }
}

void ChessApp::drawBoard() {
  tft.drawRect(42, 42, 156, 156, COL_CARD_BORDER);

  for (int8_t sq = 0; sq < 64; sq++) {
    drawSquare(sq);
  }
}

void ChessApp::drawTopStatus() {
  // Top pill: X=58, Y=18, W=124, H=20 (Fully inside circle)
  tft.fillRoundRect(58, 18, 124, 20, 5, COL_CARD_BG);
  tft.drawRoundRect(58, 18, 124, 20, 5, COL_CARD_BORDER);

  char msg[24];
  uint16_t col = 0xFFFF;

  if (state == CHESS_AI_THINKING) {
    snprintf(msg, sizeof(msg), "AI THINKING");
    col = 0x07FF;
  } else if (state == CHESS_AI_PREVIEW) {
    char fAlg[3], tAlg[3];
    squareToAlgebraic(pendingAIMove.from, fAlg);
    squareToAlgebraic(pendingAIMove.to, tAlg);
    snprintf(msg, sizeof(msg), "AI: %s->%s", fAlg, tAlg);
    col = COL_AI_FROM;
  } else if (gameStatus == STATUS_CHECKMATE) {
    snprintf(msg, sizeof(msg), "CHECKMATE");
    col = 0xF800;
  } else if (gameStatus == STATUS_STALEMATE) {
    snprintf(msg, sizeof(msg), "STALEMATE");
    col = 0xFFE0;
  } else if (gameStatus == STATUS_DRAW) {
    snprintf(msg, sizeof(msg), "DRAW");
    col = 0xFFE0;
  } else if (gameStatus == STATUS_CHECK) {
    snprintf(msg, sizeof(msg), "CHECK!");
    col = 0xF800;
  } else {
    if (engine.getTurn() == COLOR_WHITE) {
      snprintf(msg, sizeof(msg), "WHITE TURN");
      col = 0xFFFF;
    } else {
      snprintf(msg, sizeof(msg), modeVsAI ? "COSMO TURN" : "BLACK TURN");
      col = 0x9CF3;
    }
  }

  centerText(msg, 33, &FreeSansBold9pt7b, col);
}

void ChessApp::drawBottomBar() {
  // Bottom pill: X=58, Y=202, W=124, H=20 (Fully inside circle)
  tft.fillRoundRect(58, 202, 124, 20, 5, COL_CARD_BG);
  tft.drawRoundRect(58, 202, 124, 20, 5, COL_CARD_BORDER);

  char buf[24];
  uint16_t col = COL_TEXT_GREY;

  if (state == CHESS_SELECT_DEST) {
    snprintf(buf, sizeof(buf), "SELECT MOVE");
    col = 0x07FF;
  } else if (state == CHESS_AI_PREVIEW) {
    snprintf(buf, sizeof(buf), "COSMO MOVE");
    col = COL_AI_FROM;
  } else if (state == CHESS_GAME_OVER) {
    snprintf(buf, sizeof(buf), "GAME OVER");
    col = 0xF800;
  } else {
    if (modeVsAI) {
      snprintf(buf, sizeof(buf), "P1 vs COSMO");
    } else {
      snprintf(buf, sizeof(buf), "2-PLAYER");
    }
  }

  centerText(buf, 217, &FreeSans9pt7b, col);
}

void ChessApp::drawMenu() {
  // Centered modal: X=42, Y=38, W=156, H=164 (Fully inside circle)
  tft.fillRoundRect(42, 38, 156, 164, 8, 0x0842);
  tft.drawRoundRect(42, 38, 156, 164, 8, 0x07FF);

  centerText("CHESS MENU", 56, &FreeSansBold9pt7b, 0x07FF);

  const char* items[MENU_ITEM_COUNT];
  items[0] = "RESUME";
  items[1] = "NEW GAME";

  char modeBuf[20];
  snprintf(modeBuf, sizeof(modeBuf), "%s", modeVsAI ? "MODE: PvAI" : "MODE: 2P");
  items[2] = modeBuf;

  char diffBuf[20];
  const char* diffName = (aiDifficulty == DIFF_EASY) ? "EASY" : ((aiDifficulty == DIFF_MEDIUM) ? "MED" : "HARD");
  snprintf(diffBuf, sizeof(diffBuf), "AI: %s", diffName);
  items[3] = diffBuf;

  items[4] = "UNDO MOVE";
  items[5] = "EXIT TO HOME";

  int startY = 80;
  int lineSpacing = 18;

  for (int i = 0; i < MENU_ITEM_COUNT; i++) {
    int y = startY + (i * lineSpacing);
    bool isSel = (i == menuIndex);

    if (isSel) {
      tft.fillRoundRect(48, y - 13, 144, 16, 4, 0x2104);
      centerText(items[i], y, &FreeSans9pt7b, 0xFFE0);
    } else {
      centerText(items[i], y, &FreeSans9pt7b, 0xFFFF);
    }
  }
}

void ChessApp::renderAll() {
  tft.fillScreen(COL_BG);
  drawBoard();
  drawTopStatus();
  drawBottomBar();

  if (state == CHESS_MENU) {
    drawMenu();
  }
}

void ChessApp::update(bool left, bool right, bool select) {
  // 1. In-Game Menu State
  if (state == CHESS_MENU) {
    if (left) {
      menuIndex = (menuIndex + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
      drawMenu();
    } else if (right) {
      menuIndex = (menuIndex + 1) % MENU_ITEM_COUNT;
      drawMenu();
    } else if (select) {
      switch (menuIndex) {
        case 0: // RESUME
          state = CHESS_SELECT_PIECE;
          renderAll();
          break;
        case 1: // NEW GAME
          init();
          break;
        case 2: // MODE TOGGLE
          modeVsAI = !modeVsAI;
          drawMenu();
          break;
        case 3: // DIFFICULTY
          aiDifficulty = (AIDifficulty)((aiDifficulty + 1) % 3);
          drawMenu();
          break;
        case 4: // UNDO MOVE
          if (engine.canUndo()) {
            engine.undoMove();
            if (modeVsAI && engine.canUndo()) {
              engine.undoMove();
            }
            selectedSq = -1;
            lastMoveFrom = -1;
            lastMoveTo = -1;
            state = CHESS_SELECT_PIECE;
            gameStatus = engine.evaluateGameStatus();
            renderAll();
          }
          break;
        case 5: // EXIT TO HOME
          exitRequested = true;
          break;
      }
    }
    return;
  }

  // 2. Game Over State Handling
  if (state == CHESS_GAME_OVER) {
    if (select) {
      state = CHESS_MENU;
      menuIndex = 1; // Default to "NEW GAME"
      drawMenu();
    }
    return;
  }

  // 3. AI Thinking State (Calculates move, enters Preview)
  if (state == CHESS_AI_THINKING) {
    if (engine.computeAIMove(aiDifficulty, pendingAIMove)) {
      aiPreviewStart = millis();
      state = CHESS_AI_PREVIEW;
      renderAll(); // Shows "AI: G8->F6" and highlights source & target squares
    } else {
      gameStatus = engine.evaluateGameStatus();
      state = CHESS_GAME_OVER;
      renderAll();
    }
    return;
  }

  // 4. AI Preview State (Shows intended move for ~950ms before moving)
  if (state == CHESS_AI_PREVIEW) {
    if (millis() - aiPreviewStart >= 950) {
      engine.executeMove(pendingAIMove);
      lastMoveFrom = pendingAIMove.from;
      lastMoveTo = pendingAIMove.to;
      gameStatus = engine.evaluateGameStatus();

      if (gameStatus == STATUS_CHECKMATE || gameStatus == STATUS_STALEMATE || gameStatus == STATUS_DRAW) {
        state = CHESS_GAME_OVER;
      } else {
        state = CHESS_SELECT_PIECE;
      }
      renderAll();
    }
    return;
  }

  // 5. Piece Selection State
  if (state == CHESS_SELECT_PIECE) {
    int8_t prevSq = cursorSq;

    if (left) {
      cursorSq = (cursorSq + 63) % 64;
    } else if (right) {
      cursorSq = (cursorSq + 1) % 64;
    }

    if (cursorSq != prevSq) {
      drawSquare(prevSq);
      drawSquare(cursorSq);
    }

    if (select) {
      int8_t p = engine.getPiece(cursorSq);
      // Can only select own piece
      if (p != 0 && ((p > 0) == (engine.getTurn() == COLOR_WHITE))) {
        currentMoveCount = engine.generateLegalMoves(cursorSq, currentMoves, 32);
        if (currentMoveCount > 0) {
          selectedSq = cursorSq;
          currentMoveIndex = 0;
          cursorSq = currentMoves[0].to;
          state = CHESS_SELECT_DEST;
          renderAll();
          return;
        }
      }
    }
  }

  // 6. Destination Selection State
  else if (state == CHESS_SELECT_DEST) {
    int8_t prevSq = cursorSq;
    int totalChoices = currentMoveCount + 1; // last choice is cancel (returns to selected piece)

    if (left) {
      currentMoveIndex = (currentMoveIndex + totalChoices - 1) % totalChoices;
    } else if (right) {
      currentMoveIndex = (currentMoveIndex + 1) % totalChoices;
    }

    if (currentMoveIndex < currentMoveCount) {
      cursorSq = currentMoves[currentMoveIndex].to;
    } else {
      cursorSq = selectedSq; // Cancel option
    }

    if (cursorSq != prevSq) {
      drawSquare(prevSq);
      drawSquare(cursorSq);
    }

    if (select) {
      if (cursorSq == selectedSq || currentMoveIndex == currentMoveCount) {
        // Cancel selection
        selectedSq = -1;
        state = CHESS_SELECT_PIECE;
        renderAll();
        return;
      }

      // Execute selected move
      ChessMove chosenMove = currentMoves[currentMoveIndex];
      engine.executeMove(chosenMove);

      lastMoveFrom = chosenMove.from;
      lastMoveTo = chosenMove.to;
      selectedSq = -1;
      gameStatus = engine.evaluateGameStatus();

      if (gameStatus == STATUS_CHECKMATE || gameStatus == STATUS_STALEMATE || gameStatus == STATUS_DRAW) {
        state = CHESS_GAME_OVER;
        renderAll();
      } else {
        if (modeVsAI && engine.getTurn() == COLOR_BLACK) {
          state = CHESS_AI_THINKING;
          renderAll(); // Shows "AI THINKING" immediately
        } else {
          state = CHESS_SELECT_PIECE;
          renderAll();
        }
      }
    }
  }
}
