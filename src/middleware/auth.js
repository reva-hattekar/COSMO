const crypto = require('crypto');

/**
 * Middleware to enforce COSMO_API_KEY authentication on private endpoints.
 *
 * Behavior:
 * - If COSMO_API_KEY is not configured or empty, requests are permitted (backwards compatibility).
 * - When COSMO_API_KEY is configured, requires:
 *     Authorization: Bearer <COSMO_API_KEY>
 *   or
 *     x-api-key: <COSMO_API_KEY>
 * - Uses timingSafeEqual to avoid timing attack vulnerabilities.
 * - Never reflects or logs the API key.
 */
function requireApiKey(req, res, next) {
    const configuredKey = process.env.COSMO_API_KEY;
    if (!configuredKey || typeof configuredKey !== 'string' || configuredKey.trim().length === 0) {
        return next();
    }

    const authHeader = req.headers['authorization'];
    const xApiKey = req.headers['x-api-key'];

    let providedToken = null;
    if (authHeader && typeof authHeader === 'string' && authHeader.startsWith('Bearer ')) {
        providedToken = authHeader.substring(7).trim();
    } else if (xApiKey && typeof xApiKey === 'string') {
        providedToken = xApiKey.trim();
    }

    if (!providedToken) {
        return res.status(401).json({
            status: 'error',
            error: 'Unauthorized',
            message: 'API key required. Provide Bearer token in Authorization header.'
        });
    }

    const expectedBuf = Buffer.from(configuredKey.trim());
    const actualBuf = Buffer.from(providedToken);

    if (expectedBuf.length !== actualBuf.length || !crypto.timingSafeEqual(expectedBuf, actualBuf)) {
        return res.status(403).json({
            status: 'error',
            error: 'Forbidden',
            message: 'Invalid API key.'
        });
    }

    next();
}

module.exports = { requireApiKey };
