const fs = require('fs');
const path = require('path');
const { getDatabase } = require('./connection');

/**
 * Initialize database tables, indexes, and triggers from schema.sql or schema.postgres.sql.
 */
async function initSchema(db = null) {
    const database = db || getDatabase();
    await database.open();

    const isPostgres = database.type === 'postgres' || Boolean(database.pool);
    const schemaFile = isPostgres ? 'schema.postgres.sql' : 'schema.sql';
    const schemaPath = path.join(__dirname, schemaFile);
    const sql = fs.readFileSync(schemaPath, 'utf8');

    await database.exec(sql);
    return database;
}

module.exports = {
    initSchema
};
