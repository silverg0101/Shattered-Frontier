-- Apply after Schema.RewardSnapshot.sql (or an existing version 2 Schema.sql).
BEGIN;
DO $$ BEGIN
  IF (SELECT version FROM character_state.schema_version WHERE singleton=true) IS DISTINCT FROM 2
     OR (SELECT migration_id FROM character_state.schema_version WHERE singleton=true) IS DISTINCT FROM 'character-state-002' THEN
    RAISE EXCEPTION 'expected character-state schema version 2';
  END IF;
END $$;
CREATE TABLE character_state.positions (
  character_id text PRIMARY KEY,
  revision bigint NOT NULL CHECK (revision >= 1),
  zone_id text NOT NULL CHECK (zone_id ~ '^[a-z0-9_]{1,64}$'),
  x double precision NOT NULL CHECK (x BETWEEN -10000000 AND 10000000),
  y double precision NOT NULL CHECK (y BETWEEN -10000000 AND 10000000),
  z double precision NOT NULL CHECK (z BETWEEN -10000000 AND 10000000),
  yaw real NOT NULL CHECK (yaw BETWEEN -360 AND 360),
  updated_at timestamptz NOT NULL DEFAULT now()
);
UPDATE character_state.schema_version SET version=3,migration_id='character-state-003',
  compatible_build_min=3,compatible_build_max=3,applied_at=now() WHERE singleton=true;
COMMIT;
