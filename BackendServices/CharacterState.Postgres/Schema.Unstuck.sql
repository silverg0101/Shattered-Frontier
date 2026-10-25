-- Owner-role migration after Schema.DeathPenalty.sql. Unstuck shares the durable position record.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM character_state.schema_version WHERE singleton=true AND version=9 AND migration_id='character-state-009') THEN
    RAISE EXCEPTION 'expected character-state schema version 9';
  END IF;
END $$;
ALTER TABLE character_state.positions ADD COLUMN unstuck_at_unix_ms bigint NOT NULL DEFAULT 0 CHECK (unstuck_at_unix_ms >= 0);
ALTER TABLE character_state.positions ADD COLUMN unstuck_operation_id text NOT NULL DEFAULT '';
ALTER TABLE character_state.positions ADD COLUMN unstuck_fingerprint text NOT NULL DEFAULT '';
UPDATE character_state.schema_version SET version=10,migration_id='character-state-010',
  compatible_build_min=10,compatible_build_max=10,applied_at=now() WHERE singleton=true;
COMMIT;
