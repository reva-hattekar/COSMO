const fs = require('fs');
const path = require('path');

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

    saveTokens(tokens) {
        try {
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

    getAuthUrl() {
        const rootUrl = 'https://accounts.google.com/o/oauth2/v2/auth';
        const options = {
            redirect_uri: this.redirectUri,
            client_id: this.clientId,
            access_type: 'offline',
            response_type: 'code',
            prompt: 'consent',
            scope: SCOPES.join(' ')
        };
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
            body: body.toString()
        });

        const data = await res.json();
        if (!res.ok) {
            throw new Error(data.error_description || data.error || 'Failed to exchange authorization code');
        }

        const tokens = {
            access_token: data.access_token,
            refresh_token: data.refresh_token,
            expires_at: Date.now() + ((data.expires_in || 3600) * 1000),
            updated_at: new Date().toISOString()
        };

        this.saveTokens(tokens);
        return tokens;
    }

    async getValidAccessToken() {
        let tokens = this.getTokens();
        if (!tokens || !tokens.refresh_token) {
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
            body: body.toString()
        });

        const data = await res.json();
        if (!res.ok) {
            console.error('[GoogleService] Failed to refresh token:', data);
            throw new Error(data.error_description || data.error || 'Token refresh failed');
        }

        tokens.access_token = data.access_token;
        tokens.expires_at = Date.now() + ((data.expires_in || 3600) * 1000);
        tokens.updated_at = new Date().toISOString();
        this.saveTokens(tokens);

        return tokens.access_token;
    }

    async getStatus() {
        const tokens = this.getTokens();
        if (!tokens || !tokens.refresh_token) {
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
            headers: { 'Authorization': `Bearer ${token}` }
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
            headers: { 'Authorization': `Bearer ${token}` }
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
                headers: { 'Authorization': `Bearer ${token}` }
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

    async getCombinedReminders() {
        const status = await this.getStatus();
        if (!status.authenticated) {
            return {
                status: 'unauthenticated',
                message: 'Google account not linked',
                authUrl: '/api/google/auth',
                updatedAt: new Date().toISOString(),
                events: [],
                tasks: []
            };
        }

        try {
            const [events, tasks] = await Promise.all([
                this.getCalendarEvents(8).catch(e => {
                    console.error('[GoogleService] Events fetch error:', e.message);
                    return [];
                }),
                this.getTasks(10).catch(e => {
                    console.error('[GoogleService] Tasks fetch error:', e.message);
                    return [];
                })
            ]);

            return {
                status: 'ok',
                updatedAt: new Date().toISOString(),
                eventsCount: events.length,
                tasksCount: tasks.length,
                events,
                tasks
            };
        } catch (err) {
            return {
                status: 'error',
                message: err.message,
                updatedAt: new Date().toISOString(),
                events: [],
                tasks: []
            };
        }
    }
}

module.exports = new GoogleService();
