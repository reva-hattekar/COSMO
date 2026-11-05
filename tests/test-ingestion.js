const assert = require('assert');
const { QueryBuilder } = require('../src/ingestion/queryBuilder');
const EntityMapper = require('../src/ingestion/entityMapper');
const WikidataClient = require('../src/ingestion/wikidataClient');
const Importer = require('../src/ingestion/importer');
const ImportManager = require('../src/ingestion/importManager');
const QualityReporter = require('../src/ingestion/qualityReporter');
const { DatabaseConnection } = require('../src/db/connection');
const { initSchema } = require('../src/db/schema');
const DatabaseQueries = require('../src/db/queries');

async function runIngestionTests() {
    console.log('==============================================');
    console.log('      COSMO PHASE 2 INGESTION TESTS           ');
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
            console.error(`    ${err.stack || err.message}`);
            failed++;
        }
    }

    // 1. Query Builder Tests
    console.log('[1] Query Builder Tests');
    await test('QueryBuilder contains all 14 required categories', () => {
        const categories = QueryBuilder.getCategories();
        assert.strictEqual(categories.length, 14);
        assert(categories.includes('kpop'));
        assert(categories.includes('international_musicians'));
        assert(categories.includes('bollywood_actors'));
        assert(categories.includes('hollywood_actors'));
        assert(categories.includes('celebrities'));
        assert(categories.includes('athletes'));
        assert(categories.includes('anime_manga'));
        assert(categories.includes('movie_characters'));
        assert(categories.includes('tv_characters'));
        assert(categories.includes('cartoon_characters'));
        assert(categories.includes('video_game_characters'));
        assert(categories.includes('superheroes_villains'));
        assert(categories.includes('historical_figures'));
        assert(categories.includes('notable_entities'));
    });

    await test('QueryBuilder correctly formats LIMIT, OFFSET and sitelinks filter', () => {
        const query = QueryBuilder.buildQuery('anime_manga', {
            limit: 75,
            offset: 150,
            minSitelinks: 5
        });
        assert(query.includes('LIMIT 75'), 'Missing LIMIT 75');
        assert(query.includes('OFFSET 150'), 'Missing OFFSET 150');
        assert(query.includes('FILTER(?sitelinks >= 5)'), 'Missing min sitelinks filter');
        assert(query.includes('ORDER BY DESC(?sitelinks)'), 'Missing ORDER BY DESC(?sitelinks)');
    });

    // 2. Entity Mapper Tests
    console.log('\n[2] Entity Mapper & Quality Scoring Tests');
    await test('EntityMapper extracts properties and deduplicates aliases', () => {
        const mockBinding = {
            item: { value: 'http://www.wikidata.org/entity/Q7982' },
            itemLabel: { value: 'Spider-Man' },
            itemDescription: { value: 'fictional Marvel Comics character' },
            itemAltLabel: { value: 'Peter Parker, Spidey, Spider-Man, Peter Benjamin Parker' },
            sitelinks: { value: '112' },
            genderLabel: { value: 'male' },
            countryLabel: { value: 'United States' },
            image: { value: 'http://upload.wikimedia.org/spiderman.jpg' },
            seriesLabel: { value: 'Marvel Universe' }
        };

        const mapped = EntityMapper.mapBinding(mockBinding, {
            categoryName: 'Superheroes & Villains',
            isFictional: 1
        });

        assert(mapped, 'Mapping returned null');
        assert.strictEqual(mapped.wikidata_id, 'Q7982');
        assert.strictEqual(mapped.name, 'Spider-Man');
        assert.strictEqual(mapped.is_fictional, 1);
        assert.strictEqual(mapped.image_url, 'https://upload.wikimedia.org/spiderman.jpg');
        assert(mapped.popularity_rank < 1000000);

        // Verify aliases do not include primary name and have no duplicates
        assert.deepStrictEqual(mapped.aliases, ['Peter Parker', 'Spidey', 'Peter Benjamin Parker']);

        // Verify structured attributes
        assert.strictEqual(mapped.attributes.is_real, '0');
        assert.strictEqual(mapped.attributes.is_male, '1');
        assert.strictEqual(mapped.attributes.is_superhero, '1');
        assert.strictEqual(mapped.attributes.marvel_universe, '1');
        assert.strictEqual(mapped.attributes.has_superpowers, '1');
        assert.strictEqual(mapped.attributes.from_usa, '1');

        // Verify completeness calculation
        assert(mapped.completenessScore >= 70, `Expected high completeness, got ${mapped.completenessScore}`);
        assert.strictEqual(mapped.is_game_ready, true);
    });

    await test('EntityMapper rejects bindings missing English label or with QID placeholder', () => {
        const invalidBinding = {
            item: { value: 'http://www.wikidata.org/entity/Q999999' },
            itemLabel: { value: 'Q999999' }
        };
        const mapped = EntityMapper.mapBinding(invalidBinding, { categoryName: 'Test', isFictional: 0 });
        assert.strictEqual(mapped, null);
    });

    // 3. Client & Retry Handling Tests
    console.log('\n[3] Wikidata Client Rate Limiting & Retry Tests');
    await test('WikidataClient retries on HTTP 429 / 503 with backoff and succeeds', async () => {
        let attempts = 0;
        const mockFetch = async () => {
            attempts++;
            if (attempts < 3) {
                // Return 429 Too Many Requests on first 2 attempts
                return {
                    ok: false,
                    status: 429,
                    statusText: 'Too Many Requests',
                    headers: { get: () => '0' }
                };
            }
            // Return 200 on 3rd attempt
            return {
                ok: true,
                status: 200,
                json: async () => ({
                    results: {
                        bindings: [{ item: { value: 'http://www.wikidata.org/entity/Q42' } }]
                    }
                })
            };
        };

        const client = new WikidataClient({
            fetchFn: mockFetch,
            minRequestIntervalMs: 50,
            maxRetries: 4
        });

        const bindings = await client.querySparql('SELECT * WHERE {}');
        assert.strictEqual(attempts, 3);
        assert.strictEqual(bindings.length, 1);
    });

    // 4. Batch Database Insertion & Deduplication Tests
    console.log('\n[4] Database Importer & Deduplication Tests');
    const testDb = new DatabaseConnection(':memory:');
    await initSchema(testDb);
    const importer = new Importer(testDb);
    const queries = new DatabaseQueries(testDb);

    await test('Importer saves batch of entities with aliases and attributes in a transaction', async () => {
        const batch = [
            {
                wikidata_id: 'Q20150',
                name: 'Psy',
                description: 'South Korean singer',
                entity_type: 'human',
                category: 'K-pop',
                gender: 'male',
                nationality: 'South Korea',
                birth_date: '1977-12-31',
                is_fictional: 0,
                popularity_rank: 50,
                aliases: ['Park Jae-sang'],
                attributes: {
                    is_real: '1',
                    is_male: '1',
                    is_singer: '1',
                    is_kpop: '1'
                }
            },
            {
                wikidata_id: 'Q11571',
                name: 'Cristiano Ronaldo',
                description: 'Portuguese professional footballer',
                entity_type: 'human',
                category: 'Sports',
                gender: 'male',
                nationality: 'Portugal',
                birth_date: '1985-02-05',
                is_fictional: 0,
                popularity_rank: 2,
                aliases: ['CR7'],
                attributes: {
                    is_real: '1',
                    is_male: '1',
                    is_athlete: '1',
                    plays_football: '1'
                }
            }
        ];

        const summary = await importer.importBatch(batch);
        assert.strictEqual(summary.inserted, 2);
        assert.strictEqual(summary.updated, 0);
        assert.strictEqual(summary.skipped, 0);
        assert(summary.aliasesAdded >= 2);
        assert(summary.attributesAdded >= 6);

        // Verify entity and aliases in DB
        const psy = await queries.getEntityByWikidataId('Q20150');
        assert(psy);
        assert.strictEqual(psy.name, 'Psy');
        assert(psy.aliases.includes('Park Jae-sang'));
    });

    await test('Deduplication: re-importing same batch updates existing records without duplication', async () => {
        const countBefore = (await testDb.get('SELECT COUNT(*) as c FROM entities')).c;

        const batch = [
            {
                wikidata_id: 'Q20150',
                name: 'Psy',
                description: 'South Korean singer and record producer',
                category: 'K-pop',
                is_fictional: 0,
                aliases: ['Park Jae-sang', 'PSY'],
                attributes: {
                    is_real: '1',
                    is_alive: '1'
                }
            }
        ];

        const summary = await importer.importBatch(batch);
        assert.strictEqual(summary.inserted, 0);
        assert.strictEqual(summary.updated, 1);

        const countAfter = (await testDb.get('SELECT COUNT(*) as c FROM entities')).c;
        assert.strictEqual(countAfter, countBefore, 'Entity count should remain unchanged');

        const updatedPsy = await queries.getEntityByWikidataId('Q20150');
        assert.strictEqual(updatedPsy.description, 'South Korean singer and record producer');
        assert(updatedPsy.aliases.includes('PSY'));
    });

    // 5. Interrupted Recovery & Resumability Tests
    console.log('\n[5] Resumable Import Pipeline Tests');
    await test('ImportManager resumes cleanly from interrupted offset', async () => {
        let callCount = 0;
        const mockClient = {
            querySparql: async (sparql) => {
                callCount++;
                if (callCount === 1) {
                    // First page: 2 items
                    return [
                        {
                            item: { value: 'http://www.wikidata.org/entity/Q100' },
                            itemLabel: { value: 'Item 100' },
                            sitelinks: { value: '10' }
                        },
                        {
                            item: { value: 'http://www.wikidata.org/entity/Q101' },
                            itemLabel: { value: 'Item 101' },
                            sitelinks: { value: '12' }
                        }
                    ];
                }
                if (callCount === 2) {
                    // Simulate network failure on 2nd batch
                    throw new Error('Simulated network interruption');
                }
                // Resumed 2nd batch succeeds
                return [
                    {
                        item: { value: 'http://www.wikidata.org/entity/Q102' },
                        itemLabel: { value: 'Item 102' },
                        sitelinks: { value: '8' }
                    }
                ];
            }
        };

        const mgr = new ImportManager({
            db: testDb,
            client: mockClient,
            batchSize: 2
        });

        // First attempt: should fail on 2nd batch and record failed status
        try {
            await mgr.importCategory('celebrities', { targetCount: 4, resume: false });
            assert.fail('Should have thrown on 2nd batch');
        } catch (err) {
            assert.strictEqual(err.message, 'Simulated network interruption');
        }

        const progressAfterFail = await queries.getImportProgress('wikidata_celebrities_v1');
        assert.strictEqual(progressAfterFail.status, 'failed');
        assert.strictEqual(progressAfterFail.last_offset, 2);
        assert.strictEqual(progressAfterFail.total_imported, 2);

        // Resume: should start at offset 2 and succeed!
        const resumeResult = await mgr.importCategory('celebrities', { targetCount: 4, resume: true });
        assert.strictEqual(resumeResult.totalImported, 3);

        const progressAfterResume = await queries.getImportProgress('wikidata_celebrities_v1');
        assert.strictEqual(progressAfterResume.status, 'completed');
    });

    // 6. Quality Report Tests
    console.log('\n[6] Data Quality Reporter Tests');
    await test('QualityReporter computes completeness tiers and category metrics', async () => {
        const reporter = new QualityReporter(testDb);
        const report = await reporter.generateReport();

        assert(report.totalEntities >= 5);
        assert(report.metrics.pctWithUsefulAttributes > 0);
        assert(report.metrics.pctWithAliases > 0);
        assert(Array.isArray(report.qualityTiers));
        assert(Array.isArray(report.categories));
    });

    await testDb.close();

    console.log('\n==============================================');
    console.log(`INGESTION TEST SUMMARY: ${passed} Passed, ${failed} Failed`);
    console.log('==============================================\n');

    if (failed > 0) {
        process.exit(1);
    }
}

runIngestionTests().catch(err => {
    console.error('Fatal error in ingestion test runner:', err);
    process.exit(1);
});

