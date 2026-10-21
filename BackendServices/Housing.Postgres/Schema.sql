CREATE SCHEMA IF NOT EXISTS housing;
CREATE TABLE housing.schema_version (singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton), version integer NOT NULL CHECK (version > 0));
INSERT INTO housing.schema_version(singleton,version) VALUES (true,1);
CREATE TABLE housing.claims (
 id text PRIMARY KEY, owner_id text NOT NULL, state text NOT NULL,
 revision bigint NOT NULL CHECK (revision >= 0), permission_revision bigint NOT NULL CHECK (permission_revision >= 0),
 funded_until timestamptz NOT NULL, grace_ends timestamptz, notice_ends timestamptz,
 grants jsonb NOT NULL DEFAULT '{}'::jsonb
);
CREATE TABLE housing.claim_operations (operation_id text PRIMARY KEY, fingerprint text NOT NULL, claim_id text NOT NULL REFERENCES housing.claims(id), revision bigint NOT NULL, receipt jsonb NOT NULL);
CREATE TABLE housing.claim_structure_revisions (claim_id text PRIMARY KEY REFERENCES housing.claims(id), revision bigint NOT NULL DEFAULT 0 CHECK (revision >= 0));
CREATE TABLE housing.placements (
 operation_id text PRIMARY KEY, fingerprint text NOT NULL, claim_id text NOT NULL REFERENCES housing.claims(id),
 actor_id text NOT NULL, structure_id text NOT NULL UNIQUE, cell_x integer NOT NULL, cell_y integer NOT NULL, cell_z integer NOT NULL,
 content_id text NOT NULL, material_id text NOT NULL, material_quantity bigint NOT NULL CHECK (material_quantity > 0),
 container_id text NOT NULL, expected_container_revision bigint NOT NULL CHECK (expected_container_revision >= 0),
 state text NOT NULL CHECK (state IN ('Pending','Committed','Compensated')),
 revision bigint NOT NULL CHECK (revision >= 0), consume_receipt jsonb,
 UNIQUE (claim_id,cell_x,cell_y,cell_z)
);
