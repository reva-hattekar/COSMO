const assert = require('assert');
const path = require('path');
const fs = require('fs');
const http = require('http');

const { DatabaseConnection, resetDefaultInstance } = require('../src/db/connection');
const googleService = require('../src/services/googleService');
const { app } = require('../src/server');

const TEST_PORT = 3097;
let server;

function request(path, options = {}) {
    return new Promise((resolve, reject) => {
        const req = http.request({
            hostname: '127.0.0.1',
            port: TEST_PORT,
            path,
            method: options.method || 'GET',
            headers: options.headers || {}
        }, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                let parsed = null;
                try {
                    parsed = JSON.parse(data);
                } catch (e) {
                    parsed = data;
                }
                resolve({ status: res.statusCode, data: parsed, headers: res.headers });
            });
        });
        req.on('error', reject);
        if (options.body) {
            req.write(typeof options.body === 'string' ? options.body : JSON.stringify(options.body));
        }
        req.end();
    });
}

async function runTests() {
    console.log('==============================================');
    console.log('    COSMO RAILWAY HARDENING & SECURITY TESTS  ');
    console.log('==============================================\n');

    let passed = 0;
    let failed = 0;

    async function test(name, fn) {
        try {
            await fn();
            console.log(`  PASS: ${name}`);
            passed++;
        } catch (err) {
            console.error(`  FAIL: ${name}`);
            console.error(`    ${err.message}`);
            failed++;
        }
    }

    // ----------------------------------------------------
    // 1. Database Persistence & Path Precedence Tests
    // ----------------------------------------------------
    console.log('[1] Database Persistence & Path Precedence');

    await test('DATABASE_PATH is prioritized over DB_PATH and default', () => {
        const originalDbPath = process.env.DB_PATH;
        const originalDatabasePath = process.env.DATABASE_PATH;
        try {
            process.env.DATABASE_PATH = './tmp_test_dir/priority.db';
            process.env.DB_PATH = './tmp_test_dir/ignored.db';

            const conn = new DatabaseConnection();
            assert.strictEqual(conn.dbPath, './tmp_test_dir/priority.db');
            assert.ok(fs.existsSync('./tmp_test_dir'), 'Directory was created automatically');
        } finally {
            if (originalDbPath) process.env.DB_PATH = originalDbPath; else delete process.env.DB_PATH;
            if (originalDatabasePath) process.env.DATABASE_PATH = originalDatabasePath; else delete process.env.DATABASE_PATH;
            if (fs.existsSync('./tmp_test_dir')) fs.rmSync('./tmp_test_dir', { recursive: true, force: true });
        }
    });

    await test('DB_PATH is used when DATABASE_PATH is not set', () => {
        const originalDbPath = process.env.DB_PATH;
        const originalDatabasePath = process.env.DATABASE_PATH;
        try {
            delete process.env.DATABASE_PATH;
            process.env.DB_PATH = './tmp_test_dir2/fallback.db';

            const conn = new DatabaseConnection();
            assert.strictEqual(conn.dbPath, './tmp_test_dir2/fallback.db');
            assert.ok(fs.existsSync('./tmp_test_dir2'), 'Directory was created automatically');
        } finally {
            if (originalDbPath) process.env.DB_PATH = originalDbPath; else delete process.env.DB_PATH;
            if (originalDatabasePath) process.env.DATABASE_PATH = originalDatabasePath; else delete process.env.DATABASE_PATH;
            if (fs.existsSync('./tmp_test_dir2')) fs.rmSync('./tmp_test_dir2', { recursive: true, force: true });
        }
    });

    await test('Default path is used when neither env var is set', () => {
        const originalDbPath = process.env.DB_PATH;
        const originalDatabasePath = process.env.DATABASE_PATH;
        try {
            delete process.env.DATABASE_PATH;
            delete process.env.DB_PATH;

            const conn = new DatabaseConnection();
            assert.ok(conn.dbPath.endsWith(path.join('data', 'cosmo.db')));
        } finally {
            if (originalDbPath) process.env.DB_PATH = originalDbPath; else delete process.env.DB_PATH;
            if (originalDatabasePath) process.env.DATABASE_PATH = originalDatabasePath; else delete process.env.DATABASE_PATH;
        }
    });

    // ----------------------------------------------------
    // 2. Google OAuth Refresh Token Preservation Tests
    // ----------------------------------------------------
    console.log('\n[2] Google Refresh Token Preservation');

    const TOKENS_PATH = path.join(__dirname, '../data/google_tokens.json');
    let originalTokenBackup = null;
    if (fs.existsSync(TOKENS_PATH)) {
        originalTokenBackup = fs.readFileSync(TOKENS_PATH, 'utf8');
    }

    try {
        await test('saveTokens preserves existing refresh_token when new payload lacks one', () => {
            // First save initial tokens with valid refresh_token
            googleService.saveTokens({
                access_token: 'initial_access',
                refresh_token: 'valid_refresh_token_12345',
                expires_at: Date.now() + 3600000,
                updated_at: new Date().toISOString()
            });

            const saved1 = googleService.getTokens();
            assert.strictEqual(saved1.refresh_token, 'valid_refresh_token_12345');

            // Now save new tokens that omit refresh_token
            googleService.saveTokens({
                access_token: 'new_access_token',
                refresh_token: undefined,
                expires_at: Date.now() + 7200000,
                updated_at: new Date().toISOString()
            });

            const saved2 = googleService.getTokens();
            assert.strictEqual(saved2.access_token, 'new_access_token');
            assert.strictEqual(saved2.refresh_token, 'valid_refresh_token_12345', 'Refresh token was preserved');
        });

        await test('saveTokens preserves existing refresh_token when new refresh_token is empty string', () => {
            googleService.saveTokens({
                access_token: 'third_access_token',
                refresh_token: '   ',
                expires_at: Date.now() + 3600000,
                updated_at: new Date().toISOString()
            });

            const saved = googleService.getTokens();
            assert.strictEqual(saved.refresh_token, 'valid_refresh_token_12345', 'Whitespace/empty refresh token ignored');
        });
    } finally {
        if (originalTokenBackup !== null) {
            fs.writeFileSync(TOKENS_PATH, originalTokenBackup, 'utf8');
        } else if (fs.existsSync(TOKENS_PATH)) {
            fs.unlinkSync(TOKENS_PATH);
        }
        googleService._tokenCache = null;
        try {
            const { getDatabase } = require('../src/db/connection');
            const db = getDatabase();
            await db.open();
            await db.run('DELETE FROM google_tokens WHERE id = ?', ['default']);
        } catch (e) {
            // Ignore cleanup error
        }
    }

    // ----------------------------------------------------
    // 3. OAuth State Generation & Validation Tests
    // ----------------------------------------------------
    console.log('\n[3] OAuth State Generation & CSRF Protection');

    await test('generateOAuthState creates valid hex string with 10-minute TTL', () => {
        const state = googleService.generateOAuthState();
        assert.ok(typeof state === 'string');
        assert.strictEqual(state.length, 48); // 24 bytes in hex
        assert.ok(googleService.validateOAuthState(state), 'Valid state passes validation');
    });

    await test('validateOAuthState consumes state (single use, prevents replay attacks)', () => {
        const state = googleService.generateOAuthState();
        assert.ok(googleService.validateOAuthState(state), 'First use succeeds');
        assert.strictEqual(googleService.validateOAuthState(state), false, 'Second use fails (replay prevented)');
    });

    await test('validateOAuthState rejects invalid or non-existent states', () => {
        assert.strictEqual(googleService.validateOAuthState('totally-bogus-state'), false);
        assert.strictEqual(googleService.validateOAuthState(''), false);
        assert.strictEqual(googleService.validateOAuthState(null), false);
    });

    // ----------------------------------------------------
    // 4. Google API Error Handling & Combined Reminders Tests
    // ----------------------------------------------------
    console.log('\n[4] Google API Error Handling (getCombinedReminders)');

    await test('getCombinedReminders returns "unauthenticated" when no account is connected', async () => {
        // Temporarily ensure tokens are empty
        const originalTokens = googleService.getTokens;
        googleService.getTokens = () => null;

        try {
            const res = await googleService.getCombinedReminders();
            assert.strictEqual(res.status, 'unauthenticated');
            assert.strictEqual(res.eventsCount, 0);
            assert.strictEqual(res.tasksCount, 0);
            assert.ok(Array.isArray(res.events));
            assert.ok(Array.isArray(res.tasks));
            assert.ok(res.authUrl);
            assert.ok(res.updatedAt);
        } finally {
            googleService.getTokens = originalTokens;
        }
    });

    await test('getCombinedReminders returns "partial" status when one service fails', async () => {
        const origGetStatus = googleService.getStatus;
        const origGetCalendar = googleService.getCalendarEvents;
        const origGetTasks = googleService.getTasks;

        googleService.getStatus = async () => ({ configured: true, authenticated: true });
        googleService.getCalendarEvents = async () => ([
            { id: '1', title: 'Calendar Event 1', time: '10:00', allDay: false }
        ]);
        googleService.getTasks = async () => {
            throw new Error('Tasks API rate limit exceeded');
        };

        try {
            const res = await googleService.getCombinedReminders();
            assert.strictEqual(res.status, 'partial', 'Status must be partial');
            assert.strictEqual(res.eventsCount, 1);
            assert.strictEqual(res.tasksCount, 0);
            assert.strictEqual(res.events.length, 1);
            assert.strictEqual(res.tasks.length, 0);
            assert.ok(res.errors && res.errors.tasks, 'Error details preserved');
            assert.ok(res.message.includes('Tasks failed'));
        } finally {
            googleService.getStatus = origGetStatus;
            googleService.getCalendarEvents = origGetCalendar;
            googleService.getTasks = origGetTasks;
        }
    });

    await test('getCombinedReminders returns "error" status when both services fail with API error', async () => {
        const origGetStatus = googleService.getStatus;
        const origGetCalendar = googleService.getCalendarEvents;
        const origGetTasks = googleService.getTasks;

        googleService.getStatus = async () => ({ configured: true, authenticated: true });
        googleService.getCalendarEvents = async () => {
            throw new Error('503 Service Unavailable');
        };
        googleService.getTasks = async () => {
            throw new Error('500 Internal Server Error');
        };

        try {
            const res = await googleService.getCombinedReminders();
            assert.strictEqual(res.status, 'error', 'Status must be error when both fail');
            assert.strictEqual(res.eventsCount, 0);
            assert.strictEqual(res.tasksCount, 0);
            assert.ok(Array.isArray(res.events));
            assert.ok(Array.isArray(res.tasks));
            assert.ok(res.errors && res.errors.calendar && res.errors.tasks);
        } finally {
            googleService.getStatus = origGetStatus;
            googleService.getCalendarEvents = origGetCalendar;
            googleService.getTasks = origGetTasks;
        }
    });

    await test('getCombinedReminders returns "ok" on complete success', async () => {
        const origGetStatus = googleService.getStatus;
        const origGetCalendar = googleService.getCalendarEvents;
        const origGetTasks = googleService.getTasks;

        googleService.getStatus = async () => ({ configured: true, authenticated: true });
        googleService.getCalendarEvents = async () => ([
            { id: 'ev1', title: 'Team Sync', time: '14:00', allDay: false }
        ]);
        googleService.getTasks = async () => ([
            { id: 't1', title: 'Buy milk', dueFormatted: 'Nov 7', completed: false, overdue: false }
        ]);

        try {
            const res = await googleService.getCombinedReminders();
            assert.strictEqual(res.status, 'ok', 'Status must be ok');
            assert.strictEqual(res.eventsCount, 1);
            assert.strictEqual(res.tasksCount, 1);
            assert.strictEqual(res.events[0].title, 'Team Sync');
            assert.strictEqual(res.tasks[0].title, 'Buy milk');
            assert.ok(!res.errors);
        } finally {
            googleService.getStatus = origGetStatus;
            googleService.getCalendarEvents = origGetCalendar;
            googleService.getTasks = origGetTasks;
        }
    });

    // ----------------------------------------------------
    // 5. API Key Security (COSMO_API_KEY) Tests
    // ----------------------------------------------------
    console.log('\n[5] COSMO_API_KEY Security Tests');

    server = app.listen(TEST_PORT, '127.0.0.1');
    await new Promise(r => setTimeout(r, 200));

    try {
        const originalApiKey = process.env.COSMO_API_KEY;

        // Set test API key
        process.env.COSMO_API_KEY = 'super_secret_test_key_xyz987';

        await test('Private endpoint without API key returns 401 Unauthorized', async () => {
            const res = await request('/api/google/reminders');
            assert.strictEqual(res.status, 401);
            assert.strictEqual(res.data.error, 'Unauthorized');
        });

        await test('Private database endpoint without API key returns 401 Unauthorized', async () => {
            const res = await request('/api/database/stats');
            assert.strictEqual(res.status, 401);
            assert.strictEqual(res.data.error, 'Unauthorized');
        });

        await test('Private endpoint with invalid API key returns 403 Forbidden', async () => {
            const res = await request('/api/google/reminders', {
                headers: { 'Authorization': 'Bearer wrong_key' }
            });
            assert.strictEqual(res.status, 403);
            assert.strictEqual(res.data.error, 'Forbidden');
        });

        await test('Private endpoint with correct Bearer token returns 200 OK', async () => {
            const res = await request('/api/google/reminders', {
                headers: { 'Authorization': 'Bearer super_secret_test_key_xyz987' }
            });
            assert.strictEqual(res.status, 200);
            assert.ok(res.data.status);
        });

        await test('Public health check /health remains accessible without API key', async () => {
            const res = await request('/health');
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.data.status, 'online');
        });

        await test('Browser OAuth /api/google/auth remains accessible without API key', async () => {
            const res = await request('/api/google/auth?format=json');
            // When client ID/secret are missing, it returns 500 but NOT 401/403
            assert.ok(res.status === 200 || res.status === 500);
            assert.notStrictEqual(res.status, 401);
            assert.notStrictEqual(res.status, 403);
        });

        await test('Browser OAuth /api/google/callback rejects missing or invalid state with 403', async () => {
            const res = await request('/api/google/callback?code=fake_code&state=fake_state');
            assert.strictEqual(res.status, 403);
            assert.ok(typeof res.data === 'string' && res.data.includes('Security Verification Failed'));
        });

        // Restore original env
        if (originalApiKey) {
            process.env.COSMO_API_KEY = originalApiKey;
        } else {
            delete process.env.COSMO_API_KEY;
        }

        await test('When COSMO_API_KEY is not set, endpoints are accessible without token', async () => {
            delete process.env.COSMO_API_KEY;
            const res = await request('/api/google/reminders');
            assert.strictEqual(res.status, 200);
        });

    } finally {
        server.close();
    }

    console.log('\n==============================================');
    console.log(`TEST SUMMARY: ${passed} Passed, ${failed} Failed`);
    console.log('==============================================\n');

    if (failed > 0) process.exit(1);
}

runTests().catch(err => {
    console.error('Fatal test error:', err);
    process.exit(1);
});
