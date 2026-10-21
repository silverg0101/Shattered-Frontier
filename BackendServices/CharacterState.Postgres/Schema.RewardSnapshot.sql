-- Apply to an empty-reward version 1 store. Existing reward intents require an
-- audited item snapshot backfill and cannot be inferred from current content.
BEGIN;
DO $$ BEGIN
  IF (SELECT version FROM character_state.schema_version WHERE singleton=true) <> 1 THEN
    RAISE EXCEPTION 'expected character-state schema version 1';
  END IF;
  IF EXISTS (SELECT 1 FROM character_state.reward_receipts) THEN
    RAISE EXCEPTION 'reward snapshot backfill required';
  END IF;
END $$;
ALTER TABLE character_state.reward_receipts
  ADD COLUMN content_revision integer NOT NULL DEFAULT 1 CHECK (content_revision > 0),
  ADD COLUMN item_lines jsonb NOT NULL DEFAULT '[]'::jsonb CHECK (jsonb_typeof(item_lines) = 'array');
ALTER TABLE character_state.reward_receipts ALTER COLUMN content_revision DROP DEFAULT,
  ALTER COLUMN item_lines DROP DEFAULT;
UPDATE character_state.schema_version SET version=2,migration_id='character-state-002',
  compatible_build_min=2,compatible_build_max=2 WHERE singleton=true;
COMMIT;
