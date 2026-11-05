const { getDatabase } = require('./connection');

class DatabaseQueries {
    constructor(db = null) {
        this.db = db || getDatabase();
    }

    // ==========================================
    // CATEGORIES
    // ==========================================

    async upsertCategory({ name, parent_id = null, description = null }) {
        const existing = await this.db.get('SELECT id FROM categories WHERE name = ?', [name]);
        if (existing) {
            return existing.id;
        }
        const result = await this.db.run(
            'INSERT INTO categories (name, parent_id, description) VALUES (?, ?, ?)',
            [name, parent_id, description]
        );
        return result.lastID;
    }

    async getCategories() {
        return this.db.all(`
            SELECT c.*, COUNT(e.id) as entity_count
            FROM categories c
            LEFT JOIN entities e ON e.category = c.name
            GROUP BY c.id
            ORDER BY entity_count DESC, c.name ASC
        `);
    }

    // ==========================================
    // ATTRIBUTES
    // ==========================================

    async upsertAttribute({
        attribute_key,
        attribute_name,
        data_type = 'boolean',
        question_template,
        category_scope = null
    }) {
        const existing = await this.db.get('SELECT id FROM attributes WHERE attribute_key = ?', [attribute_key]);
        if (existing) {
            await this.db.run(
                `UPDATE attributes SET 
                    attribute_name = ?, 
                    data_type = ?, 
                    question_template = ?, 
                    category_scope = ? 
                WHERE id = ?`,
                [attribute_name, data_type, question_template, category_scope, existing.id]
            );
            return existing.id;
        }

        const result = await this.db.run(
            `INSERT INTO attributes (attribute_key, attribute_name, data_type, question_template, category_scope) 
             VALUES (?, ?, ?, ?, ?)`,
            [attribute_key, attribute_name, data_type, question_template, category_scope]
        );
        return result.lastID;
    }

    async getAttributeByKey(key) {
        return this.db.get('SELECT * FROM attributes WHERE attribute_key = ?', [key]);
    }

    async getAllAttributes() {
        return this.db.all('SELECT * FROM attributes ORDER BY id ASC');
    }

    // ==========================================
    // ENTITIES
    // ==========================================

    async upsertEntity(entity) {
        const {
            wikidata_id = null,
            name,
            description = null,
            entity_type = 'human',
            category_id = null,
            category,
            subcategory = null,
            image_url = null,
            gender = null,
            nationality = null,
            birth_date = null,
            death_date = null,
            debut_year = null,
            is_fictional = 0,
            popularity_rank = 1000000,
            source = 'wikidata'
        } = entity;

        if (!name) {
            throw new Error('Entity name is required');
        }

        // Deduplication check: 1. wikidata_id, 2. (name, category)
        let existing = null;
        if (wikidata_id) {
            existing = await this.db.get('SELECT id FROM entities WHERE wikidata_id = ?', [wikidata_id]);
        }
        if (!existing) {
            existing = await this.db.get('SELECT id FROM entities WHERE name = ? COLLATE NOCASE AND category = ?', [name, category]);
        }

        if (existing) {
            await this.db.run(
                `UPDATE entities SET
                    name = ?,
                    description = COALESCE(?, description),
                    entity_type = ?,
                    category_id = COALESCE(?, category_id),
                    category = ?,
                    subcategory = COALESCE(?, subcategory),
                    image_url = COALESCE(?, image_url),
                    gender = COALESCE(?, gender),
                    nationality = COALESCE(?, nationality),
                    birth_date = COALESCE(?, birth_date),
                    death_date = COALESCE(?, death_date),
                    debut_year = COALESCE(?, debut_year),
                    is_fictional = ?,
                    popularity_rank = MIN(popularity_rank, ?),
                    source = ?
                WHERE id = ?`,
                [
                    name, description, entity_type, category_id, category, subcategory,
                    image_url, gender, nationality, birth_date, death_date, debut_year,
                    is_fictional, popularity_rank, source, existing.id
                ]
            );
            return { id: existing.id, isNew: false };
        }

        const result = await this.db.run(
            `INSERT INTO entities (
                wikidata_id, name, description, entity_type, category_id, category,
                subcategory, image_url, gender, nationality, birth_date, death_date,
                debut_year, is_fictional, popularity_rank, source
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
            [
                wikidata_id, name, description, entity_type, category_id, category,
                subcategory, image_url, gender, nationality, birth_date, death_date,
                debut_year, is_fictional, popularity_rank, source
            ]
        );

        return { id: result.lastID, isNew: true };
    }

    async getEntityById(id) {
        const entity = await this.db.get('SELECT * FROM entities WHERE id = ?', [id]);
        if (!entity) return null;

        const attributes = await this.getEntityAttributes(id);
        const aliases = await this.getEntityAliases(id);
        return {
            ...entity,
            attributes,
            aliases
        };
    }

    async getEntityByWikidataId(wikidataId) {
        const entity = await this.db.get('SELECT * FROM entities WHERE wikidata_id = ?', [wikidataId]);
        if (!entity) return null;
        return this.getEntityById(entity.id);
    }

    async searchEntities(query, limit = 20) {
        const term = `%${query}%`;
        return this.db.all(`
            SELECT DISTINCT e.*
            FROM entities e
            LEFT JOIN aliases a ON a.entity_id = e.id
            WHERE e.name LIKE ? COLLATE NOCASE
               OR a.alias LIKE ? COLLATE NOCASE
               OR e.description LIKE ? COLLATE NOCASE
            ORDER BY e.popularity_rank ASC, e.name ASC
            LIMIT ?
        `, [term, term, term, limit]);
    }

    async deleteEntity(id) {
        return this.db.run('DELETE FROM entities WHERE id = ?', [id]);
    }

    // ==========================================
    // ENTITY ATTRIBUTES
    // ==========================================

    async setEntityAttribute(entityId, attributeId, value, confidence = 1.0, source = 'wikidata') {
        const strVal = String(value);
        return this.db.run(`
            INSERT INTO entity_attributes (entity_id, attribute_id, attribute_value, confidence, source)
            VALUES (?, ?, ?, ?, ?)
            ON CONFLICT(entity_id, attribute_id, attribute_value) DO UPDATE SET
                confidence = excluded.confidence,
                source = excluded.source
        `, [entityId, attributeId, strVal, confidence, source]);
    }

    async setEntityAttributeByKey(entityId, attributeKey, value, confidence = 1.0, source = 'wikidata') {
        let attr = await this.getAttributeByKey(attributeKey);
        if (!attr) {
            const prettyName = attributeKey
                .replace(/^is_|^from_|^has_/, '')
                .replace(/_/g, ' ')
                .replace(/\b\w/g, c => c.toUpperCase());
            const question = `Is your character associated with ${prettyName}?`;
            const attrId = await this.upsertAttribute({
                attribute_key: attributeKey,
                attribute_name: prettyName,
                data_type: 'boolean',
                question_template: question
            });
            attr = { id: attrId };
        }
        return this.setEntityAttribute(entityId, attr.id, value, confidence, source);
    }

    async getEntityAttributes(entityId) {
        return this.db.all(`
            SELECT 
                a.id as attribute_id,
                a.attribute_key,
                a.attribute_name,
                a.question_template,
                ea.attribute_value,
                ea.confidence,
                ea.source
            FROM entity_attributes ea
            JOIN attributes a ON a.id = ea.attribute_id
            WHERE ea.entity_id = ?
            ORDER BY a.attribute_name ASC
        `, [entityId]);
    }

    // ==========================================
    // ALIASES
    // ==========================================

    async addEntityAlias(entityId, alias, language = 'en') {
        if (!alias || typeof alias !== 'string') return;
        const trimmed = alias.trim();
        if (!trimmed) return;

        return this.db.run(`
            INSERT OR IGNORE INTO aliases (entity_id, alias, language)
            VALUES (?, ?, ?)
        `, [entityId, trimmed, language]);
    }

    async getEntityAliases(entityId) {
        const rows = await this.db.all('SELECT alias, language FROM aliases WHERE entity_id = ?', [entityId]);
        return rows.map(r => r.alias);
    }

    // ==========================================
    // IMPORT PROGRESS (RESUMABLE INGESTION)
    // ==========================================

    async getImportProgress(batchName) {
        return this.db.get('SELECT * FROM import_progress WHERE batch_name = ?', [batchName]);
    }

    async saveImportProgress({
        batch_name,
        category,
        last_offset = 0,
        total_fetched = 0,
        total_imported = 0,
        total_skipped = 0,
        status = 'in_progress',
        error_message = null
    }) {
        const existing = await this.getImportProgress(batch_name);
        if (existing) {
            return this.db.run(`
                UPDATE import_progress SET
                    last_offset = ?,
                    total_fetched = ?,
                    total_imported = ?,
                    total_skipped = ?,
                    status = ?,
                    error_message = ?,
                    updated_at = CURRENT_TIMESTAMP,
                    completed_at = CASE WHEN ? = 'completed' THEN CURRENT_TIMESTAMP ELSE completed_at END
                WHERE batch_name = ?
            `, [last_offset, total_fetched, total_imported, total_skipped, status, error_message, status, batch_name]);
        }

        return this.db.run(`
            INSERT INTO import_progress (
                batch_name, category, last_offset, total_fetched, total_imported,
                total_skipped, status, error_message, started_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
        `, [batch_name, category, last_offset, total_fetched, total_imported, total_skipped, status, error_message]);
    }

    // ==========================================
    // DATABASE STATISTICS
    // ==========================================

    async getDatabaseStats() {
        const [
            entityCount,
            attributeCount,
            entityAttrCount,
            aliasCount,
            categories,
            fictionalStats,
            topAttributes
        ] = await Promise.all([
            this.db.get('SELECT COUNT(*) as count FROM entities'),
            this.db.get('SELECT COUNT(*) as count FROM attributes'),
            this.db.get('SELECT COUNT(*) as count FROM entity_attributes'),
            this.db.get('SELECT COUNT(*) as count FROM aliases'),
            this.db.all(`
                SELECT category, COUNT(*) as count 
                FROM entities 
                GROUP BY category 
                ORDER BY count DESC
            `),
            this.db.all(`
                SELECT 
                    CASE WHEN is_fictional = 1 THEN 'fictional' ELSE 'real' END as type,
                    COUNT(*) as count
                FROM entities
                GROUP BY is_fictional
            `),
            this.db.all(`
                SELECT 
                    a.attribute_key,
                    a.attribute_name,
                    COUNT(ea.entity_id) as coverage_count
                FROM attributes a
                LEFT JOIN entity_attributes ea ON ea.attribute_id = a.id
                GROUP BY a.id
                ORDER BY coverage_count DESC
                LIMIT 10
            `)
        ]);

        return {
            totalEntities: entityCount.count,
            totalAttributes: attributeCount.count,
            totalEntityAttributes: entityAttrCount.count,
            totalAliases: aliasCount.count,
            categories: categories,
            entityDistribution: fictionalStats,
            topAttributes: topAttributes
        };
    }
}

module.exports = DatabaseQueries;

