/**
 * COSMO Ingestion Pipeline CLI
 * Command-line runner for batch imports, status checks, and data quality audits.
 */

const { getDatabase } = require('../db/connection');
const { initSchema } = require('../db/schema');
const ImportManager = require('./importManager');
const QualityReporter = require('./qualityReporter');
const { QueryBuilder } = require('./queryBuilder');
const DatabaseQueries = require('../db/queries');

async function main() {
    const args = process.argv.slice(2);
    const command = args[0] || 'help';

    const db = getDatabase();
    await initSchema(db);

    const importManager = new ImportManager({ db, batchSize: 50 });
    const reporter = new QualityReporter(db);
    const queries = new DatabaseQueries(db);

    switch (command) {
        case 'test-500': {
            console.log('==============================================');
            console.log('  RUNNING 500-ENTITY WIKIDATA TEST IMPORT     ');
            console.log('==============================================\n');
            const startTime = Date.now();
            await importManager.importTarget({ totalTarget: 500, resume: true });
            const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
            console.log(`\nImport completed in ${elapsed}s.`);

            console.log('\n--- DATA QUALITY AUDIT ---');
            const report = await reporter.generateReport();
            printQualityReport(report);
            break;
        }

        case 'import': {
            const targetArg = args.find(a => a.startsWith('--target='));
            const target = targetArg ? parseInt(targetArg.split('=')[1], 10) : 500;
            console.log(`Starting import with target: ${target} entities...`);
            await importManager.importTarget({ totalTarget: target, resume: true });
            const report = await reporter.generateReport();
            printQualityReport(report);
            break;
        }

        case 'category': {
            const catKey = args[1];
            if (!catKey) {
                console.error('Please specify category key. Available:', QueryBuilder.getCategories().join(', '));
                process.exit(1);
            }
            const countArg = args.find(a => a.startsWith('--target='));
            const count = countArg ? parseInt(countArg.split('=')[1], 10) : 50;
            console.log(`Importing category: ${catKey} (target: ${count})...`);
            await importManager.importCategory(catKey, { targetCount: count, resume: true });
            const report = await reporter.generateReport();
            printQualityReport(report);
            break;
        }

        case 'status': {
            console.log('--- INGESTION PROGRESS STATUS ---');
            const batches = await db.all('SELECT * FROM import_progress ORDER BY updated_at DESC');
            if (batches.length === 0) {
                console.log('No import batches recorded yet.');
            } else {
                console.table(batches.map(b => ({
                    Batch: b.batch_name,
                    Category: b.category,
                    Offset: b.last_offset,
                    Fetched: b.total_fetched,
                    Imported: b.total_imported,
                    Skipped: b.total_skipped,
                    Status: b.status,
                    Updated: b.updated_at
                })));
            }
            break;
        }

        case 'quality': {
            console.log('--- DATA QUALITY REPORT ---');
            const report = await reporter.generateReport();
            printQualityReport(report);
            break;
        }

        case 'help':
        default: {
            console.log(`
COSMO Ingestion Engine CLI
Commands:
  node src/ingestion/cli.js test-500              Run the validated 500-entity multi-category import
  node src/ingestion/cli.js import --target=1000  Run multi-category import with target count
  node src/ingestion/cli.js category <key>        Import specific category (e.g. kpop, anime_manga)
  node src/ingestion/cli.js status                Show progress of all batch jobs
  node src/ingestion/cli.js quality               Show comprehensive data quality audit report
            `);
            break;
        }
    }

    await db.close();
}

function printQualityReport(report) {
    console.log(`Total Entities in Database:       ${report.totalEntities}`);
    console.log(`Unique Wikidata IDs:              ${report.totalUniqueWikidataIds}`);
    console.log(`Real People:                      ${report.fictionalRealRatio.realPeople}`);
    console.log(`Fictional Characters:             ${report.fictionalRealRatio.fictionalCharacters}`);
    console.log('\nField & Attribute Coverage:');
    console.log(`- Entities with Description:       ${report.metrics.pctWithDescription}%`);
    console.log(`- Entities with Image URL:         ${report.metrics.pctWithImage}%`);
    console.log(`- Entities with Gender:            ${report.metrics.pctWithGender}%`);
    console.log(`- Entities with Nationality:       ${report.metrics.pctWithNationality}%`);
    console.log(`- Entities with Birth/Death Date:  ${report.metrics.pctWithDates}%`);
    console.log(`- Entities with Useful Attributes: ${report.metrics.pctWithUsefulAttributes}%`);
    console.log(`- Entities with Aliases:           ${report.metrics.pctWithAliases}%`);
    console.log(`- Avg Attributes per Entity:       ${report.metrics.avgAttributesPerEntity}`);

    console.log('\nQuality Tier Distribution:');
    if (report.qualityTiers && report.qualityTiers.length > 0) {
        console.table(report.qualityTiers);
    }

    console.log('\nCategory Breakdown:');
    if (report.categories && report.categories.length > 0) {
        console.table(report.categories);
    }
}

if (require.main === module) {
    main().catch(err => {
        console.error('CLI Fatal Error:', err);
        process.exit(1);
    });
}

module.exports = { main };

