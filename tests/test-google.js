const assert = require('assert');
const http = require('http');
const { app } = require('../src/server');

const TEST_PORT = 3098;
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
    console.log('       COSMO GOOGLE REMINDERS API TESTS      ');
    console.log('==============================================');

    const fs = require('fs');
    const path = require('path');
    const googleService = require('../src/services/googleService');
    const tokenFile = path.join(__dirname, '../data/google_tokens.json');
    let backupTokens = null;
    if (fs.existsSync(tokenFile)) {
        backupTokens = fs.readFileSync(tokenFile, 'utf8');
        fs.unlinkSync(tokenFile);
    }
    googleService._tokenCache = null;
    try {
        const { getDatabase } = require('../src/db/connection');
        const db = getDatabase();
        await db.open();
        await db.run('DELETE FROM google_tokens WHERE id = ?', ['default']);
    } catch (e) {}

    server = app.listen(TEST_PORT, '127.0.0.1');
    await new Promise(r => setTimeout(r, 200));

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

    try {
        await test('GET /api/google/status returns unconfigured when env vars missing', async () => {
            const res = await request('/api/google/status');
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.data.authenticated, false);
        });

        await test('GET /api/google/reminders returns unauthenticated payload for ESP32', async () => {
            const res = await request('/api/google/reminders');
            assert.strictEqual(res.status, 200);
            assert.strictEqual(res.data.status, 'unauthenticated');
            assert.ok(Array.isArray(res.data.events));
            assert.ok(Array.isArray(res.data.tasks));
            assert.ok(res.data.authUrl);
        });

        await test('GET /api/google/auth fails gracefully when env vars missing', async () => {
            const res = await request('/api/google/auth');
            assert.strictEqual(res.status, 500);
            assert.strictEqual(res.data.error, 'Google OAuth not configured');
        });

        console.log('\n==============================================');
        console.log(`GOOGLE API TEST SUMMARY: ${passed} Passed, ${failed} Failed`);
        console.log('==============================================\n');
    } finally {
        server.close();
        if (backupTokens !== null) {
            fs.writeFileSync(tokenFile, backupTokens, 'utf8');
        }
        googleService._tokenCache = null;
    }

    if (failed > 0) process.exit(1);
}

runTests().catch(err => {
    console.error('Test execution error:', err);
    process.exit(1);
});

