const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const TOKENS_FILE = path.join(__dirname, '../../data/google_tokens.json');

const SCOPES = [
    'https://www.googleapis.com/auth/calendar.events.readonly',
    'https://www.googleapis.com/auth/tasks.readonly'
];

class GoogleService {
    constructor() {
        this.clientId = process.env.GOOGLE_CLIENT_ID || '';
        this.clientSecret = process.env.GOOGLE_CLIENT_SECRET || '';
        this.redirectUri = process.env.GOOGLE_REDIRECT_URI || 'http://localhost:3000/api/google/callback';
        this.oauthStates = new Map(); // state -> expiresAt (timestamp)
    }

    /**
     * Generate a cryptographically secure OAuth state parameter with a 10-minute TTL.
     */
    generateOAuthState() {
        this._cleanExpiredStates();
        const state = crypto.randomBytes(24).toString('hex');
        this.oauthStates.set(state, Date.now() + (10 * 60 * 1000));
        return state;
    }

    /**
     * Validate and consume an OAuth state parameter (one-time use to protect against CSRF and replay attacks).
     */
    validateOAuthState(state) {
        if (!state || typeof state !== 'string' || state.trim().length === 0) {
            return false;
        }
        this._cleanExpiredStates();
        const expiresAt = this.oauthStates.get(state.trim());
        if (!expiresAt) {
            return false;
        }
        // One-time use: consume immediately
        this.oauthStates.delete(state.trim());
        return Date.now() <= expiresAt;
    }

    _cleanExpiredStates() {
        const now = Date.now();
        for (const [state, expiresAt] of this.oauthStates.entries()) {
            if (now > expiresAt) {
                this.oauthStates.delete(state);
            }
        }
    }

    getTokens() {
        try {
            if (fs.existsSync(TOKENS_FILE)) {
                const data = fs.readFileSync(TOKENS_FILE, 'utf8');
                return JSON.parse(data);
            }
        } catch (err) {
            console.error('[GoogleService] Error reading tokens file:', err.message);
        }
        return null;
    }

    /**
     * Save tokens to file.
     * CRITICAL: Preserves existing refresh_token if new payload does not provide one.
     * Never overwrites a valid refresh token with undefined, null, or empty string.
     */
    saveTokens(tokens) {
        try {
            if (!tokens || typeof tokens !== 'object') {
                return false;
            }

            const existing = this.getTokens();
            if (existing && existing.refresh_token && typeof existing.refresh_token === 'string' && existing.refresh_token.trim().length > 0) {
                if (!tokens.refresh_token || typeof tokens.refresh_token !== 'string' || tokens.refresh_token.trim().length === 0) {
                    tokens.refresh_token = existing.refresh_token.trim();
                }
            }

            const dir = path.dirname(TOKENS_FILE);
            if (!fs.existsSync(dir)) {
                fs.mkdirSync(dir, { recursive: true });
            }
            fs.writeFileSync(TOKENS_FILE, JSON.stringify(tokens, null, 2), 'utf8');
            console.log('[GoogleService] Tokens saved successfully.');
            return true;
        } catch (err) {
            console.error('[GoogleService] Error writing tokens file:', err.message);
            return false;
        }
    }

    getAuthUrl(state) {
        const rootUrl = 'https://accounts.google.com/o/oauth2/v2/auth';
        const options = {
            redirect_uri: this.redirectUri,
            client_id: this.clientId,
            access_type: 'offline',
            response_type: 'code',
            prompt: 'consent',
            scope: SCOPES.join(' ')
        };
        if (state) {
            options.state = state;
        }
        const qs = new URLSearchParams(options);
        return `${rootUrl}?${qs.toString()}`;
    }

    async exchangeCode(code) {
        const tokenUrl = 'https://oauth2.googleapis.com/token';
        const body = new URLSearchParams({
            code,
            client_id: this.clientId,
            client_secret: this.clientSecret,
            redirect_uri: this.redirectUri,
            grant_type: 'authorization_code'
        });

        const res = await fetch(tokenUrl, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body.toString(),
            signal: AbortSignal.timeout(10000)
        });

        const data = await res.json();
        if (!res.ok) {
            throw new Error(data.error_description || data.error || 'Failed to exchange authorization code');
        }

        const existingTokens = this.getTokens();
        // Preserve existing refresh token if new response omits it
        let refreshToken = (data.refresh_token && typeof data.refresh_token === 'string' && data.refresh_token.trim().length > 0)
            ? data.refresh_token.trim()
            : (existingTokens && existingTokens.refresh_token && existingTokens.refresh_token.trim().length > 0 ? existingTokens.refresh_token.trim() : null);

        const tokens = {
            access_token: data.access_token,
            refresh_token: refreshToken,
            expires_at: Date.now() + ((data.expires_in || 3600) * 1000),
            updated_at: new Date().toISOString()
        };

        this.saveTokens(tokens);
        return tokens;
    }

    async getValidAccessToken() {
        let tokens = this.getTokens();
        if (!tokens || !tokens.refresh_token || !tokens.refresh_token.trim()) {
            return null;
        }

        // Return current access token if not expired (with 60-second buffer)
        if (tokens.access_token && tokens.expires_at && Date.now() < (tokens.expires_at - 60000)) {
            return tokens.access_token;
        }

        // Refresh access token
        console.log('[GoogleService] Access token expired, refreshing via Google OAuth...');
        const tokenUrl = 'https://oauth2.googleapis.com/token';
        const body = new URLSearchParams({
            client_id: this.clientId,
            client_secret: this.clientSecret,
            refresh_token: tokens.refresh_token,
            grant_type: 'refresh_token'
        });

        const res = await fetch(tokenUrl, {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: body.toString(),
            signal: AbortSignal.timeout(10000)
        });

        const data = await res.json();
        if (!res.ok) {
            console.error('[GoogleService] Failed to refresh token:', data);
            throw new Error(data.error_description || data.error || 'Token refresh failed');
        }

        tokens.access_token = data.access_token;
        if (data.refresh_token && typeof data.refresh_token === 'string' && data.refresh_token.trim().length > 0) {
            tokens.refresh_token = data.refresh_token.trim();
        }
        tokens.expires_at = Date.now() + ((data.expires_in || 3600) * 1000);
        tokens.updated_at = new Date().toISOString();
        this.saveTokens(tokens);

        return tokens.access_token;
    }

    async getStatus() {
        const tokens = this.getTokens();
        if (!tokens || !tokens.refresh_token || !tokens.refresh_token.trim()) {
            return {
                configured: Boolean(this.clientId && this.clientSecret),
                authenticated: false,
                message: 'No Google account connected. Visit /api/google/auth to authenticate.'
            };
        }

        const isExpired = tokens.expires_at ? Date.now() >= tokens.expires_at : false;
        return {
            configured: true,
            authenticated: true,
            tokenExpiresAt: tokens.expires_at ? new Date(tokens.expires_at).toISOString() : null,
            tokenExpired: isExpired,
            lastUpdated: tokens.updated_at
        };
    }

    async getCalendarEvents(limit = 10) {
        const token = await this.getValidAccessToken();
        if (!token) throw new Error('Not authenticated with Google');

        const now = new Date().toISOString();
        const url = `https://www.googleapis.com/calendar/v3/calendars/primary/events?singleEvents=true&orderBy=startTime&timeMin=${encodeURIComponent(now)}&maxResults=${limit}`;

        const res = await fetch(url, {
            headers: { 'Authorization': `Bearer ${token}` },
            signal: AbortSignal.timeout(8000)
        });

        const data = await res.json();
        if (!res.ok) {
            throw new Error(data.error?.message || 'Failed to fetch calendar events');
        }

        const items = data.items || [];
        return items.map(item => {
            const isAllDay = Boolean(item.start?.date);
            const startStr = item.start?.dateTime || item.start?.date || '';
            const endStr = item.end?.dateTime || item.end?.date || '';

            // Extract HH:MM or date
            let timeDisplay = '';
            if (!isAllDay && item.start?.dateTime) {
                const d = new Date(item.start.dateTime);
                timeDisplay = d.toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit' });
            } else {
                timeDisplay = 'ALL DAY';
            }

            return {
                id: item.id,
                title: item.summary || '(Untitled Event)',
                start: startStr,
                end: endStr,
                time: timeDisplay,
                allDay: isAllDay
            };
        });
    }

    async getTasks(limit = 15) {
        const token = await this.getValidAccessToken();
        if (!token) throw new Error('Not authenticated with Google');

        // Fetch task lists first
        const listsUrl = 'https://tasks.googleapis.com/tasks/v1/users/@me/lists?maxResults=5';
        const listsRes = await fetch(listsUrl, {
            headers: { 'Authorization': `Bearer ${token}` },
            signal: AbortSignal.timeout(8000)
        });
        const listsData = await listsRes.json();
        if (!listsRes.ok) {
            throw new Error(listsData.error?.message || 'Failed to fetch task lists');
        }

        const taskLists = listsData.items || [];
        if (taskLists.length === 0) return [];

        const allTasks = [];
        const now = new Date();

        // Query default / first task list
        for (const list of taskLists.slice(0, 2)) {
            const tasksUrl = `https://tasks.googleapis.com/tasks/v1/lists/${encodeURIComponent(list.id)}/tasks?showCompleted=false&maxResults=${limit}`;
            const tRes = await fetch(tasksUrl, {
                headers: { 'Authorization': `Bearer ${token}` },
                signal: AbortSignal.timeout(8000)
            });
            const tData = await tRes.json();
            if (tRes.ok && tData.items) {
                for (const t of tData.items) {
                    if (t.title && t.status !== 'completed') {
                        let isOverdue = false;
                        let dueFormatted = '';
                        if (t.due) {
                            const dueDate = new Date(t.due);
                            isOverdue = dueDate < now;
                            dueFormatted = dueDate.toLocaleDateString('en-GB', { month: 'short', day: 'numeric' });
                        }

                        allTasks.push({
                            id: t.id,
                            listTitle: list.title,
                            title: t.title,
                            due: t.due || '',
                            dueFormatted: dueFormatted || 'NO DUE DATE',
                            completed: t.status === 'completed',
                            overdue: isOverdue
                        });
                    }
                }
            }
        }

        return allTasks.slice(0, limit);
    }

    /**
     * Get combined reminders from Calendar & Tasks with explicit status handling.
     * Distinguishes:
     * - 'ok': Complete success (both succeeded)
     * - 'partial': Partial success (one succeeded, one failed)
     * - 'unauthenticated': Authentication missing or expired
     * - 'error': API failure (both failed or unexpected error)
     *
     * Preserves: events, tasks, updatedAt, eventsCount, tasksCount for firmware compatibility.
     */
    async getCombinedReminders() {
        const status = await this.getStatus();
        if (!status.authenticated) {
            return {
                status: 'unauthenticated',
                message: 'Google account not linked',
                authUrl: '/api/google/auth',
                updatedAt: new Date().toISOString(),
                eventsCount: 0,
                tasksCount: 0,
                events: [],
                tasks: []
            };
        }

        let events = [];
        let tasks = [];
        let eventsError = null;
        let tasksError = null;

        const [eventsResult, tasksResult] = await Promise.allSettled([
            this.getCalendarEvents(8),
            this.getTasks(10)
        ]);

        if (eventsResult.status === 'fulfilled') {
            events = eventsResult.value;
        } else {
            eventsError = eventsResult.reason?.message || 'Calendar events fetch failed';
            console.error('[GoogleService] Events fetch error:', eventsError);
        }

        if (tasksResult.status === 'fulfilled') {
            tasks = tasksResult.value;
        } else {
            tasksError = tasksResult.reason?.message || 'Tasks fetch failed';
            console.error('[GoogleService] Tasks fetch error:', tasksError);
        }

        const isAuthError = (err) => err && (
            err.includes('Not authenticated') ||
            err.includes('invalid_grant') ||
            err.includes('Invalid Credentials') ||
            err.includes('Token refresh failed')
        );

        // Authentication failure during request
        if (eventsError && tasksError && (isAuthError(eventsError) || isAuthError(tasksError))) {
            return {
                status: 'unauthenticated',
                message: 'Google authentication expired or invalid. Please re-authenticate.',
                authUrl: '/api/google/auth',
                updatedAt: new Date().toISOString(),
                eventsCount: 0,
                tasksCount: 0,
                events: [],
                tasks: [],
                errors: { calendar: eventsError, tasks: tasksError }
            };
        }

        // Both services failed due to API error
        if (eventsError && tasksError) {
            return {
                status: 'error',
                message: `Failed to fetch reminders: Calendar (${eventsError}), Tasks (${tasksError})`,
                updatedAt: new Date().toISOString(),
                eventsCount: 0,
                tasksCount: 0,
                events: [],
                tasks: [],
                errors: { calendar: eventsError, tasks: tasksError }
            };
        }

        // Partial success: one service failed, one succeeded
        if (eventsError || tasksError) {
            const failedName = eventsError ? 'Calendar' : 'Tasks';
            const failReason = eventsError || tasksError;
            return {
                status: 'partial',
                message: `Partial reminders sync: ${failedName} failed (${failReason})`,
                updatedAt: new Date().toISOString(),
                eventsCount: events.length,
                tasksCount: tasks.length,
                events,
                tasks,
                errors: {
                    calendar: eventsError || null,
                    tasks: tasksError || null
                }
            };
        }

        // Complete success
        return {
            status: 'ok',
            updatedAt: new Date().toISOString(),
            eventsCount: events.length,
            tasksCount: tasks.length,
            events,
            tasks
        };
    }
}

module.exports = new GoogleService();
