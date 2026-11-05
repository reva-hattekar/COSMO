# COSMO Backend – Railway Deployment Guide

This guide explains how to deploy the COSMO Node.js backend to [Railway](https://railway.app) so that COSMO can run independently 24/7 without requiring your local machine.

---

## 1. Prerequisites
- A Railway account ([railway.app](https://railway.app)).
- The [COSMO GitHub repository](https://github.com/reva-hattekar/COSMO).

---

## 2. Deploying to Railway

1. **Create New Project**:
   - In Railway Dashboard, click **New Project**.
   - Select **Deploy from GitHub repo**.
   - Choose `reva-hattekar/COSMO`.

2. **Configure Environment Variables**:
   In Railway project settings, go to the **Variables** tab and add:
   ```env
   NODE_ENV=production
   PORT=3000
   DATABASE_PATH=/app/data/cosmo_knowledge.db
   GOOGLE_CLIENT_ID=your_google_client_id.apps.googleusercontent.com
   GOOGLE_CLIENT_SECRET=your_google_client_secret
   GOOGLE_REDIRECT_URI=https://<your-railway-domain>.up.railway.app/api/google/callback
   ```
   *(Note: Set `GOOGLE_REDIRECT_URI` after Railway assigns your public domain in step 3).*

3. **Generate Public Domain**:
   - In Railway project settings, navigate to **Networking**.
   - Click **Generate Domain** (e.g. `cosmo-production-xxxx.up.railway.app`).
   - Copy this domain and update `GOOGLE_REDIRECT_URI` in both Railway variables and your Google Cloud Console OAuth Authorized Redirect URIs.

4. **Persistent Volume (Optional but Recommended for SQLite & Tokens)**:
   - Add a Railway Volume mounted at `/app/data`.
   - This ensures your SQLite knowledge database and OAuth refresh tokens persist across deployments.

5. **Healthcheck Configuration**:
   - Railway automatically monitors your app.
   - Built-in healthcheck path: `/health` (returns HTTP 200 with online status and database connectivity).

---

## 3. Endpoints Available on Railway
- `GET /health` – Service health & database connectivity
- `POST /api/game/start` – Start Akinator session
- `POST /api/game/:id/answer` – Submit Akinator answer
- `GET /api/google/auth` – Initiate Google OAuth flow
- `GET /api/google/callback` – Google OAuth redirect callback
- `GET /api/google/status` – Google authentication status
- `GET /api/google/reminders` – Compact reminders feed for COSMO ESP32
