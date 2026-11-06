const sqlite3 = require('sqlite3').verbose();
const { Pool } = require('pg');
const path = require('path');
const fs = require('fs');

/**
 * PostgreSQL Database Connection (Neon PostgreSQL with connection pooling & SSL)
 */
class PostgresDatabaseConnection {
    constructor(connectionString) {
        this.connectionString = connectionString || process.env.DATABASE_URL;
        this.type = 'postgres';
        this.pool = null;
        this._isReady = false;
    }

    async open() {
        if (this.pool && this._isReady) return this;

        // Configure connection pool for Neon
        const poolConfig = {
            connectionString: this.connectionString,
            ssl: {
                rejectUnauthorized: false
            },
            max: 10,
            idleTimeoutMillis: 30000,
            connectionTimeoutMillis: 10000
        };

        this.pool = new Pool(poolConfig);

        // Verify connection with health query
        const client = await this.pool.connect();
        try {
            await client.query('SELECT 1');
            this._isReady = true;
        } finally {
            client.release();
        }

        return this;
    }

    _formatSql(sql) {
        let paramIndex = 1;
        // Convert ? placeholders to $1, $2, ...
        let formatted = sql.replace(/\?/g, () => `$${paramIndex++}`);
        // Remove SQLite COLLATE NOCASE
        formatted = formatted.replace(/\bCOLLATE\s+NOCASE\b/gi, '');
        // Translate scalar MIN(col, ?) to LEAST(col, ?)
        formatted = formatted.replace(/\bMIN\s*\(\s*([a-zA-Z0-9_]+)\s*,\s*(\$[0-9]+)\s*\)/gi, 'LEAST($1, $2)');
        // Translate INSERT OR IGNORE INTO to INSERT INTO ... ON CONFLICT DO NOTHING
        if (/\bINSERT\s+OR\s+IGNORE\s+INTO\s+([a-zA-Z0-9_]+)/i.test(formatted)) {
            formatted = formatted.replace(/\bINSERT\s+OR\s+IGNORE\s+INTO\s+/i, 'INSERT INTO ');
            if (!/ON\s+CONFLICT/i.test(formatted)) {
                // Determine target conflict target if aliases
                if (/INTO\s+aliases\b/i.test(formatted)) {
                    formatted += ' ON CONFLICT (entity_id, alias) DO NOTHING';
                }
            }
        }
        return formatted;
    }

    async run(sql, params = []) {
        if (!this.pool) await this.open();
        let formatted = this._formatSql(sql);

        const isInsert = /^\s*INSERT\s+INTO/i.test(formatted);
        const hasReturning = /\bRETURNING\b/i.test(formatted);
        const hasConflictDoNothing = /ON\s+CONFLICT\b.*DO\s+NOTHING/i.test(formatted);

        // If insert lacks RETURNING id and isn't DO NOTHING, request id
        if (isInsert && !hasReturning && !hasConflictDoNothing) {
            formatted += ' RETURNING id';
        }

        const res = await this.pool.query(formatted, params);
        const lastID = (res.rows && res.rows[0] && res.rows[0].id !== undefined) ? res.rows[0].id : null;

        return {
            lastID,
            changes: res.rowCount || 0
        };
    }

    async get(sql, params = []) {
        if (!this.pool) await this.open();
        const formatted = this._formatSql(sql);
        const res = await this.pool.query(formatted, params);
        return res.rows && res.rows.length > 0 ? res.rows[0] : undefined;
    }

    async all(sql, params = []) {
        if (!this.pool) await this.open();
        const formatted = this._formatSql(sql);
        const res = await this.pool.query(formatted, params);
        return res.rows || [];
    }

    async exec(sql) {
        if (!this.pool) await this.open();
        await this.pool.query(sql);
    }

    async transaction(action) {
        if (!this.pool) await this.open();
        const client = await this.pool.connect();
        try {
            await client.query('BEGIN');
            const txDb = {
                type: 'postgres',
                run: async (sql, params = []) => {
                    let formatted = this._formatSql(sql);
                    const isInsert = /^\s*INSERT\s+INTO/i.test(formatted);
                    const hasReturning = /\bRETURNING\b/i.test(formatted);
                    const hasConflictDoNothing = /ON\s+CONFLICT\b.*DO\s+NOTHING/i.test(formatted);
                    if (isInsert && !hasReturning && !hasConflictDoNothing) {
                        formatted += ' RETURNING id';
                    }
                    const r = await client.query(formatted, params);
                    return {
                        lastID: (r.rows && r.rows[0] && r.rows[0].id !== undefined) ? r.rows[0].id : null,
                        changes: r.rowCount || 0
                    };
                },
                get: async (sql, params = []) => {
                    const r = await client.query(this._formatSql(sql), params);
                    return r.rows && r.rows.length > 0 ? r.rows[0] : undefined;
                },
                all: async (sql, params = []) => {
                    const r = await client.query(this._formatSql(sql), params);
                    return r.rows || [];
                },
                exec: async (sql) => {
                    await client.query(sql);
                }
            };

            const result = await action(txDb);
            await client.query('COMMIT');
            return result;
        } catch (err) {
            try {
                await client.query('ROLLBACK');
            } catch (rbErr) {
                console.error('[PostgreSQL] Rollback failed:', rbErr);
            }
            throw err;
        } finally {
            client.release();
        }
    }

    async close() {
        if (this.pool) {
            await this.pool.end();
            this.pool = null;
            this._isReady = false;
        }
    }
}

/**
 * SQLite Database Connection (Local development and testing fallback)
 */
class SqliteDatabaseConnection {
    constructor(dbPath) {
        let targetPath = dbPath || process.env.DATABASE_PATH || process.env.DB_PATH;
        if (!targetPath || typeof targetPath !== 'string' || targetPath.trim().length === 0) {
            targetPath = path.join(__dirname, '../../data/cosmo.db');
        } else {
            targetPath = targetPath.trim();
        }

        if (targetPath !== ':memory:') {
            try {
                if (fs.existsSync(targetPath) && fs.statSync(targetPath).isDirectory()) {
                    targetPath = path.join(targetPath, 'cosmo.db');
                }
            } catch (e) {
                // Ignore stat errors if path does not exist yet
            }

            const dir = path.dirname(targetPath);
            if (!fs.existsSync(dir)) {
                fs.mkdirSync(dir, { recursive: true });
            }
        }

        this.dbPath = targetPath;
        this.type = 'sqlite';
        this.db = null;
        this._isReady = false;
    }

    async open() {
        if (this.db) return this;

        if (this.dbPath !== ':memory:') {
            const dir = path.dirname(this.dbPath);
            if (!fs.existsSync(dir)) {
                fs.mkdirSync(dir, { recursive: true });
            }
        }

        return new Promise((resolve, reject) => {
            this.db = new sqlite3.Database(this.dbPath, (err) => {
                if (err) return reject(err);

                this.db.serialize(() => {
                    this.db.run('PRAGMA foreign_keys = ON;');
                    if (this.dbPath !== ':memory:') {
                        this.db.run('PRAGMA journal_mode = WAL;');
                        this.db.run('PRAGMA synchronous = NORMAL;');
                    }
                    this.db.run('PRAGMA cache_size = -64000;');
                    this.db.run('PRAGMA temp_store = MEMORY;', (pragmaErr) => {
                        if (pragmaErr) return reject(pragmaErr);
                        this._isReady = true;
                        resolve(this);
                    });
                });
            });
        });
    }

    run(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.run(sql, params, function (err) {
                if (err) return reject(err);
                resolve({ lastID: this.lastID, changes: this.changes });
            });
        });
    }

    get(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.get(sql, params, (err, row) => {
                if (err) return reject(err);
                resolve(row);
            });
        });
    }

    all(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.all(sql, params, (err, rows) => {
                if (err) return reject(err);
                resolve(rows || []);
            });
        });
    }

    exec(sql) {
        return new Promise((resolve, reject) => {
            this.db.exec(sql, (err) => {
                if (err) return reject(err);
                resolve();
            });
        });
    }

    async transaction(action) {
        await this.run('BEGIN IMMEDIATE;');
        try {
            const result = await action(this);
            await this.run('COMMIT;');
            return result;
        } catch (error) {
            try {
                await this.run('ROLLBACK;');
            } catch (rollbackErr) {
                console.error('Rollback failed:', rollbackErr);
            }
            throw error;
        }
    }

    close() {
        return new Promise((resolve, reject) => {
            if (!this.db) return resolve();
            this.db.close((err) => {
                if (err) return reject(err);
                this.db = null;
                this._isReady = false;
                resolve();
            });
        });
    }
}

// Default singleton instance
let defaultInstance = null;

function getDatabase(customPathOrUrl) {
    if (customPathOrUrl) {
        if (/^postgres(?:ql)?:\/\//i.test(customPathOrUrl)) {
            return new PostgresDatabaseConnection(customPathOrUrl);
        }
        return new SqliteDatabaseConnection(customPathOrUrl);
    }

    if (!defaultInstance) {
        const dbUrl = process.env.DATABASE_URL;
        if (dbUrl && /^postgres(?:ql)?:\/\//i.test(dbUrl.trim())) {
            defaultInstance = new PostgresDatabaseConnection(dbUrl.trim());
        } else {
            defaultInstance = new SqliteDatabaseConnection();
        }
    }
    return defaultInstance;
}

function resetDefaultInstance() {
    if (defaultInstance) {
        try {
            defaultInstance.close();
        } catch (e) {
            // Ignore close error on reset
        }
    }
    defaultInstance = null;
}

module.exports = {
    PostgresDatabaseConnection,
    SqliteDatabaseConnection,
    DatabaseConnection: SqliteDatabaseConnection, // Alias for backward compatibility
    getDatabase,
    resetDefaultInstance
};
