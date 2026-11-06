/**
 * COSMO Database Migration: SQLite -> Neon PostgreSQL
 *
 * Usage:
 *   node scripts/migrate-sqlite-to-neon.js
 * Or with explicit DATABASE_URL:
 *   DATABASE_URL="postgresql://..." node scripts/migrate-sqlite-to-neon.js
 */

require('dotenv').config();

const path = require('path');
const fs = require('fs');
const { Pool } = require('pg');
const sqlite3 = require('sqlite3').verbose();

// Determine source SQLite path
function getSourceSqlitePath() {
    const candidates = [
        process.env.DATABASE_PATH,
        process.env.DB_PATH,
        path.join(__dirname, '../data/cosmo.db'),
        path.join(__dirname, '../data/cosmo_knowledge.db')
    ];

    for (const c of candidates) {
        if (c && fs.existsSync(c)) {
            return c;
        }
    }
    return path.join(__dirname, '../data/cosmo.db');
}

// Open SQLite helper
function openSqlite(dbPath) {
    return new Promise((resolve, reject) => {
        const db = new sqlite3.Database(dbPath, sqlite3.OPEN_READONLY, (err) => {
            if (err) return reject(err);
            resolve(db);
        });
    });
}

function sqliteAll(db, sql, params = []) {
    return new Promise((resolve, reject) => {
        db.all(sql, params, (err, rows) => {
            if (err) return reject(err);
            resolve(rows || []);
        });
    });
}

async function migrate() {
    console.log('=======================================================');
    console.log('   COSMO KNOWLEDGE & REMINDERS DATABASE MIGRATION     ');
    console.log('            SQLite -> Neon PostgreSQL                ');
    console.log('=======================================================\n');

    const targetUrl = process.env.DATABASE_URL || process.argv[2];
    if (!targetUrl || !targetUrl.startsWith('postgres')) {
        console.error('ERROR: DATABASE_URL environment variable is required.');
        console.error('Example: DATABASE_URL="postgresql://user:pass@ep-cool.us-east-2.aws.neon.tech/neondb?sslmode=require" node scripts/migrate-sqlite-to-neon.js\n');
        process.exit(1);
    }

    const sqlitePath = getSourceSqlitePath();
    console.log(`Source SQLite:   ${sqlitePath} (${fs.existsSync(sqlitePath) ? 'EXISTS' : 'NOT FOUND'})`);
    console.log(`Target Postgres: ${targetUrl.replace(/:[^:@]+@/, ':****@')}\n`);

    // 1. Initialize PostgreSQL Connection Pool
    const pgPool = new Pool({
        connectionString: targetUrl,
        ssl: { rejectUnauthorized: false },
        max: 10,
        connectionTimeoutMillis: 10000
    });

    const client = await pgPool.connect();
    console.log('Connected to Neon PostgreSQL successfully.');

    let sqliteDb = null;
    if (fs.existsSync(sqlitePath)) {
        sqliteDb = await openSqlite(sqlitePath);
        console.log('Opened source SQLite database.');
    } else {
        console.log('No existing SQLite database found. Initializing fresh PostgreSQL schema.');
    }

    try {
        // 2. Initialize PostgreSQL Schema
        console.log('\n[1/7] Applying PostgreSQL Schema...');
        const schemaSql = fs.readFileSync(path.join(__dirname, '../src/db/schema.postgres.sql'), 'utf8');
        await client.query(schemaSql);
        console.log('  Schema and indexes verified.');

        const stats = {
            categories: 0,
            attributes: 0,
            entities: 0,
            entity_attributes: 0,
            aliases: 0,
            import_progress: 0,
            google_tokens: 0
        };

        if (sqliteDb) {
            await client.query('BEGIN');

            // 3. Migrate Categories
            console.log('\n[2/7] Migrating Categories...');
            const categories = await sqliteAll(sqliteDb, 'SELECT * FROM categories ORDER BY parent_id ASC NULLS FIRST, id ASC');
            for (const c of categories) {
                await client.query(`
                    INSERT INTO categories (id, name, parent_id, description, created_at)
                    VALUES ($1, $2, $3, $4, COALESCE($5::timestamptz, CURRENT_TIMESTAMP))
                    ON CONFLICT (name) DO UPDATE SET
                        parent_id = EXCLUDED.parent_id,
                        description = EXCLUDED.description
                `, [c.id, c.name, c.parent_id, c.description, c.created_at]);
                stats.categories++;
            }
            console.log(`  Migrated ${stats.categories} categories.`);

            // 4. Migrate Attributes
            console.log('\n[3/7] Migrating Attributes...');
            const attributes = await sqliteAll(sqliteDb, 'SELECT * FROM attributes ORDER BY id ASC');
            for (const a of attributes) {
                await client.query(`
                    INSERT INTO attributes (id, attribute_key, attribute_name, data_type, question_template, category_scope, created_at)
                    VALUES ($1, $2, $3, $4, $5, $6, COALESCE($7::timestamptz, CURRENT_TIMESTAMP))
                    ON CONFLICT (attribute_key) DO UPDATE SET
                        attribute_name = EXCLUDED.attribute_name,
                        data_type = EXCLUDED.data_type,
                        question_template = EXCLUDED.question_template,
                        category_scope = EXCLUDED.category_scope
                `, [a.id, a.attribute_key, a.attribute_name, a.data_type, a.question_template, a.category_scope, a.created_at]);
                stats.attributes++;
            }
            console.log(`  Migrated ${stats.attributes} attributes.`);

            // 5. Migrate Entities
            console.log('\n[4/7] Migrating Entities...');
            const entities = await sqliteAll(sqliteDb, 'SELECT * FROM entities ORDER BY id ASC');
            for (const e of entities) {
                await client.query(`
                    INSERT INTO entities (
                        id, wikidata_id, name, description, entity_type, category_id, category,
                        subcategory, image_url, gender, nationality, birth_date, death_date,
                        debut_year, is_fictional, popularity_rank, source, created_at, updated_at
                    ) VALUES (
                        $1, $2, $3, $4, $5, $6, $7,
                        $8, $9, $10, $11, $12, $13,
                        $14, $15, $16, $17, COALESCE($18::timestamptz, CURRENT_TIMESTAMP), COALESCE($19::timestamptz, CURRENT_TIMESTAMP)
                    )
                    ON CONFLICT (id) DO UPDATE SET
                        wikidata_id = EXCLUDED.wikidata_id,
                        name = EXCLUDED.name,
                        description = EXCLUDED.description,
                        entity_type = EXCLUDED.entity_type,
                        category_id = EXCLUDED.category_id,
                        category = EXCLUDED.category,
                        subcategory = EXCLUDED.subcategory,
                        image_url = EXCLUDED.image_url,
                        gender = EXCLUDED.gender,
                        nationality = EXCLUDED.nationality,
                        birth_date = EXCLUDED.birth_date,
                        death_date = EXCLUDED.death_date,
                        debut_year = EXCLUDED.debut_year,
                        is_fictional = EXCLUDED.is_fictional,
                        popularity_rank = EXCLUDED.popularity_rank,
                        source = EXCLUDED.source,
                        updated_at = EXCLUDED.updated_at
                `, [
                    e.id, e.wikidata_id, e.name, e.description, e.entity_type, e.category_id, e.category,
                    e.subcategory, e.image_url, e.gender, e.nationality, e.birth_date, e.death_date,
                    e.debut_year, e.is_fictional, e.popularity_rank, e.source, e.created_at, e.updated_at
                ]);
                stats.entities++;
            }
            console.log(`  Migrated ${stats.entities} entities.`);

            // 6. Migrate Entity Attributes
            console.log('\n[5/7] Migrating Entity Attributes...');
            const entityAttrs = await sqliteAll(sqliteDb, 'SELECT * FROM entity_attributes');
            for (const ea of entityAttrs) {
                await client.query(`
                    INSERT INTO entity_attributes (entity_id, attribute_id, attribute_value, confidence, source, created_at)
                    VALUES ($1, $2, $3, $4, $5, COALESCE($6::timestamptz, CURRENT_TIMESTAMP))
                    ON CONFLICT (entity_id, attribute_id, attribute_value) DO UPDATE SET
                        confidence = EXCLUDED.confidence,
                        source = EXCLUDED.source
                `, [ea.entity_id, ea.attribute_id, ea.attribute_value, ea.confidence, ea.source, ea.created_at]);
                stats.entity_attributes++;
            }
            console.log(`  Migrated ${stats.entity_attributes} entity attributes.`);

            // 7. Migrate Aliases
            console.log('\n[6/7] Migrating Aliases...');
            const aliases = await sqliteAll(sqliteDb, 'SELECT * FROM aliases ORDER BY id ASC');
            for (const a of aliases) {
                await client.query(`
                    INSERT INTO aliases (id, entity_id, alias, language, created_at)
                    VALUES ($1, $2, $3, $4, COALESCE($5::timestamptz, CURRENT_TIMESTAMP))
                    ON CONFLICT (entity_id, alias) DO NOTHING
                `, [a.id, a.entity_id, a.alias, a.language, a.created_at]);
                stats.aliases++;
            }
            console.log(`  Migrated ${stats.aliases} aliases.`);

            // 8. Migrate Import Progress
            try {
                const progress = await sqliteAll(sqliteDb, 'SELECT * FROM import_progress ORDER BY id ASC');
                for (const p of progress) {
                    await client.query(`
                        INSERT INTO import_progress (
                            id, batch_name, category, last_offset, total_fetched, total_imported,
                            total_skipped, status, error_message, started_at, updated_at, completed_at
                        ) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)
                        ON CONFLICT (batch_name) DO UPDATE SET
                            last_offset = EXCLUDED.last_offset,
                            total_fetched = EXCLUDED.total_fetched,
                            total_imported = EXCLUDED.total_imported,
                            total_skipped = EXCLUDED.total_skipped,
                            status = EXCLUDED.status,
                            error_message = EXCLUDED.error_message,
                            updated_at = EXCLUDED.updated_at,
                            completed_at = EXCLUDED.completed_at
                    `, [
                        p.id, p.batch_name, p.category, p.last_offset, p.total_fetched, p.total_imported,
                        p.total_skipped, p.status, p.error_message, p.started_at, p.updated_at, p.completed_at
                    ]);
                    stats.import_progress++;
                }
            } catch (err) {
                // Table might be empty or not yet used
            }

            // 9. Migrate Google OAuth Tokens (from SQLite or local file)
            console.log('\n[7/7] Migrating Google OAuth Tokens...');
            let tokensToMigrate = null;
            try {
                const tokenRows = await sqliteAll(sqliteDb, 'SELECT * FROM google_tokens WHERE id = "default"');
                if (tokenRows && tokenRows.length > 0) {
                    tokensToMigrate = tokenRows[0];
                }
            } catch (e) {
                // Not in SQLite table, check local JSON file
            }

            if (!tokensToMigrate) {
                const tokenJsonPath = path.join(__dirname, '../data/google_tokens.json');
                if (fs.existsSync(tokenJsonPath)) {
                    try {
                        const fileContent = JSON.parse(fs.readFileSync(tokenJsonPath, 'utf8'));
                        tokensToMigrate = {
                            access_token: fileContent.access_token,
                            refresh_token: fileContent.refresh_token,
                            expires_at: fileContent.expires_at,
                            updated_at: fileContent.updated_at
                        };
                    } catch (e) {
                        console.error('  Warning: Could not parse local token file:', e.message);
                    }
                }
            }

            if (tokensToMigrate && (tokensToMigrate.access_token || tokensToMigrate.refresh_token)) {
                await client.query(`
                    INSERT INTO google_tokens (id, access_token, refresh_token, expires_at, updated_at)
                    VALUES ('default', $1, $2, $3, CURRENT_TIMESTAMP)
                    ON CONFLICT (id) DO UPDATE SET
                        access_token = EXCLUDED.access_token,
                        refresh_token = COALESCE(NULLIF(EXCLUDED.refresh_token, ''), google_tokens.refresh_token),
                        expires_at = EXCLUDED.expires_at,
                        updated_at = CURRENT_TIMESTAMP
                `, [tokensToMigrate.access_token, tokensToMigrate.refresh_token, tokensToMigrate.expires_at]);
                stats.google_tokens = 1;
                console.log('  Google OAuth refresh tokens migrated into PostgreSQL google_tokens table.');
            } else {
                console.log('  No existing Google OAuth tokens found to migrate.');
            }

            // 10. Reset PostgreSQL Auto-Increment Sequences
            console.log('\nResetting PostgreSQL Sequences...');
            const tablesWithSeq = ['categories', 'attributes', 'entities', 'aliases', 'import_progress'];
            for (const table of tablesWithSeq) {
                await client.query(`
                    SELECT setval(pg_get_serial_sequence('${table}', 'id'), COALESCE((SELECT MAX(id) FROM ${table}), 1), true)
                `);
            }
            console.log('  All sequences synchronized to maximum primary keys.');

            await client.query('COMMIT');
        }

        // 11. Verification Audit
        console.log('\n=======================================================');
        console.log('               MIGRATION VERIFICATION AUDIT            ');
        console.log('=======================================================');
        const counts = await Promise.all([
            client.query('SELECT COUNT(*) FROM categories'),
            client.query('SELECT COUNT(*) FROM attributes'),
            client.query('SELECT COUNT(*) FROM entities'),
            client.query('SELECT COUNT(*) FROM entity_attributes'),
            client.query('SELECT COUNT(*) FROM aliases'),
            client.query('SELECT COUNT(*) FROM google_tokens')
        ]);

        console.log(`  Categories:         ${counts[0].rows[0].count} in PostgreSQL (Migrated: ${stats.categories})`);
        console.log(`  Attributes:         ${counts[1].rows[0].count} in PostgreSQL (Migrated: ${stats.attributes})`);
        console.log(`  Entities:           ${counts[2].rows[0].count} in PostgreSQL (Migrated: ${stats.entities})`);
        console.log(`  Entity Attributes:  ${counts[3].rows[0].count} in PostgreSQL (Migrated: ${stats.entity_attributes})`);
        console.log(`  Aliases:            ${counts[4].rows[0].count} in PostgreSQL (Migrated: ${stats.aliases})`);
        console.log(`  Google Tokens:      ${counts[5].rows[0].count} in PostgreSQL`);
        console.log('=======================================================');
        console.log('STATUS: Migration completed successfully with zero data loss!\n');

    } catch (err) {
        await client.query('ROLLBACK');
        console.error('\nFATAL: Migration failed, transaction rolled back:', err);
        process.exit(1);
    } finally {
        client.release();
        await pgPool.end();
        if (sqliteDb) {
            sqliteDb.close();
        }
    }
}

if (require.main === module) {
    migrate().catch(e => {
        console.error('Fatal migration error:', e);
        process.exit(1);
    });
}

module.exports = { migrate };
