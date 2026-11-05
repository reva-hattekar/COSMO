#include "SimonGame.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

#define COL_BG         0x0000
#define COL_WHITE      0xFFFF
#define COL_CYAN       0x07FF
#define COL_GREY       0x8410
#define COL_DARKGREY   0x2104

// Dim and Bright colors for the 4 quadrants
#define COL_GREEN_DIM   0x0320
#define COL_GREEN_BRT   0x07E0

#define COL_RED_DIM     0x7800
#define COL_RED_BRT     0xF800

#define COL_YELLOW_DIM  0x6B40
#define COL_YELLOW_BRT  0xFFE0

#define COL_BLUE_DIM    0x0010
#define COL_BLUE_BRT    0x051F

#define BTN_LEFT        33
#define BTN_RIGHT       25

static const uint16_t PAD_COLORS_DIM[4] = {
  COL_GREEN_DIM,  // 0: Top-Left (Green)
  COL_RED_DIM,    // 1: Top-Right (Red)
  COL_YELLOW_DIM, // 2: Bottom-Left (Yellow)
  COL_BLUE_DIM    // 3: Bottom-Right (Blue)
};

static const uint16_t PAD_COLORS_BRT[4] = {
  COL_GREEN_BRT,
  COL_RED_BRT,
  COL_YELLOW_BRT,
  COL_BLUE_BRT
};

SimonGame::SimonGame(Adafruit_GC9A01A& display)
  : tft(display),
    currentLevel(1),
    playerStep(0),
    bestScore(0),
    state(STARTING),
    activeFlashColor(-1),
    stateTimer(0),
    sequenceStep(0),
    pendingInput(-1),
    pendingInputTime(0) {}

void SimonGame::init() {
  exitRequested = false;
  currentLevel = 1;
  playerStep = 0;
  activeFlashColor = -1;
  pendingInput = -1;

  for (int i = 0; i < MAX_STEPS; i++) {
    sequence[i] = random(4);
  }

  render();

  state = STARTING;
  stateTimer = millis() + 800; // brief pause before showing sequence
}

void SimonGame::drawPad(int index, bool highlighted) {
  uint16_t color = highlighted ? PAD_COLORS_BRT[index] : PAD_COLORS_DIM[index];
  int px = 0;
  int py = 0;

  if (index == 0) { // Top-Left (Green)
    px = 38;
    py = 38;
  } else if (index == 1) { // Top-Right (Red)
    px = 124;
    py = 38;
  } else if (index == 2) { // Bottom-Left (Yellow)
    px = 38;
    py = 124;
  } else if (index == 3) { // Bottom-Right (Blue)
    px = 124;
    py = 124;
  }

  tft.fillRoundRect(px, py, 78, 78, 12, color);

  if (highlighted) {
    tft.drawRoundRect(px, py, 78, 78, 12, COL_WHITE);
    tft.drawRoundRect(px + 1, py + 1, 76, 76, 11, COL_WHITE);
  } else {
    tft.drawRoundRect(px, py, 78, 78, 12, COL_DARKGREY);
  }
}

void SimonGame::drawCenterConsole() {
  tft.fillCircle(120, 120, 28, COL_BG);
  tft.drawCircle(120, 120, 28, COL_WHITE);
  tft.drawCircle(120, 120, 27, COL_CYAN);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_CYAN);
  tft.setCursor(112, 108);
  tft.print("LVL");

  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(COL_WHITE);

  char lvlBuf[6];
  snprintf(lvlBuf, sizeof(lvlBuf), "%d", currentLevel);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(lvlBuf, 0, 131, &x1, &y1, &w, &h);
  tft.setCursor(120 - (w / 2), 131);
  tft.print(lvlBuf);
}

void SimonGame::drawGameOver() {
  tft.fillRoundRect(35, 75, 170, 95, 8, COL_RED_DIM);
  tft.drawRoundRect(35, 75, 170, 95, 8, COL_WHITE);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(COL_WHITE);

  const char* go = "WRONG!";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(go, 0, 98, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 98);
  tft.print(go);

  char buf[24];
  snprintf(buf, sizeof(buf), "LEVEL: %d  BEST: %d", currentLevel, bestScore);
  tft.setTextColor(COL_YELLOW_BRT);
  tft.getTextBounds(buf, 0, 120, &x1, &y1, &w, &h);
  tft.setCursor((240 - w) / 2, 120);
  tft.print(buf);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(68, 138);
  tft.print("SELECT: RESTART");

  tft.setTextColor(COL_CYAN);
  tft.setCursor(76, 153);
  tft.print("L + R: EXIT");
}

void SimonGame::render() {
  tft.fillScreen(COL_BG);

  // Outer decorative circle
  tft.drawCircle(120, 120, 114, COL_DARKGREY);

  // Draw 4 pads
  for (int i = 0; i < 4; i++) {
    drawPad(i, false);
  }

  // Draw button assignment legends around the pads
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(COL_GREY);

  // Top labels
  tft.setCursor(62, 26);
  tft.print("LEFT");

  tft.setCursor(154, 26);
  tft.print("L+R");

  // Bottom labels
  tft.setCursor(56, 206);
  tft.print("SELECT");

  tft.setCursor(150, 206);
  tft.print("RIGHT");

  drawCenterConsole();
}

void SimonGame::playFlash(int color) {
  activeFlashColor = color;
  for (int i = 0; i < 4; i++) {
    drawPad(i, i == color);
  }
}

void SimonGame::checkPlayerInput(int color) {
  playFlash(color);
  state = WAITING_PLAYER;
  stateTimer = millis() + 200; // flash duration

  if (color == sequence[playerStep]) {
    playerStep++;
    if (playerStep >= currentLevel) {
      // Completed level!
      if (currentLevel > bestScore) {
        bestScore = currentLevel;
      }
      currentLevel++;
      state = CORRECT_FEEDBACK;
      stateTimer = millis() + 500;
    }
  } else {
    // Incorrect input
    state = WRONG_FEEDBACK;
    stateTimer = millis() + 350;
  }
}

void SimonGame::update(bool left, bool right, bool select) {
  unsigned long now = millis();

  // Handle Game Over
  if (state == GAME_OVER) {
    if ((digitalRead(BTN_LEFT) == LOW && digitalRead(BTN_RIGHT) == LOW) ||
        (left && digitalRead(BTN_RIGHT) == LOW) ||
        (right && digitalRead(BTN_LEFT) == LOW)) {
      exitRequested = true;
      return;
    }

    if (select) {
      exitRequested = false;
      init();
    }
    return;
  }

  // Handle Start Delay
  if (state == STARTING) {
    if (now >= stateTimer) {
      state = PLAYING_SEQUENCE;
      sequenceStep = 0;
      activeFlashColor = -1;
      stateTimer = now + 100;
    }
    return;
  }

  // Playback Sequence
  if (state == PLAYING_SEQUENCE) {
    if (activeFlashColor != -1) {
      // Currently flashing a color
      if (now >= stateTimer) {
        // Turn off flash
        drawPad(activeFlashColor, false);
        activeFlashColor = -1;
        sequenceStep++;

        if (sequenceStep >= currentLevel) {
          // Finished playing sequence, hand over to player
          state = WAITING_PLAYER;
          playerStep = 0;
          return;
        } else {
          // Pause before next flash
          stateTimer = now + 180;
        }
      }
    } else {
      // Ready for next flash in sequence
      if (now >= stateTimer) {
        int color = sequence[sequenceStep];
        playFlash(color);
        stateTimer = now + 380; // flash duration
      }
    }
    return;
  }

  // Correct Feedback
  if (state == CORRECT_FEEDBACK) {
    if (now >= stateTimer) {
      drawPad(activeFlashColor, false);
      activeFlashColor = -1;
      drawCenterConsole();

      state = PLAYING_SEQUENCE;
      sequenceStep = 0;
      stateTimer = now + 400;
    }
    return;
  }

  // Wrong Feedback
  if (state == WRONG_FEEDBACK) {
    if (now >= stateTimer) {
      // Flash all 4 red briefly
      for (int i = 0; i < 4; i++) {
        drawPad(i, false);
      }
      state = GAME_OVER;
      drawGameOver();
    }
    return;
  }

  // Player Turn:
  if (state == WAITING_PLAYER) {
    // If a pad is currently flashing from player press, turn it off after 200ms
    if (activeFlashColor != -1 && now >= stateTimer) {
      drawPad(activeFlashColor, false);
      activeFlashColor = -1;
    }

    // 4-way button decoding:
    // Left = 0 (Green)
    // Select = 2 (Yellow)
    // Right = 3 (Blue)
    // Left + Right = 1 (Red)

    if (pendingInput != -1) {
      // Resolving potential simultaneous Left+Right press
      if (digitalRead(BTN_LEFT) == LOW && digitalRead(BTN_RIGHT) == LOW) {
        pendingInput = -1;
        checkPlayerInput(1); // Red (Left+Right)
        return;
      }
      if (now - pendingInputTime > 55) {
        int resolved = pendingInput;
        pendingInput = -1;
        checkPlayerInput(resolved);
        return;
      }
    }

    // Immediate check if both are held
    if (digitalRead(BTN_LEFT) == LOW && digitalRead(BTN_RIGHT) == LOW) {
      checkPlayerInput(1); // Red
      return;
    }

    if (select) {
      checkPlayerInput(2); // Yellow
      return;
    }

    if (left) {
      // Wait 55ms to see if right is pressed simultaneously
      if (digitalRead(BTN_RIGHT) == LOW) {
        checkPlayerInput(1); // Red
      } else {
        pendingInput = 0; // Green
        pendingInputTime = now;
      }
      return;
    }

    if (right) {
      if (digitalRead(BTN_LEFT) == LOW) {
        checkPlayerInput(1); // Red
      } else {
        pendingInput = 3; // Blue
        pendingInputTime = now;
      }
      return;
    }
  }
}

