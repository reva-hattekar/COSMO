const { getDatabase } = require('./connection');
const { initSchema } = require('./schema');
const DatabaseQueries = require('./queries');
const { categories, attributes, entities } = require('./seedData');

async function seedDatabase(customDb = null) {
    const db = customDb || getDatabase();
    await initSchema(db);
    const queries = new DatabaseQueries(db);

    console.log('--- SEEDING DATABASE ---');

    await db.transaction(async () => {
        // 1. Seed Categories
        console.log(`Seeding ${categories.length} categories...`);
        for (const cat of categories) {
            await queries.upsertCategory(cat);
        }

        // 2. Seed Attributes
        console.log(`Seeding ${attributes.length} attributes...`);
        for (const attr of attributes) {
            await queries.upsertAttribute(attr);
        }

        // 3. Seed Entities, Attributes & Aliases
        console.log(`Seeding ${entities.length} entities...`);
        for (const entity of entities) {
            const { attributes: entityAttrs, aliases, ...entityData } = entity;
            
            const { id: entityId } = await queries.upsertEntity(entityData);

            // Add aliases
            if (aliases && Array.isArray(aliases)) {
                for (const alias of aliases) {
                    await queries.addEntityAlias(entityId, alias);
                }
            }

            // Add attributes
            if (entityAttrs) {
                for (const [key, val] of Object.entries(entityAttrs)) {
                    await queries.setEntityAttributeByKey(entityId, key, val, 1.0, 'seed');
                }
            }
        }
    });

    const stats = await queries.getDatabaseStats();
    console.log('Seeding completed successfully!');
    console.log(`- Entities: ${stats.totalEntities}`);
    console.log(`- Attributes: ${stats.totalAttributes}`);
    console.log(`- Entity Attributes: ${stats.totalEntityAttributes}`);
    console.log(`- Aliases: ${stats.totalAliases}`);
    console.log(`- Categories: ${stats.categories.length}`);

    return stats;
}

if (require.main === module) {
    seedDatabase()
        .then(() => process.exit(0))
        .catch((err) => {
            console.error('Seed error:', err);
            process.exit(1);
        });
}

module.exports = { seedDatabase };

