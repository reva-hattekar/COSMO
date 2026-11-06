const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const fs = require('fs');

class DatabaseConnection {
    constructor(dbPath) {
        let targetPath = dbPath || process.env.DATABASE_PATH || process.env.DB_PATH;
        if (!targetPath || typeof targetPath !== 'string' || targetPath.trim().length === 0) {
            targetPath = path.join(__dirname, '../../data/cosmo.db');
        } else {
            targetPath = targetPath.trim();
        }

        // If configured path is a directory (e.g. Railway volume mounted at /data), append default db filename
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
        this.db = null;
        this._isReady = false;
    }

    /**
     * Open SQLite connection and configure PRAGMAs for performance and integrity.
     */
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

                // Run high-performance PRAGMAs
                this.db.serialize(() => {
                    this.db.run('PRAGMA foreign_keys = ON;');
                    if (this.dbPath !== ':memory:') {
                        this.db.run('PRAGMA journal_mode = WAL;');
                        this.db.run('PRAGMA synchronous = NORMAL;');
                    }
                    this.db.run('PRAGMA cache_size = -64000;'); // 64MB cache
                    this.db.run('PRAGMA temp_store = MEMORY;', (pragmaErr) => {
                        if (pragmaErr) return reject(pragmaErr);
                        this._isReady = true;
                        resolve(this);
                    });
                });
            });
        });
    }

    /**
     * Execute an INSERT, UPDATE, or DELETE query.
     */
    run(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.run(sql, params, function (err) {
                if (err) return reject(err);
                resolve({ lastID: this.lastID, changes: this.changes });
            });
        });
    }

    /**
     * Fetch a single row.
     */
    get(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.get(sql, params, (err, row) => {
                if (err) return reject(err);
                resolve(row);
            });
        });
    }

    /**
     * Fetch all matching rows.
     */
    all(sql, params = []) {
        return new Promise((resolve, reject) => {
            this.db.all(sql, params, (err, rows) => {
                if (err) return reject(err);
                resolve(rows || []);
            });
        });
    }

    /**
     * Execute multi-statement SQL strings (e.g., schema DDL).
     */
    exec(sql) {
        return new Promise((resolve, reject) => {
            this.db.exec(sql, (err) => {
                if (err) return reject(err);
                resolve();
            });
        });
    }

    /**
     * Run operations inside a transaction.
     */
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

    /**
     * Close the database connection.
     */
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

function getDatabase(dbPath) {
    if (dbPath) {
        return new DatabaseConnection(dbPath);
    }
    if (!defaultInstance) {
        defaultInstance = new DatabaseConnection();
    }
    return defaultInstance;
}

function resetDefaultInstance() {
    defaultInstance = null;
}

module.exports = {
    DatabaseConnection,
    getDatabase,
    resetDefaultInstance
};

