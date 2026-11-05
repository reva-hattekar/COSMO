#include "TetrisGame.h"
#include <Fonts/FreeSans9pt7b.h>

#define COL_BG       0x0000
#define COL_WHITE    0xFFFF
#define COL_CYAN     0x07FF
#define COL_BLUE     0x001F
#define COL_ORANGE   0xFD20
#define COL_YELLOW   0xFFE0
#define COL_GREEN    0x07E0
#define COL_PURPLE   0x801F
#define COL_RED      0xF800
#define COL_GREY     0x8410
#define COL_DARKGREY 0x4208

#define BTN_SELECT   14

static const uint16_t PIECE_COLORS[7] = {
  COL_CYAN,   // 0: I
  COL_BLUE,   // 1: J
  COL_ORANGE, // 2: L
  COL_YELLOW, // 3: O
  COL_GREEN,  // 4: S
  COL_PURPLE, // 5: T
  COL_RED     // 6: Z
};

static const int8_t PIECE_SHAPES[7][4][4][2] = {
  // 0: I
  {
    {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
    {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
    {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
    {{1, 0}, {1, 1}, {1, 2}, {1, 3}}
  },
  // 1: J
  {
    {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
    {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
    {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
    {{1, 0}, {1, 1}, {0, 2}, {1, 2}}
  },
  // 2: L
  {
    {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
    {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
    {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
    {{0, 0}, {1, 0}, {1, 1}, {1, 2}}
  },
  // 3: O
  {
    {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
    {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
    {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
    {{1, 0}, {2, 0}, {1, 1}, {2, 1}}
  },
  // 4: S
  {
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
    {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
    {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
    {{0, 0}, {0, 1}, {1, 1}, {1, 2}}
  },
  // 5: T
  {
    {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
    {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
    {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
    {{1, 0}, {0, 1}, {1, 1}, {1, 2}}
  },
  // 6: Z
  {
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
    {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
    {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
    {{1, 0}, {0, 1}, {1, 1}, {0, 2}}
  }
};

TetrisGame::TetrisGame(Adafruit_GC9A01A& display)
  : tft(display),
    currentPiece(0),
    currentRotation(0),
    pieceX(3),
    pieceY(0),
    nextPiece(0),
    score(0),
    linesCleared(0),
    level(1),
    dropInterval(750),
    lastDropTime(0),
    selectPressStartTime(0),
    selectIsHeld(false),
    hardDropTriggered(false),
    state(PLAYING) {}

void TetrisGame::init() {
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      board[y][x] = 0;
    }
  }

  score = 0;
  linesCleared = 0;
  level = 1;
  dropInterval = 750;
  state = PLAYING;
  lastDropTime = millis();
  selectIsHeld = false;
  hardDropTriggered = false;

  currentPiece = random(7);
  nextPiece = random(7);
  currentRotation = 0;
  pieceX = 3;
  pieceY = 0;

  render();
}

bool TetrisGame::checkCollision(int piece, int rotation, int px, int py) {
  for (int i = 0; i < 4; i++) {
    int bx = px + PIECE_SHAPES[piece][rotation][i][0];
    int by = py + PIECE_SHAPES[piece][rotation][i][1];

    if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) {
      return true;
    }
    if (by >= 0 && board[by][bx] != 0) {
      return true;
    }
  }
  return false;
}

void TetrisGame::drawBlock(int bx, int by, uint16_t color) {
  int px = BOARD_X + bx * CELL_SIZE;
  int py = BOARD_Y + by * CELL_SIZE;

  if (color == COL_BG) {
    tft.fillRect(px, py, CELL_SIZE, CELL_SIZE, COL_BG);
  } else {
    tft.fillRect(px, py, CELL_SIZE, CELL_SIZE, color);
    tft.drawRect(px, py, CELL_SIZE, CELL_SIZE, COL_WHITE);
  }
}

void TetrisGame::drawPiece(int piece, int rotation, int px, int py, uint16_t color) {
  for (int i = 0; i < 4; i++) {
    int bx = px + PIECE_SHAPES[piece][rotation][i][0];
    int by = py + PIECE_SHAPES[piece][rotation][i][1];
    if (by >= 0) {
      drawBlock(bx, by, color);
    }
  }
}

void TetrisGame::drawBoard() {
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      drawBlock(x, y, board[y][x]);
    }
  }
}

void TetrisGame::drawSidebar() {
  // Score label and value on Left
  tft.fillRect(15, 80, 60, 45, COL_BG);
  tft.setFont(NULL); // Fast default font for compact sidebar
  tft.setTextSize(1);
  tft.setTextColor(COL_CYAN);
  tft.setCursor(22, 85);
  tft.print("SCORE");
  tft.setTextColor(COL_WHITE);
  tft.setCursor(20, 100);
  tft.print(score);

  // Level label and value on Left
  tft.fillRect(15, 135, 60, 45, COL_BG);
  tft.setTextColor(COL_CYAN);
  tft.setCursor(24, 140);
  tft.print("LEVEL");
  tft.setTextColor(COL_YELLOW);
  tft.setCursor(32, 155);
  tft.print(level);

  drawNextPiecePreview();
}

void TetrisGame::drawNextPiecePreview() {
  // Next piece box on Right
  tft.fillRect(166, 80, 50, 52, COL_BG);
  tft.drawRoundRect(166, 80, 48, 50, 4, COL_GREY);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_CYAN);
  tft.setCursor(174, 85);
  tft.print("NEXT");

  // Draw 4x4 preview centered in box
  uint16_t color = PIECE_COLORS[nextPiece];
  for (int i = 0; i < 4; i++) {
    int nx = PIECE_SHAPES[nextPiece][0][i][0];
    int ny = PIECE_SHAPES[nextPiece][0][i][1];
    int px = 173 + nx * 6;
    int py = 98 + ny * 6;
    tft.fillRect(px, py, 5, 5, color);
    tft.drawRect(px, py, 5, 5, COL_WHITE);
  }
}

void TetrisGame::drawGameOver() {
  tft.fillRoundRect(35, 80, 170, 80, 8, COL_PURPLE);
  tft.drawRoundRect(35, 80, 170, 80, 8, COL_WHITE);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_WHITE);

  const char* go = "GAME OVER";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(go, 0, 108, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 108);
  tft.print(go);

  char buf[20];
  snprintf(buf, sizeof(buf), "SCORE: %d", score);
  tft.setTextColor(COL_CYAN);
  tft.getTextBounds(buf, 0, 130, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 130);
  tft.print(buf);

  const char* rst = "SELECT: RESTART";
  tft.setTextColor(COL_WHITE);
  tft.getTextBounds(rst, 0, 150, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 150);
  tft.print(rst);
}

void TetrisGame::render() {
  tft.fillScreen(COL_BG);

  // Playfield frame
  tft.drawRect(BOARD_X - 1, BOARD_Y - 1, BOARD_W * CELL_SIZE + 2, BOARD_H * CELL_SIZE + 2, COL_CYAN);
  tft.drawRect(BOARD_X - 2, BOARD_Y - 2, BOARD_W * CELL_SIZE + 4, BOARD_H * CELL_SIZE + 4, COL_BLUE);

  drawBoard();
  drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
  drawSidebar();

  // Bottom hint
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_GREY);
  tft.setCursor(55, 215);
  tft.print("L/R: MOVE  SEL: ROTATE");
}

void TetrisGame::spawnNewPiece() {
  currentPiece = nextPiece;
  nextPiece = random(7);
  currentRotation = 0;
  pieceX = 3;
  pieceY = 0;

  if (checkCollision(currentPiece, currentRotation, pieceX, pieceY)) {
    state = GAME_OVER;
    drawGameOver();
    return;
  }

  drawNextPiecePreview();
  drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
}

void TetrisGame::lockPiece() {
  for (int i = 0; i < 4; i++) {
    int bx = pieceX + PIECE_SHAPES[currentPiece][currentRotation][i][0];
    int by = pieceY + PIECE_SHAPES[currentPiece][currentRotation][i][1];
    if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
      board[by][bx] = PIECE_COLORS[currentPiece];
    }
  }

  checkLines();
  spawnNewPiece();
}

void TetrisGame::checkLines() {
  int linesFound = 0;

  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x] == 0) {
        full = false;
        break;
      }
    }

    if (full) {
      linesFound++;
      // Shift rows down
      for (int row = y; row > 0; row--) {
        for (int x = 0; x < BOARD_W; x++) {
          board[row][x] = board[row - 1][x];
        }
      }
      for (int x = 0; x < BOARD_W; x++) {
        board[0][x] = 0;
      }
      y++; // recheck this row index
    }
  }

  if (linesFound > 0) {
    int lineScores[5] = {0, 100, 300, 500, 800};
    score += lineScores[linesFound] * level;
    linesCleared += linesFound;

    level = 1 + (linesCleared / 10);
    dropInterval = max(120, 750 - (level - 1) * 60);

    drawBoard();
    drawSidebar();
  }
}

void TetrisGame::update(bool left, bool right, bool select) {
  if (state == GAME_OVER) {
    if (select) {
      init();
    }
    return;
  }

  // Handle Select hold for Hard Drop
  if (select) {
    selectPressStartTime = millis();
    selectIsHeld = true;
    hardDropTriggered = false;

    // Normal tap: Try clockwise rotation
    int nextRot = (currentRotation + 1) % 4;
    if (!checkCollision(currentPiece, nextRot, pieceX, pieceY)) {
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, COL_BG);
      currentRotation = nextRot;
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
    }
  }

  if (selectIsHeld) {
    if (digitalRead(BTN_SELECT) == LOW) {
      if (millis() - selectPressStartTime > 350 && !hardDropTriggered) {
        // Hard Drop
        hardDropTriggered = true;
        drawPiece(currentPiece, currentRotation, pieceX, pieceY, COL_BG);
        while (!checkCollision(currentPiece, currentRotation, pieceX, pieceY + 1)) {
          pieceY++;
        }
        drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
        lockPiece();
        return;
      }
    } else {
      selectIsHeld = false;
    }
  }

  // Handle Horizontal Movement
  if (left) {
    if (!checkCollision(currentPiece, currentRotation, pieceX - 1, pieceY)) {
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, COL_BG);
      pieceX--;
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
    }
  } else if (right) {
    if (!checkCollision(currentPiece, currentRotation, pieceX + 1, pieceY)) {
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, COL_BG);
      pieceX++;
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
    }
  }

  // Gravity
  unsigned long now = millis();
  if (now - lastDropTime >= (unsigned long)dropInterval) {
    lastDropTime = now;

    if (!checkCollision(currentPiece, currentRotation, pieceX, pieceY + 1)) {
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, COL_BG);
      pieceY++;
      drawPiece(currentPiece, currentRotation, pieceX, pieceY, PIECE_COLORS[currentPiece]);
    } else {
      lockPiece();
    }
  }
}

