const express = require('express');
const router = express.Router();
const googleService = require('../services/googleService');

// GET /api/google/auth: Initiate OAuth flow
router.get('/auth', (req, res) => {
    if (!process.env.GOOGLE_CLIENT_ID || !process.env.GOOGLE_CLIENT_SECRET) {
        return res.status(500).json({
            error: 'Google OAuth not configured',
            details: 'GOOGLE_CLIENT_ID and GOOGLE_CLIENT_SECRET environment variables are missing.'
        });
    }

    const authUrl = googleService.getAuthUrl();
    if (req.query.format === 'json') {
        return res.json({ authUrl });
    }
    res.redirect(authUrl);
});

// GET /api/google/callback: OAuth redirect handler
router.get('/callback', async (req, res) => {
    const { code, error } = req.query;

    if (error) {
        return res.status(400).send(`
            <html>
                <body style="font-family: sans-serif; background: #0f172a; color: #ef4444; padding: 40px; text-align: center;">
                    <h1>Authentication Failed</h1>
                    <p>${error}</p>
                    <a href="/api/google/auth" style="color: #38bdf8;">Try Again</a>
                </body>
            </html>
        `);
    }

    if (!code) {
        return res.status(400).json({ error: 'Missing authorization code' });
    }

    try {
        await googleService.exchangeCode(code);
        res.send(`
            <html>
                <body style="font-family: sans-serif; background: #0f172a; color: #f8fafc; padding: 40px; text-align: center;">
                    <h1 style="color: #22c55e;">COSMO Connected!</h1>
                    <p>Google Calendar and Google Tasks are successfully linked to COSMO.</p>
                    <p style="color: #94a3b8;">You can now close this tab. COSMO will display your reminders automatically.</p>
                    <a href="/api/google/reminders" style="display: inline-block; margin-top: 20px; padding: 10px 20px; background: #38bdf8; color: #0f172a; text-decoration: none; border-radius: 6px; font-weight: bold;">View Reminders Feed</a>
                </body>
            </html>
        `);
    } catch (err) {
        res.status(500).send(`
            <html>
                <body style="font-family: sans-serif; background: #0f172a; color: #ef4444; padding: 40px; text-align: center;">
                    <h1>Token Exchange Error</h1>
                    <p>${err.message}</p>
                    <a href="/api/google/auth" style="color: #38bdf8;">Try Again</a>
                </body>
            </html>
        `);
    }
});

// GET /api/google/status: Check auth & configuration status
router.get('/status', async (req, res) => {
    try {
        const status = await googleService.getStatus();
        res.json(status);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// GET /api/google/calendar: Raw/formatted calendar events
router.get('/calendar', async (req, res) => {
    try {
        const limit = parseInt(req.query.limit, 10) || 10;
        const events = await googleService.getCalendarEvents(limit);
        res.json({ events });
    } catch (err) {
        res.status(err.message.includes('Not authenticated') ? 401 : 502).json({
            error: err.message,
            authUrl: '/api/google/auth'
        });
    }
});

// GET /api/google/tasks: Raw/formatted tasks
router.get('/tasks', async (req, res) => {
    try {
        const limit = parseInt(req.query.limit, 10) || 15;
        const tasks = await googleService.getTasks(limit);
        res.json({ tasks });
    } catch (err) {
        res.status(err.message.includes('Not authenticated') ? 401 : 502).json({
            error: err.message,
            authUrl: '/api/google/auth'
        });
    }
});

// GET /api/google/reminders: Compact unified endpoint for COSMO ESP32
router.get('/reminders', async (req, res) => {
    try {
        const reminders = await googleService.getCombinedReminders();
        res.json(reminders);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

module.exports = router;

