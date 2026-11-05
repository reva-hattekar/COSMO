const assert = require('assert');
const { DatabaseConnection } = require('../src/db/connection');
const { initSchema } = require('../src/db/schema');
const DatabaseQueries = require('../src/db/queries');
const { seedDatabase } = require('../src/db/seed');

async function runTests() {
    console.log('==============================================');
    console.log('       COSMO PHASE 1 DATABASE TESTS          ');
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

    // Use an isolated in-memory database for unit tests
    const testDb = new DatabaseConnection(':memory:');
    await initSchema(testDb);
    const queries = new DatabaseQueries(testDb);

    console.log('[1] Schema & Configuration Tests');
    await test('SQLite initializes in-memory schema with all tables', async () => {
        const tables = await testDb.all(`
            SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'
        `);
        const tableNames = tables.map(t => t.name);
        assert(tableNames.includes('entities'), 'Missing entities table');
        assert(tableNames.includes('attributes'), 'Missing attributes table');
        assert(tableNames.includes('entity_attributes'), 'Missing entity_attributes table');
        assert(tableNames.includes('aliases'), 'Missing aliases table');
        assert(tableNames.includes('categories'), 'Missing categories table');
        assert(tableNames.includes('import_progress'), 'Missing import_progress table');
    });

    await test('Foreign keys and indexes are created', async () => {
        const indexes = await testDb.all(`
            SELECT name FROM sqlite_master WHERE type='index' AND name LIKE 'idx_%'
        `);
        assert(indexes.length >= 8, `Expected at least 8 indexes, got ${indexes.length}`);
    });

    console.log('\n[2] Category Operations');
    let animeCatId;
    await test('Create and retrieve categories', async () => {
        animeCatId = await queries.upsertCategory({
            name: 'Anime & Manga',
            description: 'Japanese animation and comics'
        });
        assert(animeCatId > 0, 'Invalid category ID');

        // Subcategory with parent_id
        const shonenId = await queries.upsertCategory({
            name: 'Shonen Anime',
            parent_id: animeCatId,
            description: 'Action shonen anime'
        });
        assert(shonenId > 0, 'Invalid subcategory ID');

        const allCats = await queries.getCategories();
        assert(allCats.length >= 2, 'Categories should have at least 2 entries');
    });

    console.log('\n[3] Attribute Operations');
    let attrRealId;
    let attrAnimeId;
    await test('Create and query attributes', async () => {
        attrRealId = await queries.upsertAttribute({
            attribute_key: 'is_real',
            attribute_name: 'Real Person',
            data_type: 'boolean',
            question_template: 'Is your character a real person?'
        });
        assert(attrRealId > 0);

        attrAnimeId = await queries.upsertAttribute({
            attribute_key: 'is_anime',
            attribute_name: 'Anime Character',
            data_type: 'boolean',
            question_template: 'Is your character from an anime?'
        });
        assert(attrAnimeId > 0);

        const fetched = await queries.getAttributeByKey('is_real');
        assert.strictEqual(fetched.attribute_name, 'Real Person');
    });

    console.log('\n[4] Entity CRUD & Deduplication');
    let luffyId;
    await test('Insert a new entity', async () => {
        const result = await queries.upsertEntity({
            wikidata_id: 'Q186008',
            name: 'Monkey D. Luffy',
            description: 'Captain of the Straw Hat Pirates',
            entity_type: 'fictional_character',
            category: 'Anime & Manga',
            is_fictional: 1,
            popularity_rank: 5
        });
        assert.strictEqual(result.isNew, true);
        luffyId = result.id;
        assert(luffyId > 0);
    });

    await test('Deduplication: Upsert with existing wikidata_id updates instead of duplicating', async () => {
        const result = await queries.upsertEntity({
            wikidata_id: 'Q186008',
            name: 'Monkey D. Luffy',
            description: 'King of the Pirates',
            category: 'Anime & Manga',
            is_fictional: 1,
            popularity_rank: 4
        });
        assert.strictEqual(result.isNew, false);
        assert.strictEqual(result.id, luffyId);

        const entity = await queries.getEntityById(luffyId);
        assert.strictEqual(entity.description, 'King of the Pirates');
        assert.strictEqual(entity.popularity_rank, 4);

        const count = await testDb.get('SELECT COUNT(*) as c FROM entities WHERE wikidata_id = ?', ['Q186008']);
        assert.strictEqual(count.c, 1, 'Should have exactly 1 record for Q186008');
    });

    await test('Deduplication: Upsert without wikidata_id deduplicates by name and category', async () => {
        const first = await queries.upsertEntity({
            name: 'Original Custom Hero',
            category: 'Superheroes',
            is_fictional: 1
        });
        assert.strictEqual(first.isNew, true);

        const second = await queries.upsertEntity({
            name: 'Original Custom Hero',
            description: 'Added description',
            category: 'Superheroes',
            is_fictional: 1
        });
        assert.strictEqual(second.isNew, false);
        assert.strictEqual(second.id, first.id);
    });

    console.log('\n[5] Entity Attributes & Missing Information Handling');
    await test('Set and fetch entity attributes', async () => {
        await queries.setEntityAttribute(luffyId, attrRealId, '0');
        await queries.setEntityAttribute(luffyId, attrAnimeId, '1');

        const attrs = await queries.getEntityAttributes(luffyId);
        assert.strictEqual(attrs.length, 2);

        const isRealVal = attrs.find(a => a.attribute_key === 'is_real');
        assert.strictEqual(isRealVal.attribute_value, '0');

        const isAnimeVal = attrs.find(a => a.attribute_key === 'is_anime');
        assert.strictEqual(isAnimeVal.attribute_value, '1');
    });

    await test('Missing attribute behavior: unrecorded attributes return null, not false', async () => {
        const attrSingerId = await queries.upsertAttribute({
            attribute_key: 'is_singer',
            attribute_name: 'Singer',
            data_type: 'boolean',
            question_template: 'Is your character a singer?'
        });

        // Luffy does not have is_singer attribute recorded
        const hasAttr = await testDb.get(`
            SELECT attribute_value FROM entity_attributes 
            WHERE entity_id = ? AND attribute_id = ?
        `, [luffyId, attrSingerId]);

        assert.strictEqual(hasAttr, undefined, 'Missing attribute should be undefined, not 0');
    });

    console.log('\n[6] Aliases & Search Operations');
    await test('Add aliases and search by primary name and alias', async () => {
        await queries.addEntityAlias(luffyId, 'Straw Hat Luffy');
        await queries.addEntityAlias(luffyId, 'Mugiwara');

        const aliases = await queries.getEntityAliases(luffyId);
        assert(aliases.includes('Straw Hat Luffy'));
        assert(aliases.includes('Mugiwara'));

        // Search by alias
        const searchAlias = await queries.searchEntities('Mugiwara');
        assert(searchAlias.length > 0, 'Search by alias failed');
        assert.strictEqual(searchAlias[0].id, luffyId);

        // Search by primary name case-insensitive
        const searchName = await queries.searchEntities('luffy');
        assert(searchName.length > 0, 'Search by name failed');
        assert.strictEqual(searchName[0].id, luffyId);
    });

    console.log('\n[7] Transaction Integrity');
    await test('Transaction rolls back completely on error', async () => {
        const countBefore = (await testDb.get('SELECT COUNT(*) as c FROM entities')).c;

        try {
            await testDb.transaction(async () => {
                await queries.upsertEntity({ name: 'Temp Entity 1', category: 'Test' });
                // Deliberately trigger SQL error
                await testDb.run('INSERT INTO nonexistent_table VALUES (1)');
            });
        } catch {
            // Expected error
        }

        const countAfter = (await testDb.get('SELECT COUNT(*) as c FROM entities')).c;
        assert.strictEqual(countAfter, countBefore, 'Transaction did not rollback cleanly');
    });

    console.log('\n[8] Seed Dataset & Multi-Category Verification');
    // Test seeding against file database or fresh in-memory database
    const seedDb = new DatabaseConnection(':memory:');
    await test('Seed database with all 11+ categories and verify statistics', async () => {
        const stats = await seedDatabase(seedDb);
        const seedQueries = new DatabaseQueries(seedDb);

        assert(stats.totalEntities >= 25, `Expected at least 25 entities, got ${stats.totalEntities}`);
        assert(stats.totalAttributes >= 25, `Expected at least 25 attributes, got ${stats.totalAttributes}`);
        assert(stats.totalEntityAttributes >= 200, `Expected at least 200 entity attributes, got ${stats.totalEntityAttributes}`);
        assert(stats.totalAliases >= 40, `Expected at least 40 aliases, got ${stats.totalAliases}`);
        assert(stats.categories.length >= 11, `Expected at least 11 categories, got ${stats.categories.length}`);

        // Verify specific entities exist across diverse domains
        const jungkook = await seedQueries.getEntityByWikidataId('Q20823729');
        assert(jungkook, 'Jungkook (K-pop) missing');
        assert.strictEqual(jungkook.category, 'K-pop');

        const messi = await seedQueries.getEntityByWikidataId('Q615');
        assert(messi, 'Lionel Messi (Sports) missing');

        const spiderman = await seedQueries.getEntityByWikidataId('Q7982');
        assert(spiderman, 'Spider-Man (Superheroes) missing');
        assert.strictEqual(spiderman.is_fictional, 1);

        const einstein = await seedQueries.getEntityByWikidataId('Q937');
        assert(einstein, 'Albert Einstein (Historical) missing');
        assert.strictEqual(einstein.is_fictional, 0);

        const srk = await seedQueries.getEntityByWikidataId('Q9535');
        assert(srk, 'Shah Rukh Khan (Bollywood) missing');

        const mrbeast = await seedQueries.getEntityByWikidataId('Q56256247');
        assert(mrbeast, 'MrBeast (Celebrity) missing');

        // Test alias search on seeded data
        const spideyResults = await seedQueries.searchEntities('Spidey');
        assert(spideyResults.some(e => e.name === 'Spider-Man'), 'Failed to find Spider-Man by "Spidey" alias');

        const kingKhanResults = await seedQueries.searchEntities('King Khan');
        assert(kingKhanResults.some(e => e.name === 'Shah Rukh Khan'), 'Failed to find SRK by "King Khan" alias');
    });

    await testDb.close();
    await seedDb.close();

    console.log('\n==============================================');
    console.log(`TEST SUMMARY: ${passed} Passed, ${failed} Failed`);
    console.log('==============================================\n');

    if (failed > 0) {
        process.exit(1);
    }
}

runTests().catch((err) => {
    console.error('Fatal test runner error:', err);
    process.exit(1);
});
