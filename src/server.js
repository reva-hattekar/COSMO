require("dotenv").config();

const express = require("express");
const cors = require("cors");
const rateLimit = require("express-rate-limit");
const crypto = require("crypto");

const {
    AkinatorClient,
    Languages,
    Themes,
    Answers
} = require("akinator-client");

const { initSchema } = require("./db/schema");
const { getDatabase } = require("./db/connection");
const databaseRoutes = require("./routes/databaseRoutes");
const googleRoutes = require("./routes/googleRoutes");

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());

app.use(rateLimit({
    windowMs: 60 * 1000,
    limit: 120
}));

// Mount Database API endpoints
app.use("/api/database", databaseRoutes);

// Mount Google Reminders (Calendar & Tasks) endpoints
app.use("/api/google", googleRoutes);

// Active game sessions (Legacy Akinator integration kept until replacement ready)
const sessions = new Map();

// Create a new Akinator client
function createAkinator() {
    return new AkinatorClient({
        language: Languages.English,
        theme: Themes.Character,
        childMode: false,
        retries: 3,
        ...(process.env.SCRAPER_API_KEY
            ? { scraperApiKey: process.env.SCRAPER_API_KEY }
            : {})
    });
}

// Format game response
function gameResponse(aki, sessionId, result) {
    return {
        sessionId,
        question: result.question,
        answers: result.answers,
        step: result.step,
        progress: result.progression,
        won: result.won,
        lost: result.ko,
        guess: result.won ? aki.winResult : null
    };
}

// Health check
app.get("/health", async (req, res) => {
    let dbStatus = "connected";
    try {
        const db = getDatabase();
        await db.get("SELECT 1");
    } catch (e) {
        dbStatus = "disconnected: " + e.message;
    }

    res.json({
        status: "online",
        service: "COSMO Akinator & Knowledge Engine Backend",
        database: dbStatus
    });
});

// START GAME
app.post("/api/game/start", async (req, res) => {
    try {
        const aki = createAkinator();
        const result = await aki.start();
        const sessionId = crypto.randomUUID();
        sessions.set(sessionId, aki);

        console.log("New Akinator game:", sessionId);

        res.status(201).json({
            ...gameResponse(aki, sessionId, result),
            status: "started"
        });
    } catch (error) {
        console.error("START ERROR:", error);
        res.status(502).json({
            error: "Could not start Akinator",
            details: error.message
        });
    }
});

// SUBMIT ANSWER
app.post("/api/game/:id/answer", async (req, res) => {
    try {
        const aki = sessions.get(req.params.id);

        if (!aki) {
            return res.status(404).json({
                error: "Game session not found"
            });
        }

        const index = Number(req.body.answerIndex);

        const answerOptions = [
            Answers.Yes,
            Answers.No,
            Answers.IDontKnow,
            Answers.Probably,
            Answers.ProbablyNot
        ];

        if (!Number.isInteger(index) || index < 0 || index > 4) {
            return res.status(400).json({
                error: "Invalid answer index"
            });
        }

        const result = await aki.answer(answerOptions[index]);

        res.json({
            ...gameResponse(aki, req.params.id, result),
            status: result.won
                ? "guess"
                : result.ko
                    ? "lost"
                    : "question"
        });
    } catch (error) {
        console.error("ANSWER ERROR:", error);
        res.status(502).json({
            error: "Could not submit answer",
            details: error.message
        });
    }
});

// GO BACK
app.post("/api/game/:id/back", async (req, res) => {
    try {
        const aki = sessions.get(req.params.id);

        if (!aki) {
            return res.status(404).json({
                error: "Game session not found"
            });
        }

        const result = await aki.back();

        res.json(
            gameResponse(aki, req.params.id, result)
        );
    } catch (error) {
        res.status(502).json({
            error: "Could not go back",
            details: error.message
        });
    }
});

// CONTINUE AFTER WRONG GUESS
app.post("/api/game/:id/continue", async (req, res) => {
    try {
        const aki = sessions.get(req.params.id);

        if (!aki) {
            return res.status(404).json({
                error: "Game session not found"
            });
        }

        const result = await aki.continue();

        res.json({
            ...gameResponse(aki, req.params.id, result),
            status: result.won ? "guess" : "question"
        });
    } catch (error) {
        console.error("CONTINUE ERROR:", error);
        res.status(502).json({
            error: "Could not continue game",
            details: error.message
        });
    }
});

// CONFIRM CORRECT GUESS
app.post("/api/game/:id/win", async (req, res) => {
    try {
        const aki = sessions.get(req.params.id);

        if (!aki) {
            return res.status(404).json({
                error: "Game session not found"
            });
        }

        await aki.submitWin();
        sessions.delete(req.params.id);

        res.json({
            status: "won",
            message: "Akinator guessed correctly!"
        });
    } catch (error) {
        res.status(502).json({
            error: "Could not confirm guess",
            details: error.message
        });
    }
});

// END GAME
app.delete("/api/game/:id", (req, res) => {
    const removed = sessions.delete(req.params.id);

    res.json({
        status: "ended",
        removed
    });
});

// INITIALIZE AND START SERVER
async function startServer() {
    try {
        await initSchema();
        console.log("Database initialized successfully.");
    } catch (err) {
        console.error("Failed to initialize database:", err);
    }

    if (process.env.NODE_ENV !== "test") {
        app.listen(PORT, "0.0.0.0", () => {
            console.log("--------------------------------");
            console.log("       COSMO AKINATOR");
            console.log("--------------------------------");
            console.log(`Server:    http://localhost:${PORT}`);
            console.log(`Health:    http://localhost:${PORT}/health`);
            console.log(`Stats:     http://localhost:${PORT}/api/database/stats`);
            console.log(`Reminders: http://localhost:${PORT}/api/google/reminders`);
            console.log(`GoogleAuth:http://localhost:${PORT}/api/google/auth`);
        });
    }
}

if (require.main === module) {
    startServer();
}

module.exports = { app, startServer };
