-- Apply after Schema.Appearance.sql. Existing progression snapshots already contain per-character skill values.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM character_state.schema_version WHERE singleton=true AND
      (version=5 AND migration_id='character-state-005' OR version=6 AND migration_id='character-state-006')) THEN
    RAISE EXCEPTION 'expected character-state schema version 5 or 6';
  END IF;
END $$;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname='progression_skills_object'
      AND conrelid='character_state.progression'::regclass) THEN
    ALTER TABLE character_state.progression ADD CONSTRAINT progression_skills_object
      CHECK (state ? 'Skills' AND jsonb_typeof(state->'Skills') = 'object');
  END IF;
END $$;
UPDATE character_state.schema_version SET version=6,migration_id='character-state-006',
  compatible_build_min=6,compatible_build_max=6,applied_at=now()
  WHERE singleton=true AND version=5 AND migration_id='character-state-005';
COMMIT;
