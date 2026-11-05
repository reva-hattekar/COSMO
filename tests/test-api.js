const assert = require('assert');
const http = require('http');
const { app } = require('../src/server');
const { seedDatabase } = require('../src/db/seed');
const { getDatabase } = require('../src/db/connection');

function request(port, path) {
    return new Promise((resolve, reject) => {
        const req = http.get(`http://127.0.0.1:${port}${path}`, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                try {
                    const parsed = JSON.parse(data);
                    resolve({ statusCode: res.statusCode, body: parsed });
                } catch (e) {
                    resolve({ statusCode: res.statusCode, body: data });
                }
            });
        });
        req.on('error', reject);
    });
}

async function runApiTests() {
    console.log('==============================================');
    console.log('        COSMO PHASE 1 REST API TESTS         ');
    console.log('==============================================\n');

    process.env.NODE_ENV = 'test';
    const db = getDatabase();
    await seedDatabase(db);

    const server = http.createServer(app);
    await new Promise((resolve) => server.listen(3099, '127.0.0.1', resolve));

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
        await test('GET /health returns online status and database connected', async () => {
            const res = await request(3099, '/health');
            assert.strictEqual(res.statusCode, 200);
            assert.strictEqual(res.body.status, 'online');
            assert.strictEqual(res.body.database, 'connected');
        });

        await test('GET /api/database/stats returns knowledge base statistics', async () => {
            const res = await request(3099, '/api/database/stats');
            assert.strictEqual(res.statusCode, 200);
            assert.strictEqual(res.body.status, 'ok');
            assert(res.body.data.totalEntities >= 25);
            assert.strictEqual(res.body.data.totalAttributes, 29);
            assert(res.body.data.categories.length >= 11);
        });

        await test('GET /api/database/categories returns all categories', async () => {
            const res = await request(3099, '/api/database/categories');
            assert.strictEqual(res.statusCode, 200);
            assert.strictEqual(res.body.status, 'ok');
            assert(Array.isArray(res.body.data));
            assert(res.body.data.some(c => c.name === 'K-pop'));
            assert(res.body.data.some(c => c.name === 'Anime & Manga'));
            assert(res.body.data.some(c => c.name === 'Superheroes & Villains'));
        });

        await test('GET /api/database/search?q=luffy finds Monkey D. Luffy', async () => {
            const res = await request(3099, '/api/database/search?q=luffy');
            assert.strictEqual(res.statusCode, 200);
            assert.strictEqual(res.body.status, 'ok');
            assert(res.body.data.length > 0);
            assert(res.body.data.some(e => e.name.toLowerCase().includes('luffy')));
        });

        await test('GET /api/database/search?q=Spidey finds Spider-Man via alias', async () => {
            const res = await request(3099, '/api/database/search?q=Spidey');
            assert.strictEqual(res.statusCode, 200);
            assert.strictEqual(res.body.status, 'ok');
            assert(res.body.data.length > 0);
            assert.strictEqual(res.body.data[0].name, 'Spider-Man');
        });

        await test('GET /api/database/search without query returns 400', async () => {
            const res = await request(3099, '/api/database/search');
            assert.strictEqual(res.statusCode, 400);
            assert.strictEqual(res.body.status, 'error');
        });

    } finally {
        await new Promise((resolve) => server.close(resolve));
        await db.close();
    }

    console.log('\n==============================================');
    console.log(`API TEST SUMMARY: ${passed} Passed, ${failed} Failed`);
    console.log('==============================================\n');

    if (failed > 0) {
        process.exit(1);
    }
}

runApiTests().catch(err => {
    console.error('API Test Error:', err);
    process.exit(1);
});
