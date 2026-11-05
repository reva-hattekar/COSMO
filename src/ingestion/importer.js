/**
 * Database Importer
 * Efficient, transaction-based batch persistence with deduplication,
 * alias merging, and attribute attachment.
 */

const DatabaseQueries = require('../db/queries');
const { getDatabase } = require('../db/connection');

class Importer {
    constructor(db = null) {
        this.db = db || getDatabase();
        this.queries = new DatabaseQueries(this.db);
    }

    /**
     * Ingest an array of mapped entities inside a single transaction.
     * @param {Array<Object>} entities - Mapped entity objects
     * @returns {Promise<Object>} Ingestion summary statistics
     */
    async importBatch(entities) {
        if (!Array.isArray(entities) || entities.length === 0) {
            return {
                total: 0,
                inserted: 0,
                updated: 0,
                skipped: 0,
                attributesAdded: 0,
                aliasesAdded: 0,
                errors: []
            };
        }

        const summary = {
            total: entities.length,
            inserted: 0,
            updated: 0,
            skipped: 0,
            attributesAdded: 0,
            aliasesAdded: 0,
            errors: []
        };

        await this.db.transaction(async () => {
            for (const entity of entities) {
                try {
                    if (!entity || !entity.name || !entity.wikidata_id) {
                        summary.skipped++;
                        continue;
                    }

                    // 1. Ensure category exists
                    let categoryId = null;
                    if (entity.category) {
                        categoryId = await this.queries.upsertCategory({
                            name: entity.category
                        });
                    }

                    // 2. Upsert entity
                    const { id: entityId, isNew } = await this.queries.upsertEntity({
                        wikidata_id: entity.wikidata_id,
                        name: entity.name,
                        description: entity.description,
                        entity_type: entity.entity_type,
                        category_id: categoryId,
                        category: entity.category,
                        subcategory: entity.subcategory,
                        image_url: entity.image_url,
                        gender: entity.gender,
                        nationality: entity.nationality,
                        birth_date: entity.birth_date,
                        death_date: entity.death_date,
                        debut_year: entity.debut_year,
                        is_fictional: entity.is_fictional,
                        popularity_rank: entity.popularity_rank,
                        source: 'wikidata'
                    });

                    if (isNew) {
                        summary.inserted++;
                    } else {
                        summary.updated++;
                    }

                    // 3. Insert aliases
                    if (entity.aliases && Array.isArray(entity.aliases)) {
                        for (const alias of entity.aliases) {
                            const res = await this.queries.addEntityAlias(entityId, alias);
                            if (res && res.changes > 0) {
                                summary.aliasesAdded++;
                            }
                        }
                    }

                    // 4. Attach attributes
                    if (entity.attributes && typeof entity.attributes === 'object') {
                        for (const [attrKey, attrVal] of Object.entries(entity.attributes)) {
                            try {
                                await this.queries.setEntityAttributeByKey(
                                    entityId,
                                    attrKey,
                                    attrVal,
                                    1.0,
                                    'wikidata'
                                );
                                summary.attributesAdded++;
                            } catch (attrErr) {
                                // Ignore missing attribute definition; attribute keys are pre-registered
                            }
                        }
                    }

                } catch (err) {
                    summary.errors.push({
                        wikidata_id: entity ? entity.wikidata_id : 'unknown',
                        name: entity ? entity.name : 'unknown',
                        error: err.message
                    });
                    summary.skipped++;
                }
            }
        });

        return summary;
    }
}

module.exports = Importer;

