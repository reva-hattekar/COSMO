/**
 * Data Quality Reporter
 * Audits the COSMO knowledge base and computes completeness, coverage,
 * and game-readiness metrics across all categories.
 */

const { getDatabase } = require('../db/connection');

class QualityReporter {
    constructor(db = null) {
        this.db = db || getDatabase();
    }

    /**
     * Generate an audit report of the database quality.
     */
    async generateReport() {
        const [
            entityStats,
            categoryStats,
            attributeCoverage,
            importProgressStats
        ] = await Promise.all([
            // Entity overall counts and field presence
            this.db.get(`
                SELECT 
                    COUNT(*) as total_entities,
                    COUNT(wikidata_id) as with_wikidata_id,
                    COUNT(description) as with_description,
                    COUNT(image_url) as with_image,
                    COUNT(gender) as with_gender,
                    COUNT(nationality) as with_nationality,
                    COUNT(birth_date) as with_birth_date,
                    COUNT(death_date) as with_death_date,
                    SUM(CASE WHEN is_fictional = 1 THEN 1 ELSE 0 END) as fictional_count,
                    SUM(CASE WHEN is_fictional = 0 THEN 1 ELSE 0 END) as real_count
                FROM entities
            `),

            // Per category breakdown
            this.db.all(`
                SELECT 
                    category,
                    COUNT(*) as count,
                    ROUND(AVG(CASE WHEN description IS NOT NULL THEN 1.0 ELSE 0.0 END) * 100, 1) as pct_description,
                    ROUND(AVG(CASE WHEN image_url IS NOT NULL THEN 1.0 ELSE 0.0 END) * 100, 1) as pct_image
                FROM entities
                GROUP BY category
                ORDER BY count DESC
            `),

            // Entities with attributes and aliases
            this.db.get(`
                SELECT 
                    COUNT(DISTINCT ea.entity_id) as entities_with_attributes,
                    COUNT(DISTINCT a.entity_id) as entities_with_aliases,
                    (SELECT COUNT(*) FROM entity_attributes) as total_entity_attributes,
                    (SELECT COUNT(*) FROM aliases) as total_aliases
                FROM entities e
                LEFT JOIN entity_attributes ea ON ea.entity_id = e.id
                LEFT JOIN aliases a ON a.entity_id = e.id
            `),

            // Import tracking totals
            this.db.all(`
                SELECT 
                    category,
                    batch_name,
                    total_fetched,
                    total_imported,
                    total_skipped,
                    status,
                    error_message
                FROM import_progress
            `)
        ]);

        const total = entityStats.total_entities || 0;

        // Completeness distribution
        // Score each entity: description (+20), image (+20), gender/nat (+20), birth/death (+20), >=3 attrs (+20)
        const scoreDistribution = await this.db.all(`
            SELECT 
                CASE 
                    WHEN score >= 80 THEN 'High (>=80%)'
                    WHEN score >= 50 THEN 'Medium (50-79%)'
                    ELSE 'Low (<50%)'
                END as quality_tier,
                COUNT(*) as count,
                ROUND(COUNT(*) * 100.0 / ${total > 0 ? total : 1}, 1) as percentage
            FROM (
                SELECT 
                    e.id,
                    (
                        (CASE WHEN e.description IS NOT NULL THEN 20 ELSE 0 END) +
                        (CASE WHEN e.image_url IS NOT NULL THEN 20 ELSE 0 END) +
                        (CASE WHEN e.gender IS NOT NULL OR e.nationality IS NOT NULL THEN 20 ELSE 0 END) +
                        (CASE WHEN e.birth_date IS NOT NULL OR e.debut_year IS NOT NULL THEN 20 ELSE 0 END) +
                        (CASE WHEN (SELECT COUNT(*) FROM entity_attributes ea WHERE ea.entity_id = e.id) >= 3 THEN 20 ELSE 0 END)
                    ) as score
                FROM entities e
            )
            GROUP BY quality_tier
            ORDER BY count DESC
        `);

        const totalSkipped = importProgressStats.reduce((sum, r) => sum + (r.total_skipped || 0), 0);
        const failedBatches = importProgressStats.filter(r => r.status === 'failed');

        return {
            totalEntities: total,
            totalUniqueWikidataIds: entityStats.with_wikidata_id || 0,
            metrics: {
                pctWithDescription: total > 0 ? Number(((entityStats.with_description / total) * 100).toFixed(1)) : 0,
                pctWithImage: total > 0 ? Number(((entityStats.with_image / total) * 100).toFixed(1)) : 0,
                pctWithGender: total > 0 ? Number(((entityStats.with_gender / total) * 100).toFixed(1)) : 0,
                pctWithNationality: total > 0 ? Number(((entityStats.with_nationality / total) * 100).toFixed(1)) : 0,
                pctWithDates: total > 0 ? Number(((entityStats.with_birth_date / total) * 100).toFixed(1)) : 0,
                pctWithUsefulAttributes: total > 0 ? Number(((attributeCoverage.entities_with_attributes / total) * 100).toFixed(1)) : 0,
                pctWithAliases: total > 0 ? Number(((attributeCoverage.entities_with_aliases / total) * 100).toFixed(1)) : 0,
                avgAttributesPerEntity: total > 0 ? Number((attributeCoverage.total_entity_attributes / total).toFixed(1)) : 0
            },
            fictionalRealRatio: {
                realPeople: entityStats.real_count || 0,
                fictionalCharacters: entityStats.fictional_count || 0
            },
            categories: categoryStats,
            qualityTiers: scoreDistribution,
            pipelineStats: {
                totalSkippedOrDuplicateRecords: totalSkipped,
                failedBatchesCount: failedBatches.length,
                failedBatches: failedBatches
            }
        };
    }
}

module.exports = QualityReporter;

