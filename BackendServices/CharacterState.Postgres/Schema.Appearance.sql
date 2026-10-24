-- Apply after Schema.Recipes.sql. Safe to replay after a completed migration.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM character_state.schema_version WHERE singleton=true AND
      (version=4 AND migration_id='character-state-004' OR version=5 AND migration_id='character-state-005')) THEN
    RAISE EXCEPTION 'expected character-state schema version 4 or 5';
  END IF;
END $$;
CREATE TABLE IF NOT EXISTS character_state.appearance (
  character_id text PRIMARY KEY,
  revision bigint NOT NULL CHECK (revision >= 1),
  packet bytea NOT NULL CHECK (octet_length(packet) BETWEEN 1 AND 64),
  updated_at timestamptz NOT NULL DEFAULT now()
);
UPDATE character_state.schema_version SET version=5,migration_id='character-state-005',
  compatible_build_min=5,compatible_build_max=5,applied_at=now()
  WHERE singleton=true AND version=4 AND migration_id='character-state-004';
COMMIT;
