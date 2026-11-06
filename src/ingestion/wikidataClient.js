/**
 * Wikidata SPARQL API Client
 * Features:
 * - Wikimedia compliant User-Agent
 * - Rate limiting to avoid overwhelming Wikimedia endpoints
 * - Exponential backoff with jitter on 429 / 5xx errors
 * - Detailed error reporting and logging
 */

class WikidataClient {
    constructor(options = {}) {
        this.endpoint = options.endpoint || 'https://query.wikidata.org/sparql';
        this.userAgent = options.userAgent || 'COSMO-Engine/1.0 (https://github.com/reva0/COSMO; contact: cosmo-project@local) Node-Fetch/20';
        this.minRequestIntervalMs = options.minRequestIntervalMs || 1200; // 1.2s delay between requests
        this.maxRetries = options.maxRetries !== undefined ? options.maxRetries : 4;
        this.timeoutMs = options.timeoutMs || 45000;
        this.lastRequestTime = 0;
        this.fetchFn = options.fetchFn || globalThis.fetch;
    }

    /**
     * Enforce rate limit delay between successive calls.
     */
    async _throttle() {
        const now = Date.now();
        const elapsed = now - this.lastRequestTime;
        if (elapsed < this.minRequestIntervalMs) {
            const waitTime = this.minRequestIntervalMs - elapsed;
            await new Promise(resolve => setTimeout(resolve, waitTime));
        }
        this.lastRequestTime = Date.now();
    }

    /**
     * Execute a SPARQL query with retries and exponential backoff.
     */
    async querySparql(sparql, options = {}) {
        const maxRetries = options.maxRetries !== undefined ? options.maxRetries : this.maxRetries;
        let attempt = 0;

        while (attempt <= maxRetries) {
            attempt++;
            await this._throttle();

            const url = `${this.endpoint}?format=json&query=${encodeURIComponent(sparql)}`;

            try {
                const controller = new AbortController();
                const timeoutId = setTimeout(() => controller.abort(), this.timeoutMs);

                const response = await this.fetchFn(url, {
                    method: 'GET',
                    headers: {
                        'User-Agent': this.userAgent,
                        'Accept': 'application/sparql-results+json'
                    },
                    signal: controller.signal
                });

                clearTimeout(timeoutId);

                if (response.ok) {
                    const data = await response.json();
                    if (!data.results || !Array.isArray(data.results.bindings)) {
                        throw new Error('Invalid SPARQL JSON structure: missing results.bindings');
                    }
                    return data.results.bindings;
                }

                // Handle rate limiting (429) or transient server errors (500, 502, 503, 504)
                if (response.status === 429 || (response.status >= 500 && response.status <= 504)) {
                    if (attempt > maxRetries) {
                        throw new Error(`Wikidata SPARQL failed after ${maxRetries} retries: HTTP ${response.status} ${response.statusText}`);
                    }

                    // Check for Retry-After header
                    const retryAfterHeader = response.headers.get('retry-after');
                    let backoffMs = retryAfterHeader ? parseInt(retryAfterHeader, 10) * 1000 : Math.pow(2, attempt) * 1500;
                    backoffMs += Math.floor(Math.random() * 800); // add jitter

                    console.warn(`[WikidataClient] HTTP ${response.status} received. Backing off for ${Math.round(backoffMs)}ms (Attempt ${attempt}/${maxRetries})...`);
                    await new Promise(resolve => setTimeout(resolve, backoffMs));
                    continue;
                }

                // Non-retriable error (e.g. 400 Bad Request, query syntax error)
                const errorText = await response.text();
                throw new Error(`Wikidata SPARQL Query Error (HTTP ${response.status}): ${errorText.slice(0, 300)}`);

            } catch (err) {
                if (err.name === 'AbortError') {
                    if (attempt > maxRetries) {
                        throw new Error(`Wikidata SPARQL query timed out after ${this.timeoutMs}ms (attempt ${attempt})`);
                    }
                    const backoffMs = Math.pow(2, attempt) * 2000;
                    console.warn(`[WikidataClient] Query timed out. Retrying in ${backoffMs}ms (Attempt ${attempt}/${maxRetries})...`);
                    await new Promise(resolve => setTimeout(resolve, backoffMs));
                    continue;
                }

                if (attempt > maxRetries || err.message.includes('Query Error')) {
                    throw err;
                }

                const backoffMs = Math.pow(2, attempt) * 1500 + Math.floor(Math.random() * 500);
                console.warn(`[WikidataClient] Network error: ${err.message}. Retrying in ${backoffMs}ms...`);
                await new Promise(resolve => setTimeout(resolve, backoffMs));
            }
        }

        throw new Error(`Wikidata SPARQL failed after ${maxRetries} attempts.`);
    }
}

module.exports = WikidataClient;

