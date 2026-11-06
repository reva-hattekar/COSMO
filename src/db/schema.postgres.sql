-- =======================================================
-- COSMO Knowledge Engine & Reminders Schema for PostgreSQL
-- Optimized for Neon PostgreSQL with Connection Pooling
-- =======================================================

-- 1. Categories
CREATE TABLE IF NOT EXISTS categories (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL UNIQUE,
    parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    description TEXT,
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- 2. Entities
CREATE TABLE IF NOT EXISTS entities (
    id SERIAL PRIMARY KEY,
    wikidata_id VARCHAR(64) UNIQUE,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    entity_type VARCHAR(64) NOT NULL DEFAULT 'human',
    category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    category VARCHAR(255) NOT NULL,
    subcategory VARCHAR(255),
    image_url TEXT,
    gender VARCHAR(64),
    nationality VARCHAR(128),
    birth_date VARCHAR(64),
    death_date VARCHAR(64),
    debut_year INTEGER,
    is_fictional INTEGER NOT NULL DEFAULT 0,
    popularity_rank INTEGER DEFAULT 1000000,
    source VARCHAR(64) DEFAULT 'wikidata',
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- 3. Attributes
CREATE TABLE IF NOT EXISTS attributes (
    id SERIAL PRIMARY KEY,
    attribute_key VARCHAR(128) NOT NULL UNIQUE,
    attribute_name VARCHAR(255) NOT NULL,
    data_type VARCHAR(64) NOT NULL DEFAULT 'boolean',
    question_template TEXT NOT NULL,
    category_scope VARCHAR(255),
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- 4. Entity Attributes
CREATE TABLE IF NOT EXISTS entity_attributes (
    entity_id INTEGER NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    attribute_id INTEGER NOT NULL REFERENCES attributes(id) ON DELETE CASCADE,
    attribute_value TEXT NOT NULL,
    confidence REAL NOT NULL DEFAULT 1.0,
    source VARCHAR(64) DEFAULT 'wikidata',
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (entity_id, attribute_id, attribute_value)
);

-- 5. Aliases (alternate names, stage names, romanizations)
CREATE TABLE IF NOT EXISTS aliases (
    id SERIAL PRIMARY KEY,
    entity_id INTEGER NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    alias VARCHAR(255) NOT NULL,
    language VARCHAR(16) DEFAULT 'en',
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (entity_id, alias)
);

-- 6. Import Progress (resumable ingestion pipeline tracking)
CREATE TABLE IF NOT EXISTS import_progress (
    id SERIAL PRIMARY KEY,
    batch_name VARCHAR(128) NOT NULL UNIQUE,
    category VARCHAR(255) NOT NULL,
    last_offset INTEGER DEFAULT 0,
    total_fetched INTEGER DEFAULT 0,
    total_imported INTEGER DEFAULT 0,
    total_skipped INTEGER DEFAULT 0,
    status VARCHAR(64) DEFAULT 'pending',
    error_message TEXT,
    started_at TIMESTAMPTZ,
    updated_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMPTZ
);

-- 7. Google OAuth Tokens (Secure persistent token storage for production)
CREATE TABLE IF NOT EXISTS google_tokens (
    id VARCHAR(32) PRIMARY KEY DEFAULT 'default',
    access_token TEXT,
    refresh_token TEXT,
    expires_at BIGINT,
    updated_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP
);

-- Indexes for high performance querying
CREATE INDEX IF NOT EXISTS idx_entities_name ON entities(LOWER(name));
CREATE INDEX IF NOT EXISTS idx_entities_wikidata_id ON entities(wikidata_id);
CREATE INDEX IF NOT EXISTS idx_entities_category ON entities(category);
CREATE INDEX IF NOT EXISTS idx_entities_is_fictional ON entities(is_fictional);
CREATE INDEX IF NOT EXISTS idx_entities_popularity ON entities(popularity_rank);
CREATE INDEX IF NOT EXISTS idx_entity_attributes_attr_val ON entity_attributes(attribute_id, attribute_value);
CREATE INDEX IF NOT EXISTS idx_entity_attributes_entity ON entity_attributes(entity_id);
CREATE INDEX IF NOT EXISTS idx_aliases_entity_id ON aliases(entity_id);
CREATE INDEX IF NOT EXISTS idx_aliases_alias ON aliases(LOWER(alias));
CREATE INDEX IF NOT EXISTS idx_attributes_key ON attributes(attribute_key);

-- Trigger to maintain updated_at on entities
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_entities_updated_at ON entities;
CREATE TRIGGER trg_entities_updated_at
BEFORE UPDATE ON entities
FOR EACH ROW
EXECUTE FUNCTION update_updated_at_column();
