CREATE SCHEMA IF NOT EXISTS inventory;
CREATE TABLE inventory.schema_version (
    singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    version integer NOT NULL CHECK (version > 0)
);
INSERT INTO inventory.schema_version(singleton, version) VALUES (true, 1);
CREATE TABLE inventory.containers (
    id text PRIMARY KEY,
    owner_kind integer NOT NULL,
    owner_id text NOT NULL,
    kind integer NOT NULL,
    capacity integer NOT NULL CHECK (capacity BETWEEN 1 AND 10000),
    revision bigint NOT NULL DEFAULT 0 CHECK (revision >= 0)
);
CREATE TABLE inventory.entries (
    container_id text NOT NULL REFERENCES inventory.containers(id),
    definition_id text NOT NULL,
    quantity bigint NOT NULL CHECK (quantity > 0),
    PRIMARY KEY (container_id, definition_id)
);
CREATE TABLE inventory.operation_receipts (
    operation_id text PRIMARY KEY,
    fingerprint text NOT NULL,
    receipt jsonb NOT NULL
);
