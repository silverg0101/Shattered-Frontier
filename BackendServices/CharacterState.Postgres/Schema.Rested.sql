-- Owner-role migration after Schema.Death.sql. Rested state lives in the progression JSON snapshot.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM character_state.schema_version WHERE singleton=true AND version=7 AND migration_id='character-state-007') THEN
    RAISE EXCEPTION 'expected character-state schema version 7';
  END IF;
END $$;
ALTER TABLE character_state.source_receipts DROP CONSTRAINT source_receipts_domain_check;
ALTER TABLE character_state.source_receipts ADD CONSTRAINT source_receipts_domain_check
  CHECK (domain IN ('progression','quest','progression_session_end'));
UPDATE character_state.schema_version SET version=8,migration_id='character-state-008',
  compatible_build_min=8,compatible_build_max=8,applied_at=now() WHERE singleton=true;
COMMIT;
