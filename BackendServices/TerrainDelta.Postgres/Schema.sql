CREATE SCHEMA IF NOT EXISTS terrain_delta;
CREATE TABLE terrain_delta.schema_version (singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton), version integer NOT NULL CHECK (version > 0));
INSERT INTO terrain_delta.schema_version(singleton,version) VALUES (true,1);
CREATE TABLE terrain_delta.regions (
 zone_id text NOT NULL, cell_x integer NOT NULL, cell_y integer NOT NULL,
 revision bigint NOT NULL DEFAULT 0 CHECK (revision >= 0),
 PRIMARY KEY(zone_id,cell_x,cell_y)
);
CREATE TABLE terrain_delta.operations (
 operation_id text PRIMARY KEY, fingerprint text NOT NULL,
 zone_id text NOT NULL, cell_x integer NOT NULL, cell_y integer NOT NULL,
 revision bigint NOT NULL CHECK (revision > 0), character_id text NOT NULL, claim_id text NOT NULL,
 anchor_x integer, anchor_y integer, anchor_z integer,
 kind text, magnitude integer,
 compensated_operation_id text UNIQUE REFERENCES terrain_delta.operations(operation_id),
 tombstone boolean NOT NULL,
 FOREIGN KEY(zone_id,cell_x,cell_y) REFERENCES terrain_delta.regions(zone_id,cell_x,cell_y),
 UNIQUE(zone_id,cell_x,cell_y,revision),
 CHECK ((NOT tombstone AND compensated_operation_id IS NULL AND anchor_x IS NOT NULL AND anchor_y IS NOT NULL AND anchor_z IS NOT NULL AND kind IN ('Dig','Raise','Flatten','Paint') AND magnitude BETWEEN 1 AND 32767) OR
        (tombstone AND compensated_operation_id IS NOT NULL AND anchor_x IS NULL AND anchor_y IS NULL AND anchor_z IS NULL AND kind IS NULL AND magnitude IS NULL))
);
CREATE INDEX operations_region_revision ON terrain_delta.operations(zone_id,cell_x,cell_y,revision);
