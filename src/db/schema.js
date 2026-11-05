const fs = require('fs');
const path = require('path');
const { getDatabase } = require('./connection');

/**
 * Initialize database tables, indexes, and triggers from schema.sql.
 */
async function initSchema(db = null) {
    const database = db || getDatabase();
    await database.open();

    const schemaPath = path.join(__dirname, 'schema.sql');
    const sql = fs.readFileSync(schemaPath, 'utf8');

    await database.exec(sql);
    return database;
}

module.exports = {
    initSchema
};

