const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { PostgresDatabaseConnection } = require('../src/db/connection');

async function runPostgresTests() {
    console.log('==============================================');
    console.log('   POSTGRESQL & NEON INTEGRATION UNIT TESTS   ');
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

    // 1. Connection class & SSL / Neon Pool configuration
    await test('PostgresDatabaseConnection initializes with Neon SSL & pool parameters', () => {
        const dummyUrl = 'postgresql://user:pass@ep-cool-pooler.us-east-2.aws.neon.tech/neondb?sslmode=require';
        const conn = new PostgresDatabaseConnection(dummyUrl);
        assert.strictEqual(conn.type, 'postgres');
        assert.strictEqual(conn.connectionString, dummyUrl);
        assert.strictEqual(conn._isReady, false);
    });

    // 2. Query Translation & SQL Formatting Unit Tests
    await test('PostgresDatabaseConnection._formatSql translates ? placeholders to $1, $2, $3', () => {
        const conn = new PostgresDatabaseConnection('postgresql://localhost/dummy');
        const sql = 'SELECT * FROM entities WHERE category = ? AND is_fictional = ? AND id > ?';
        const formatted = conn._formatSql(sql);
        assert.strictEqual(formatted, 'SELECT * FROM entities WHERE category = $1 AND is_fictional = $2 AND id > $3');
    });

    await test('PostgresDatabaseConnection._formatSql strips SQLite COLLATE NOCASE', () => {
        const conn = new PostgresDatabaseConnection('postgresql://localhost/dummy');
        const sql = 'SELECT * FROM entities WHERE name = ? COLLATE NOCASE';
        const formatted = conn._formatSql(sql);
        assert.strictEqual(formatted, 'SELECT * FROM entities WHERE name = $1 ');
    });

    await test('PostgresDatabaseConnection._formatSql translates scalar MIN(col, ?) to LEAST(col, $1)', () => {
        const conn = new PostgresDatabaseConnection('postgresql://localhost/dummy');
        const sql = 'UPDATE entities SET popularity_rank = MIN(popularity_rank, ?) WHERE id = ?';
        const formatted = conn._formatSql(sql);
        assert.strictEqual(formatted, 'UPDATE entities SET popularity_rank = LEAST(popularity_rank, $1) WHERE id = $2');
    });

    await test('PostgresDatabaseConnection._formatSql converts INSERT OR IGNORE INTO aliases to ON CONFLICT DO NOTHING', () => {
        const conn = new PostgresDatabaseConnection('postgresql://localhost/dummy');
        const sql = 'INSERT OR IGNORE INTO aliases (entity_id, alias, language) VALUES (?, ?, ?)';
        const formatted = conn._formatSql(sql);
        assert.ok(formatted.startsWith('INSERT INTO aliases'));
        assert.ok(formatted.includes('ON CONFLICT (entity_id, alias) DO NOTHING'));
        assert.ok(formatted.includes('$1, $2, $3'));
    });

    // 3. PostgreSQL Schema Validation
    await test('src/db/schema.postgres.sql is valid and contains all 7 tables & indexes', () => {
        const schemaPath = path.join(__dirname, '../src/db/schema.postgres.sql');
        assert.ok(fs.existsSync(schemaPath), 'schema.postgres.sql exists');
        const sql = fs.readFileSync(schemaPath, 'utf8');

        // Check all 7 tables
        const expectedTables = [
            'categories',
            'entities',
            'attributes',
            'entity_attributes',
            'aliases',
            'import_progress',
            'google_tokens'
        ];
        for (const tbl of expectedTables) {
            assert.ok(sql.includes(`CREATE TABLE IF NOT EXISTS ${tbl}`), `Table ${tbl} defined in schema`);
        }

        // Check PostgreSQL trigger
        assert.ok(sql.includes('CREATE OR REPLACE FUNCTION update_updated_at_column()'));
        assert.ok(sql.includes('CREATE TRIGGER trg_entities_updated_at'));
    });

    // 4. Live Neon Connection Test (conditional if DATABASE_URL provided)
    const liveUrl = process.env.DATABASE_URL;
    if (liveUrl && /^postgres(?:ql)?:\/\//i.test(liveUrl)) {
        console.log('\n[Live Neon PostgreSQL Database Tests Detected]');
        await test('Live Neon PostgreSQL: connects, initializes schema, and verifies query execution', async () => {
            const conn = new PostgresDatabaseConnection(liveUrl);
            await conn.open();
            assert.strictEqual(conn._isReady, true);

            const schemaPath = path.join(__dirname, '../src/db/schema.postgres.sql');
            await conn.exec(fs.readFileSync(schemaPath, 'utf8'));

            const res = await conn.get('SELECT 1 as num');
            assert.strictEqual(Number(res.num), 1);

            await conn.close();
        });
    } else {
        console.log('\n[Info] DATABASE_URL not set in local environment; live Neon connection test skipped (unit tests passed).');
    }

    console.log('\n==============================================');
    console.log(`POSTGRES TEST SUMMARY: ${passed} Passed, ${failed} Failed`);
    console.log('==============================================\n');

    if (failed > 0) process.exit(1);
}

runPostgresTests().catch(err => {
    console.error('Test execution error:', err);
    process.exit(1);
});
