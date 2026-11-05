# Google OAuth 2.0 Setup Guide for COSMO Reminders

This guide outlines the configuration of Google Cloud OAuth 2.0 to power COSMO's read-only Google Calendar and Google Tasks dashboard.

---

## 1. Google Cloud Console Configuration

### A. APIs Enabled
Ensure the following APIs are enabled in your Google Cloud Project:
- **Google Calendar API**
- **Google Tasks API**

### B. OAuth Consent Screen
1. **User Type**: External
2. **App Name**: COSMO
3. **User Support Email**: Your / brother's Google email
4. **Scopes Added**:
   - `https://www.googleapis.com/auth/calendar.events.readonly`
   - `https://www.googleapis.com/auth/tasks.readonly`
5. **Test Users**:
   - Add your brother's Google email under **Test users**.

---

## 2. OAuth Web Client Creation

1. Navigate to **APIs & Services > Credentials**.
2. Click **Create Credentials > OAuth client ID**.
3. **Application type**: Web application.
4. **Name**: `COSMO Web Client`.
5. **Authorized redirect URIs**:
   - For local development: `http://localhost:3000/api/google/callback`
   - For Railway production: `https://<your-railway-domain>.up.railway.app/api/google/callback`
6. Click **Create** and save the `Client ID` and `Client Secret`.

---

## 3. Important: The 7-Day Testing Mode Limitation

> [!WARNING]
> **Why Testing Mode Tokens Expire After 7 Days**
> When a Google Cloud OAuth app has **Publishing Status = Testing** and **User Type = External**, Google enforces a security policy where **all issued refresh tokens expire automatically after 7 calendar days**.
>
> **Implications for COSMO**:
> If the backend remains in Testing mode, the refresh token will become invalid after 7 days, causing `/api/google/reminders` to return `status: "unauthenticated"`, requiring re-authorization via `/api/google/auth`.

### Recommended Production Solution
1. Once testing is verified, navigate to **APIs & Services > OAuth consent screen**.
2. Click **Publish App** (moves status from **Testing** to **In production**).
3. **Does it require Google verification?**
   - Because COSMO uses only read-only scopes (`calendar.events.readonly`, `tasks.readonly`) for personal use and less than 100 users, Google allows the app to run in production for personal accounts with an unverified app consent warning ("Google hasn't verified this app" -> click Advanced -> Proceed).
   - Once published to production, refresh tokens **never expire automatically** unless manually revoked by the user.

---

## 4. First-Time Authentication Flow
1. Start the backend (locally or on Railway).
2. Open your browser and navigate to:
   `http://localhost:3000/api/google/auth` (or your Railway domain equivalent).
3. Sign in with your brother's Google account and grant read-only permissions.
4. You will be redirected to `/api/google/callback`, which stores the persistent refresh token.
5. COSMO's `/api/google/reminders` endpoint is now active and will automatically refresh access tokens in the background.

