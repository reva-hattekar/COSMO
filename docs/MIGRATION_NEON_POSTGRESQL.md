# COSMO Backend – Neon PostgreSQL Migration & Render Deployment Guide

This guide details the completed migration of the COSMO Node.js backend from local SQLite to **Neon PostgreSQL** with connection pooling enabled, deployed on **Render Free**.

---

## 1. Neon Project Configuration

- **Project Name**: `cosmo-database`
- **Database Name**: `neondb`
- **Branch**: `production`
- **Connection Pooling**: Enabled (PgBouncer)
- **SSL**: `sslmode=require` / `rejectUnauthorized: false` (supported automatically by backend pool)

### Neon Connection String Format
In the Neon Console, copy the **Pooled connection string**:
```text
postgresql://neondb_owner:<PASSWORD>@<EP_HOST>-pooler.us-east-2.aws.neon.tech/neondb?sslmode=require
```

---

## 2. Schema Comparison & Adaptations

| Component | SQLite | Neon PostgreSQL | Adaptation Details |
| :--- | :--- | :--- | :--- |
| **Primary Keys** | `INTEGER PRIMARY KEY AUTOINCREMENT` | `SERIAL PRIMARY KEY` | Automated sequence generation with `SERIAL` |
| **Timestamps** | `DATETIME DEFAULT CURRENT_TIMESTAMP` | `TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP` | Timezone-aware timestamp storage |
| **Parameter Placeholders** | `?` | `$1, $2, $3, ...` | Automated translation in `PostgresDatabaseConnection` |
| **Case-Insensitive Search** | `name COLLATE NOCASE` | `LOWER(name)`, `ILIKE` | Functional indexes: `idx_entities_name ON entities(LOWER(name))` |
| **Deduplication / Conflict** | `INSERT OR IGNORE INTO aliases` | `ON CONFLICT (entity_id, alias) DO NOTHING` | Standard PostgreSQL conflict handling |
| **Scalar Minimum** | `MIN(popularity_rank, ?)` | `LEAST(popularity_rank, $1)` | Automatically translated in query adapter |
| **Updated Timestamp** | SQLite table trigger | PL/pgSQL function & BEFORE UPDATE trigger | `update_updated_at_column()` |
| **OAuth Token Storage** | Local JSON file | `google_tokens` table | Persistent database table with atomic UPSERT |

---

## 3. Google OAuth Token Persistence in PostgreSQL

Tokens are now persistently stored in the `google_tokens` table on Neon:

```sql
CREATE TABLE IF NOT EXISTS google_tokens (
    id VARCHAR(32) PRIMARY KEY DEFAULT 'default',
    access_token TEXT,
    refresh_token TEXT,
    expires_at BIGINT,
    updated_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);
```

### Refresh Token Preservation Guarantee
When refreshing or exchanging OAuth codes, PostgreSQL performs an atomic UPSERT:
```sql
INSERT INTO google_tokens (id, access_token, refresh_token, expires_at, updated_at)
VALUES ('default', $1, $2, $3, CURRENT_TIMESTAMP)
ON CONFLICT (id) DO UPDATE SET
    access_token = EXCLUDED.access_token,
    refresh_token = COALESCE(NULLIF(EXCLUDED.refresh_token, ''), google_tokens.refresh_token),
    expires_at = EXCLUDED.expires_at,
    updated_at = CURRENT_TIMESTAMP;
```
If Google omits a new `refresh_token`, the database guarantees the existing valid refresh token is never overwritten or lost.

---

## 4. Running the SQLite to Neon Data Migration

Before deploying to production, migrate your existing SQLite knowledge base and OAuth tokens to Neon PostgreSQL using the provided automated script:

```bash
DATABASE_URL="postgresql://neondb_owner:<PASSWORD>@<HOST>-pooler.us-east-2.aws.neon.tech/neondb?sslmode=require" npm run migrate:neon
```

### What the Migration Script Does:
1. Connects to the local SQLite database (`data/cosmo.db`).
2. Applies `src/db/schema.postgres.sql` to Neon PostgreSQL.
3. Migrates `categories`, `attributes`, `entities`, `entity_attributes`, and `aliases` in strict foreign-key order using transactions.
4. Migrates existing Google OAuth tokens from `data/google_tokens.json` into the `google_tokens` table.
5. Synchronizes all PostgreSQL `SERIAL` sequences (`setval`) to prevent ID conflicts with future inserts.
6. Runs a verification audit comparing row counts between SQLite and PostgreSQL.

---

## 5. Deploying to Render Free

1. Go to [Render Dashboard](https://dashboard.render.com/) and click **New +** $\rightarrow$ **Web Service**.
2. Connect your GitHub repository: `reva-hattekar/COSMO`.
3. Configure service settings:
   - **Name**: `cosmo-backend`
   - **Environment**: `Node`
   - **Branch**: `main`
   - **Build Command**: `npm install`
   - **Start Command**: `npm start`
   - **Plan**: `Free`

4. Add the following **Environment Variables** in the **Environment** tab:

| Variable | Value / Format | Purpose |
| :--- | :--- | :--- |
| `DATABASE_URL` | `postgresql://neondb_owner:<PASS>@<HOST>-pooler.us-east-2.aws.neon.tech/neondb?sslmode=require` | Pooled Neon PostgreSQL connection |
| `NODE_ENV` | `production` | Production mode |
| `COSMO_API_KEY` | *(Optional random secret)* | Bearer authentication key for private routes |
| `GOOGLE_CLIENT_ID` | `<ID>.apps.googleusercontent.com` | Google Cloud Console OAuth 2.0 Client ID |
| `GOOGLE_CLIENT_SECRET` | `GOCSPX-...` | Google Cloud Console OAuth 2.0 Secret |
| `GOOGLE_REDIRECT_URI` | `https://<YOUR-RENDER-URL>.onrender.com/api/google/callback` | OAuth redirect URI |

5. Copy your Render service URL (`https://<service-name>.onrender.com`) and update:
   - `GOOGLE_REDIRECT_URI` in Render Environment Variables.
   - Authorized Redirect URIs in the Google Cloud Console.

6. Verify deployment by visiting:
   - Health check: `https://<YOUR-RENDER-URL>.onrender.com/health` (should return `"databaseEngine": "postgres"`)
   - Knowledge stats: `https://<YOUR-RENDER-URL>.onrender.com/api/database/stats`
   - Google authentication: `https://<YOUR-RENDER-URL>.onrender.com/api/google/auth`
