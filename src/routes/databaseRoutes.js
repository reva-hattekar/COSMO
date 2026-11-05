const express = require('express');
const DatabaseQueries = require('../db/queries');
const { getDatabase } = require('../db/connection');

const router = express.Router();
const queries = new DatabaseQueries(getDatabase());

/**
 * GET /api/database/stats
 * Returns overall statistics about the knowledge base.
 */
router.get('/stats', async (req, res) => {
    try {
        const stats = await queries.getDatabaseStats();
        res.json({
            status: 'ok',
            data: stats
        });
    } catch (error) {
        console.error('Stats error:', error);
        res.status(500).json({
            status: 'error',
            message: 'Failed to retrieve database statistics',
            details: error.message
        });
    }
});

/**
 * GET /api/database/categories
 * Returns all categories with entity counts.
 */
router.get('/categories', async (req, res) => {
    try {
        const categories = await queries.getCategories();
        res.json({
            status: 'ok',
            data: categories
        });
    } catch (error) {
        console.error('Categories error:', error);
        res.status(500).json({
            status: 'error',
            message: 'Failed to retrieve categories',
            details: error.message
        });
    }
});

/**
 * GET /api/database/search?q=...
 * Search entities by name, alias, or description.
 */
router.get('/search', async (req, res) => {
    try {
        const query = req.query.q;
        if (!query || typeof query !== 'string' || query.trim().length === 0) {
            return res.status(400).json({
                status: 'error',
                message: 'Query parameter "q" is required'
            });
        }

        const limit = Math.min(Math.max(parseInt(req.query.limit, 10) || 20, 1), 100);
        const results = await queries.searchEntities(query.trim(), limit);

        res.json({
            status: 'ok',
            query: query.trim(),
            count: results.length,
            data: results
        });
    } catch (error) {
        console.error('Search error:', error);
        res.status(500).json({
            status: 'error',
            message: 'Failed to search entities',
            details: error.message
        });
    }
});

module.exports = router;

