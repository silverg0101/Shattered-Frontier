-- Immutable level captured with the death event, for cross-database wear recovery.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM character_state.schema_version WHERE singleton=true AND version=8 AND migration_id='character-state-008') THEN
    RAISE EXCEPTION 'expected character-state schema version 8';
  END IF;
END $$;
ALTER TABLE character_state.death_events ADD COLUMN level_at_death integer NOT NULL DEFAULT 1
  CHECK (level_at_death BETWEEN 1 AND 1000);
ALTER TABLE character_state.death_events ADD COLUMN wear_min_level integer NOT NULL DEFAULT 10
  CHECK (wear_min_level BETWEEN 1 AND 1000);
ALTER TABLE character_state.death_events ADD COLUMN wear_percent integer NOT NULL DEFAULT 5
  CHECK (wear_percent BETWEEN 0 AND 100);
UPDATE character_state.schema_version SET version=9,migration_id='character-state-009',
  compatible_build_min=9,compatible_build_max=9,applied_at=now() WHERE singleton=true;
COMMIT;
