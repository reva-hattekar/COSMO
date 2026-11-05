# COSMO — ESP32 Smart Interactive Companion & Knowledge Engine

**COSMO** is a handheld ESP32-based interactive companion device featuring a 240×240 circular GC9A01 TFT display, a 3-button physical navigation interface, independent games, an online cloud wallpaper engine, Google Reminders (Calendar & Tasks), and an Akinator-style knowledge engine backend.

---

## 🌟 Features

- **Circular UI Launcher**: Smooth cubic-eased carousel with dynamic RGB565 brightness and 3D depth transitions.
- **Online Galaxy Wallpaper**: Automatic HTTPS download and LittleFS caching of 240×240 galaxy wallpaper with high-contrast live clock overlay.
- **Built-in Games**:
  - **Simon Says**: 4-quadrant interactive sequence memory game with dual-button chord red detection and Game Over exit handler.
  - **Snake**: Classic snake with food spawning and score tracking.
  - **Tetris**: Retro tetromino block-stacking game.
- **Standalone Akinator Client**: Native ESP32 client interfacing with COSMO's Node.js Akinator backend over Wi-Fi.
- **Google Reminders Dashboard**: Read-only display of Google Calendar events and Google Tasks.
- **Magic 8 Ball**: Fun fortunes with celestial thinking animations.

---

## 📁 Repository Structure

```
COSMO/
├── assets/
│   └── wallpaper_240.jpg       # 240x240 optimized galaxy wallpaper
├── docs/
│   ├── GOOGLE_OAUTH_SETUP.md   # Google Cloud OAuth 2.0 configuration guide
│   └── RAILWAY_DEPLOYMENT.md   # Railway deployment instructions
├── firmware/
│   └── cosmo_V1/
│       ├── cosmo_V1.ino        # Main ESP32 sketch & lifecycle management
│       ├── AkinatorApp.h/.cpp  # Akinator client engine
│       ├── SimonGame.h/.cpp    # Simon Says game engine
│       ├── SnakeGame.h/.cpp    # Snake game engine
│       ├── TetrisGame.h/.cpp   # Tetris game engine
│       └── secrets.example.h   # Safe Wi-Fi / API credentials template
├── src/
│   ├── db/                     # SQLite knowledge database & schema
│   ├── ingestion/              # Wikidata SPARQL ingestion pipeline
│   ├── routes/                 # Express REST API routes (Akinator, Google, DB)
│   ├── services/               # Google OAuth & Reminders services
│   └── server.js               # Main backend server entry point
├── tests/                      # Automated test suites
├── .env.example                # Environment variables template
├── .gitignore                  # Git ignore rules
└── package.json                # Node.js backend configuration
```

---

## 🚀 Getting Started

### 1. Firmware (ESP32)
1. Open `firmware/cosmo_V1/cosmo_V1.ino` in Arduino IDE.
2. Ensure required libraries are installed:
   - `Adafruit_GFX`
   - `Adafruit_GC9A01A`
   - `TJpg_Decoder`
3. Copy `firmware/cosmo_V1/secrets.example.h` to `secrets.h` and update your Wi-Fi credentials.
4. Select board **ESP32 Dev Module** and compile/upload.

### 2. Backend (Node.js)
1. Install dependencies:
   ```bash
   npm install
   ```
2. Copy `.env.example` to `.env` and fill in credentials.
3. Start local development server:
   ```bash
   npm start
   ```
4. Run tests:
   ```bash
   npm run test:all
   ```

---

## 🔒 Security
- All sensitive credentials (Wi-Fi passwords, OAuth secrets, database files) are strictly excluded from source control via `.gitignore`.

