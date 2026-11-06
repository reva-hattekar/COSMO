#include "ChessApp.h"

// Colors
#define COL_BG          0x0000
#define COL_LIGHT_SQ    0xD69A
#define COL_DARK_SQ     0x31A6
#define COL_SELECTED    0x07FF
#define COL_CURSOR      0xFFE0
#define COL_LEGAL_DOT   0x07E0
#define COL_CHECK       0xF800
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
    modeVsAI(true),
    aiDifficulty(DIFF_MEDIUM),
    gameStatus(STATUS_ACTIVE),
    menuIndex(0),
    exitRequested(false),
    boardDirty(true),
    aiNeedsToMove(false),
    lastAITriggerTime(0)
{
}

void ChessApp::init() {
  engine.reset();
  state = CHESS_SELECT_PIECE;
  cursorSq = 12; // Square e2
  selectedSq = -1;
  currentMoveCount = 0;
  currentMoveIndex = 0;
  gameStatus = STATUS_ACTIVE;
  menuIndex = 0;
  exitRequested = false;
  boardDirty = true;
  aiNeedsToMove = false;
  lastAITriggerTime = 0;

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
  // For White piece: Crisp White body with dark border shadow
  // For Black piece: Dark charcoal body with crisp White/Cyan outline
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
          // Check neighbors
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
    baseColor = 0x0315; // Dark cyan/blue
  }

  tft.fillRect(x, y, 19, 19, baseColor);

  // Check if this square is attacked king in check
  int8_t piece = engine.getPiece(sq);
  if (piece != 0 && abs(piece) == PIECE_KING && ((piece > 0) ? engine.isInCheck(COLOR_WHITE) : engine.isInCheck(COLOR_BLACK))) {
    tft.drawRect(x, y, 19, 19, COL_CHECK);
    tft.drawRect(x + 1, y + 1, 17, 17, COL_CHECK);
  }

  // Draw piece
  drawPiece(sq, piece);

  // Legal destination dot if in target select mode
  if (state == CHESS_SELECT_DEST) {
    for (int i = 0; i < currentMoveCount; i++) {
      if (currentMoves[i].to == sq) {
        if (piece == 0) {
          tft.fillCircle(x + 9, y + 9, 3, COL_LEGAL_DOT);
        } else {
          // Capture target: draw 4 corner markers
          tft.drawRect(x, y, 19, 19, COL_LEGAL_DOT);
          tft.drawRect(x + 1, y + 1, 17, 17, COL_LEGAL_DOT);
        }
        break;
      }
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
  // Board outer border
  tft.drawRect(42, 42, 156, 156, COL_CARD_BORDER);

  for (int8_t sq = 0; sq < 64; sq++) {
    drawSquare(sq);
  }
}

void ChessApp::drawTopStatus() {
  tft.fillRoundRect(25, 14, 190, 24, 6, COL_CARD_BG);
  tft.drawRoundRect(25, 14, 190, 24, 6, COL_CARD_BORDER);

  const char* msg = "WHITE'S TURN";
  uint16_t col = 0xFFFF;

  if (state == CHESS_AI_THINKING) {
    msg = "COSMO THINKING...";
    col = 0x07FF;
  } else if (gameStatus == STATUS_CHECKMATE) {
    msg = (engine.getTurn() == COLOR_WHITE) ? "CHECKMATE! BLACK WINS" : "CHECKMATE! WHITE WINS";
    col = 0xF800;
  } else if (gameStatus == STATUS_STALEMATE) {
    msg = "STALEMATE - DRAW";
    col = 0xFFE0;
  } else if (gameStatus == STATUS_DRAW) {
    msg = "50-MOVE DRAW";
    col = 0xFFE0;
  } else if (gameStatus == STATUS_CHECK) {
    msg = (engine.getTurn() == COLOR_WHITE) ? "WHITE IN CHECK!" : "BLACK IN CHECK!";
    col = 0xF800;
  } else {
    if (engine.getTurn() == COLOR_BLACK) {
      msg = modeVsAI ? "COSMO'S TURN" : "BLACK'S TURN";
      col = 0x9CF3;
    }
  }

  centerText(msg, 31, &FreeSansBold9pt7b, col);
}

void ChessApp::drawBottomBar() {
  tft.fillRoundRect(35, 202, 170, 24, 6, COL_CARD_BG);
  tft.drawRoundRect(35, 202, 170, 24, 6, COL_CARD_BORDER);

  char buf[32];
  if (state == CHESS_SELECT_DEST) {
    snprintf(buf, sizeof(buf), "SEL:MOVE  L+R:CANCEL");
  } else if (state == CHESS_GAME_OVER) {
    snprintf(buf, sizeof(buf), "SELECT: MENU");
  } else {
    if (modeVsAI) {
      const char* diffStr = (aiDifficulty == DIFF_EASY) ? "EASY" : ((aiDifficulty == DIFF_MEDIUM) ? "MED" : "HARD");
      snprintf(buf, sizeof(buf), "PvAI [%s]  L+R:MENU", diffStr);
    } else {
      snprintf(buf, sizeof(buf), "2-PLAYER   L+R:MENU");
    }
  }

  centerText(buf, 219, &FreeSans9pt7b, COL_TEXT_GREY);
}

void ChessApp::drawMenu() {
  // Draw semi-transparent modal card
  tft.fillRoundRect(30, 36, 180, 168, 8, 0x0842);
  tft.drawRoundRect(30, 36, 180, 168, 8, 0x07FF);

  centerText("CHESS MENU", 56, &FreeSansBold9pt7b, 0x07FF);

  const char* items[MENU_ITEM_COUNT];
  items[0] = "RESUME";
  items[1] = "NEW GAME";

  char modeBuf[24];
  snprintf(modeBuf, sizeof(modeBuf), "MODE: %s", modeVsAI ? "PvAI" : "2P LOCAL");
  items[2] = modeBuf;

  char diffBuf[24];
  const char* diffName = (aiDifficulty == DIFF_EASY) ? "EASY" : ((aiDifficulty == DIFF_MEDIUM) ? "MEDIUM" : "HARD");
  snprintf(diffBuf, sizeof(diffBuf), "AI: %s", diffName);
  items[3] = diffBuf;

  items[4] = "UNDO MOVE";
  items[5] = "EXIT TO HOME";

  int startY = 82;
  int lineSpacing = 18;

  for (int i = 0; i < MENU_ITEM_COUNT; i++) {
    int y = startY + (i * lineSpacing);
    bool isSel = (i == menuIndex);

    if (isSel) {
      tft.fillRoundRect(36, y - 13, 168, 16, 4, 0x2104);
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
  // 1. Both buttons held / chord -> Toggle In-Game Menu
  if (left && right) {
    if (state != CHESS_MENU) {
      state = CHESS_MENU;
      menuIndex = 0;
      drawMenu();
    } else {
      state = CHESS_SELECT_PIECE;
      renderAll();
    }
    return;
  }

  // 2. Menu State Handling
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
              // In PvAI mode, undo both AI and player move to return turn to player
              engine.undoMove();
            }
            selectedSq = -1;
            state = CHESS_SELECT_PIECE;
            gameStatus = engine.evaluateGameStatus();
            renderAll();
          }
          break;
        case 5: // EXIT
          exitRequested = true;
          break;
      }
    }
    return;
  }

  // 3. Game Over State Handling
  if (state == CHESS_GAME_OVER) {
    if (select || (left && right)) {
      state = CHESS_MENU;
      menuIndex = 1; // Default to "NEW GAME"
      drawMenu();
    }
    return;
  }

  // 4. AI Thinking Execution
  if (state == CHESS_AI_THINKING) {
    ChessMove aiMove;
    if (engine.computeAIMove(aiDifficulty, aiMove)) {
      engine.executeMove(aiMove);
    }
    gameStatus = engine.evaluateGameStatus();
    if (gameStatus == STATUS_CHECKMATE || gameStatus == STATUS_STALEMATE || gameStatus == STATUS_DRAW) {
      state = CHESS_GAME_OVER;
    } else {
      state = CHESS_SELECT_PIECE;
    }
    renderAll();
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

    // Left/Right cycles through legal destinations + cancel square (selectedSq)
    int totalChoices = currentMoveCount + 1; // last choice is cancel

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

      // Execute selected legal move
      ChessMove chosenMove = currentMoves[currentMoveIndex];
      engine.executeMove(chosenMove);

      selectedSq = -1;
      gameStatus = engine.evaluateGameStatus();

      if (gameStatus == STATUS_CHECKMATE || gameStatus == STATUS_STALEMATE || gameStatus == STATUS_DRAW) {
        state = CHESS_GAME_OVER;
        renderAll();
      } else {
        if (modeVsAI && engine.getTurn() == COLOR_BLACK) {
          state = CHESS_AI_THINKING;
          renderAll(); // Shows "COSMO THINKING..."
        } else {
          state = CHESS_SELECT_PIECE;
          renderAll();
        }
      }
    }
  }
}

