#include "SnakeGame.h"
#include <Fonts/FreeSans9pt7b.h>

#define COL_BG       0x0000
#define COL_WHITE    0xFFFF
#define COL_CYAN     0x07FF
#define COL_BLUE     0x001F
#define COL_GREEN    0x07E0
#define COL_LIME     0x7FE0
#define COL_RED      0xF800
#define COL_GREY     0x8410
#define COL_PURPLE   0x801F

SnakeGame::SnakeGame(Adafruit_GC9A01A& display)
  : tft(display),
    snakeLength(4),
    dir(1),
    foodX(5),
    foodY(5),
    score(0),
    highScore(0),
    moveInterval(210),
    lastMoveTime(0),
    state(PLAYING),
    needsFullRedraw(true) {}

void SnakeGame::init() {
  snakeLength = 4;
  dir = 1; // start moving Right
  int startX = 6;
  int startY = 10;

  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = startX - i;
    snakeY[i] = startY;
  }

  score = 0;
  moveInterval = 210;
  lastMoveTime = millis();
  state = PLAYING;
  needsFullRedraw = true;

  spawnFood();
  render();
}

void SnakeGame::drawCell(int gx, int gy, uint16_t color) {
  if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return;
  int px = GRID_X + gx * CELL_SIZE;
  int py = GRID_Y + gy * CELL_SIZE;
  tft.fillRect(px, py, CELL_SIZE, CELL_SIZE, color);
}

void SnakeGame::drawFood() {
  int px = GRID_X + foodX * CELL_SIZE;
  int py = GRID_Y + foodY * CELL_SIZE;
  // Draw a rounded apple-style red food with a tiny yellow center
  tft.fillRoundRect(px, py, CELL_SIZE, CELL_SIZE, 2, COL_RED);
  tft.drawPixel(px + 3, py + 3, COL_WHITE);
}

void SnakeGame::spawnFood() {
  bool onSnake;
  int attempts = 0;
  do {
    onSnake = false;
    foodX = random(GRID_W);
    foodY = random(GRID_H);

    for (int i = 0; i < snakeLength; i++) {
      if (snakeX[i] == foodX && snakeY[i] == foodY) {
        onSnake = true;
        break;
      }
    }
    attempts++;
  } while (onSnake && attempts < 100);
}

void SnakeGame::drawScore() {
  tft.fillRect(30, 10, 180, 24, COL_BG);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_CYAN);

  char buf[24];
  snprintf(buf, sizeof(buf), "SCORE: %d", score);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(buf, 0, 26, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 26);
  tft.print(buf);
}

void SnakeGame::render() {
  tft.fillScreen(COL_BG);

  // Playfield border
  tft.drawRect(GRID_X - 1, GRID_Y - 1, GRID_W * CELL_SIZE + 2, GRID_H * CELL_SIZE + 2, COL_CYAN);
  tft.drawRect(GRID_X - 2, GRID_Y - 2, GRID_W * CELL_SIZE + 4, GRID_H * CELL_SIZE + 4, COL_BLUE);

  drawScore();

  // Draw snake body and head
  for (int i = snakeLength - 1; i >= 1; i--) {
    drawCell(snakeX[i], snakeY[i], COL_GREEN);
  }
  drawCell(snakeX[0], snakeY[0], COL_LIME);

  // Draw food
  drawFood();

  // Bottom hint
  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_GREY);
  const char* hint = "L/R: TURN  SEL: PAUSE";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(hint, 0, 222, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 222);
  tft.print(hint);

  needsFullRedraw = false;
}

void SnakeGame::drawPauseOverlay() {
  tft.fillRoundRect(50, 95, 140, 50, 8, COL_BLUE);
  tft.drawRoundRect(50, 95, 140, 50, 8, COL_CYAN);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_WHITE);

  const char* pText = "PAUSED";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(pText, 0, 126, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 126);
  tft.print(pText);
}

void SnakeGame::drawGameOver() {
  tft.fillRoundRect(40, 75, 160, 90, 8, COL_PURPLE);
  tft.drawRoundRect(40, 75, 160, 90, 8, COL_WHITE);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_WHITE);

  const char* goText = "GAME OVER";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(goText, 0, 102, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 102);
  tft.print(goText);

  char scoreBuf[20];
  snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %d", score);
  tft.setTextColor(COL_CYAN);
  tft.getTextBounds(scoreBuf, 0, 126, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 126);
  tft.print(scoreBuf);

  const char* restartText = "SELECT: RESTART";
  tft.setTextColor(COL_WHITE);
  tft.getTextBounds(restartText, 0, 150, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 150);
  tft.print(restartText);
}

void SnakeGame::update(bool left, bool right, bool select) {
  if (state == GAME_OVER) {
    if (select) {
      init();
    }
    return;
  }

  if (state == PAUSED) {
    if (select) {
      state = PLAYING;
      // Re-render playfield to wipe pause box
      render();
    }
    return;
  }

  // Handle steering:
  // Left rotates 90 deg counter-clockwise: (dir + 3) % 4
  // Right rotates 90 deg clockwise: (dir + 1) % 4
  if (left) {
    dir = (dir + 3) % 4;
  } else if (right) {
    dir = (dir + 1) % 4;
  }

  if (select) {
    state = PAUSED;
    drawPauseOverlay();
    return;
  }

  unsigned long now = millis();
  if (now - lastMoveTime < moveInterval) {
    return;
  }
  lastMoveTime = now;

  // Compute next head position
  int dx = 0;
  int dy = 0;
  if (dir == 0) dy = -1;      // Up
  else if (dir == 1) dx = 1;  // Right
  else if (dir == 2) dy = 1;  // Down
  else if (dir == 3) dx = -1; // Left

  int newHeadX = snakeX[0] + dx;
  int newHeadY = snakeY[0] + dy;

  // Collision check: Walls
  if (newHeadX < 0 || newHeadX >= GRID_W || newHeadY < 0 || newHeadY >= GRID_H) {
    state = GAME_OVER;
    if (score > highScore) highScore = score;
    drawGameOver();
    return;
  }

  // Collision check: Self
  for (int i = 0; i < snakeLength - 1; i++) {
    if (snakeX[i] == newHeadX && snakeY[i] == newHeadY) {
      state = GAME_OVER;
      if (score > highScore) highScore = score;
      drawGameOver();
      return;
    }
  }

  // Check food collision
  bool ateFood = (newHeadX == foodX && newHeadY == foodY);

  if (ateFood) {
    if (snakeLength < MAX_LENGTH) {
      snakeLength++;
    }
    score += 10;
    if (moveInterval > 90) {
      moveInterval -= 4;
    }
    drawScore();
    spawnFood();
    drawFood();
  } else {
    // Erase old tail
    drawCell(snakeX[snakeLength - 1], snakeY[snakeLength - 1], COL_BG);
  }

  // Shift body coordinates
  for (int i = snakeLength - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }
  snakeX[0] = newHeadX;
  snakeY[0] = newHeadY;

  // Re-draw neck as body green and head as lime
  if (snakeLength > 1) {
    drawCell(snakeX[1], snakeY[1], COL_GREEN);
  }
  drawCell(snakeX[0], snakeY[0], COL_LIME);
}

