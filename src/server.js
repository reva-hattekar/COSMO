require("dotenv").config();

const express = require("express");
const cors = require("cors");
const rateLimit = require("express-rate-limit");

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

// Health check
app.get("/health", async (req, res) => {
    let dbStatus = "connected";
    let dbEngine = "unknown";
    try {
        const db = getDatabase();
        await db.get("SELECT 1");
        dbStatus = "connected";
        dbEngine = db.type || (process.env.DATABASE_URL ? "postgres" : "sqlite");
    } catch (e) {
        dbStatus = "disconnected: " + e.message;
    }

    res.json({
        status: "online",
        service: "COSMO Backend Engine",
        database: dbStatus,
        databaseEngine: dbEngine
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
            console.log("         COSMO BACKEND          ");
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
