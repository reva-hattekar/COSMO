-- COSMO Knowledge Engine Schema
-- Designed for high performance SQLite with 100,000+ entities

-- 1. Categories
CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. Entities
CREATE TABLE IF NOT EXISTS entities (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    wikidata_id TEXT UNIQUE,
    name TEXT NOT NULL,
    description TEXT,
    entity_type TEXT NOT NULL DEFAULT 'human',
    category_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    category TEXT NOT NULL,
    subcategory TEXT,
    image_url TEXT,
    gender TEXT,
    nationality TEXT,
    birth_date TEXT,
    death_date TEXT,
    debut_year INTEGER,
    is_fictional INTEGER NOT NULL DEFAULT 0,
    popularity_rank INTEGER DEFAULT 1000000,
    source TEXT DEFAULT 'wikidata',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 3. Attributes
CREATE TABLE IF NOT EXISTS attributes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    attribute_key TEXT NOT NULL UNIQUE,
    attribute_name TEXT NOT NULL,
    data_type TEXT NOT NULL DEFAULT 'boolean',
    question_template TEXT NOT NULL,
    category_scope TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 4. Entity Attributes
CREATE TABLE IF NOT EXISTS entity_attributes (
    entity_id INTEGER NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    attribute_id INTEGER NOT NULL REFERENCES attributes(id) ON DELETE CASCADE,
    attribute_value TEXT NOT NULL,
    confidence REAL NOT NULL DEFAULT 1.0,
    source TEXT DEFAULT 'wikidata',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (entity_id, attribute_id, attribute_value)
);

-- 5. Aliases (alternate names, stage names, romanizations)
CREATE TABLE IF NOT EXISTS aliases (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    entity_id INTEGER NOT NULL REFERENCES entities(id) ON DELETE CASCADE,
    alias TEXT NOT NULL,
    language TEXT DEFAULT 'en',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (entity_id, alias)
);

-- 6. Import Progress (resumable ingestion pipeline tracking)
CREATE TABLE IF NOT EXISTS import_progress (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    batch_name TEXT NOT NULL UNIQUE,
    category TEXT NOT NULL,
    last_offset INTEGER DEFAULT 0,
    total_fetched INTEGER DEFAULT 0,
    total_imported INTEGER DEFAULT 0,
    total_skipped INTEGER DEFAULT 0,
    status TEXT DEFAULT 'pending',
    error_message TEXT,
    started_at DATETIME,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    completed_at DATETIME
);

-- Indexes for lightning-fast candidate filtering and question selection
CREATE INDEX IF NOT EXISTS idx_entities_name ON entities(name COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS idx_entities_wikidata_id ON entities(wikidata_id);
CREATE INDEX IF NOT EXISTS idx_entities_category ON entities(category);
CREATE INDEX IF NOT EXISTS idx_entities_is_fictional ON entities(is_fictional);
CREATE INDEX IF NOT EXISTS idx_entities_popularity ON entities(popularity_rank);
CREATE INDEX IF NOT EXISTS idx_entity_attributes_attr_val ON entity_attributes(attribute_id, attribute_value);
CREATE INDEX IF NOT EXISTS idx_entity_attributes_entity ON entity_attributes(entity_id);
CREATE INDEX IF NOT EXISTS idx_aliases_entity_id ON aliases(entity_id);
CREATE INDEX IF NOT EXISTS idx_aliases_alias ON aliases(alias COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS idx_attributes_key ON attributes(attribute_key);

-- Trigger to maintain updated_at on entities
CREATE TRIGGER IF NOT EXISTS trg_entities_updated_at
AFTER UPDATE ON entities
FOR EACH ROW
BEGIN
    UPDATE entities SET updated_at = CURRENT_TIMESTAMP WHERE id = OLD.id;
END;

