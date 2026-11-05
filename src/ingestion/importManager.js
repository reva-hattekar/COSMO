/**
 * Ingestion Manager
 * Coordinates multi-category ingestion with resumability, batching,
 * progress tracking, and rate limiting.
 */

const WikidataClient = require('./wikidataClient');
const { QueryBuilder, CATEGORY_DEFINITIONS } = require('./queryBuilder');
const EntityMapper = require('./entityMapper');
const Importer = require('./importer');
const DatabaseQueries = require('../db/queries');
const { getDatabase } = require('../db/connection');

class ImportManager {
    constructor(options = {}) {
        this.db = options.db || getDatabase();
        this.queries = new DatabaseQueries(this.db);
        this.client = options.client || new WikidataClient(options.clientOptions || {});
        this.importer = new Importer(this.db);
        this.batchSize = options.batchSize || 50;
    }

    /**
     * Run an import job for a single category key with resumability.
     * @param {string} categoryKey - One of the supported category keys
     * @param {Object} options - Run options
     * @param {number} options.targetCount - Target number of entities to import for this category
     * @param {boolean} options.resume - Whether to resume from last saved offset (default: true)
     * @param {Function} options.onProgress - Optional callback for batch progress
     */
    async importCategory(categoryKey, options = {}) {
        const catDef = CATEGORY_DEFINITIONS[categoryKey];
        if (!catDef) {
            throw new Error(`Unknown category key: ${categoryKey}`);
        }

        const targetCount = options.targetCount || 50;
        const batchSize = Math.min(this.batchSize, targetCount);
        const batchName = `wikidata_${categoryKey}_v1`;

        // Check existing progress
        let progress = await this.queries.getImportProgress(batchName);
        let offset = 0;
        let totalImported = 0;
        let totalFetched = 0;
        let totalSkipped = 0;

        if (progress && options.resume !== false) {
            offset = progress.last_offset || 0;
            totalImported = progress.total_imported || 0;
            totalFetched = progress.total_fetched || 0;
            totalSkipped = progress.total_skipped || 0;
            console.log(`[ImportManager] Resuming category '${categoryKey}' from offset ${offset} (already imported: ${totalImported})`);
        } else {
            console.log(`[ImportManager] Starting fresh import for category '${categoryKey}' (target: ${targetCount})`);
            await this.queries.saveImportProgress({
                batch_name: batchName,
                category: catDef.categoryName,
                last_offset: 0,
                total_fetched: 0,
                total_imported: 0,
                total_skipped: 0,
                status: 'in_progress'
            });
        }

        let categoryImportedInRun = 0;

        while (categoryImportedInRun < targetCount) {
            const currentLimit = Math.min(batchSize, targetCount - categoryImportedInRun);
            console.log(`[ImportManager] Fetching ${categoryKey}: offset ${offset}, limit ${currentLimit}...`);

            const sparql = QueryBuilder.buildQuery(categoryKey, {
                limit: currentLimit,
                offset: offset
            });

            let bindings;
            try {
                bindings = await this.client.querySparql(sparql);
            } catch (err) {
                console.error(`[ImportManager] Error fetching category ${categoryKey} at offset ${offset}:`, err.message);
                await this.queries.saveImportProgress({
                    batch_name: batchName,
                    category: catDef.categoryName,
                    last_offset: offset,
                    total_fetched: totalFetched,
                    total_imported: totalImported,
                    total_skipped: totalSkipped,
                    status: 'failed',
                    error_message: err.message
                });
                throw err;
            }

            if (!bindings || bindings.length === 0) {
                console.log(`[ImportManager] No more entities returned for '${categoryKey}' at offset ${offset}. Finished.`);
                break;
            }

            // Map bindings to entities
            const entities = bindings
                .map(b => EntityMapper.mapBinding(b, catDef))
                .filter(Boolean);

            const batchSummary = await this.importer.importBatch(entities);

            offset += bindings.length;
            totalFetched += bindings.length;
            totalImported += (batchSummary.inserted + batchSummary.updated);
            totalSkipped += (bindings.length - entities.length) + batchSummary.skipped;
            categoryImportedInRun += batchSummary.inserted;

            console.log(`[ImportManager] Batch complete: +${batchSummary.inserted} inserted, +${batchSummary.updated} updated, ${batchSummary.skipped} skipped. Category total: ${totalImported}`);

            // Save progress
            await this.queries.saveImportProgress({
                batch_name: batchName,
                category: catDef.categoryName,
                last_offset: offset,
                total_fetched: totalFetched,
                total_imported: totalImported,
                total_skipped: totalSkipped,
                status: 'in_progress'
            });

            if (options.onProgress) {
                options.onProgress({
                    categoryKey,
                    offset,
                    batchImported: batchSummary.inserted,
                    totalImported,
                    targetCount
                });
            }

            // Stop if Wikidata returned fewer items than requested (end of dataset)
            if (bindings.length < currentLimit) {
                break;
            }
        }

        await this.queries.saveImportProgress({
            batch_name: batchName,
            category: catDef.categoryName,
            last_offset: offset,
            total_fetched: totalFetched,
            total_imported: totalImported,
            total_skipped: totalSkipped,
            status: 'completed'
        });

        console.log(`[ImportManager] Category '${categoryKey}' completed! Total imported: ${totalImported}`);
        return {
            categoryKey,
            totalFetched,
            totalImported,
            totalSkipped,
            lastOffset: offset
        };
    }

    /**
     * Run multi-category import distributed across all categories.
     * @param {Object} options
     * @param {number} options.totalTarget - Total target entities to import (e.g. 500)
     * @param {Array<string>} options.categories - Specific categories to import (default: all)
     */
    async importTarget(options = {}) {
        const totalTarget = options.totalTarget || 500;
        const availableCategories = options.categories || QueryBuilder.getCategories();
        const perCategoryTarget = Math.max(15, Math.ceil(totalTarget / availableCategories.length));

        console.log(`====================================================`);
        console.log(`[ImportManager] Starting Multi-Category Ingestion`);
        console.log(`  Total Target: ${totalTarget} entities`);
        console.log(`  Categories: ${availableCategories.length} categories`);
        console.log(`  Per-Category Target: ~${perCategoryTarget} entities`);
        console.log(`====================================================\n`);

        const results = [];
        let grandTotalImported = 0;

        for (const catKey of availableCategories) {
            if (grandTotalImported >= totalTarget) {
                console.log(`[ImportManager] Grand target of ${totalTarget} reached!`);
                break;
            }

            const remaining = totalTarget - grandTotalImported;
            const categoryTarget = Math.min(perCategoryTarget, remaining);

            try {
                const res = await this.importCategory(catKey, {
                    targetCount: categoryTarget,
                    resume: options.resume !== false
                });
                results.push(res);
                grandTotalImported += res.totalImported;
            } catch (err) {
                console.error(`[ImportManager] Category '${catKey}' encountered an error: ${err.message}. Continuing to next category...`);
                results.push({
                    categoryKey: catKey,
                    error: err.message
                });
            }
        }

        console.log(`\n====================================================`);
        console.log(`[ImportManager] Ingestion Run Finished`);
        console.log(`  Grand Total Entities Processed: ${grandTotalImported}`);
        console.log(`====================================================\n`);

        return {
            totalTarget,
            grandTotalImported,
            categoryResults: results
        };
    }
}

module.exports = ImportManager;

