-- Apply after Schema.Position.sql. Learning intents are durable before external payment.
BEGIN;
DO $$ BEGIN
  IF (SELECT version FROM character_state.schema_version WHERE singleton=true) IS DISTINCT FROM 3
     OR (SELECT migration_id FROM character_state.schema_version WHERE singleton=true) IS DISTINCT FROM 'character-state-003' THEN
    RAISE EXCEPTION 'expected character-state schema version 3';
  END IF;
END $$;
ALTER TABLE character_state.source_receipts DROP CONSTRAINT source_receipts_domain_check;
ALTER TABLE character_state.source_receipts ADD CONSTRAINT source_receipts_domain_check CHECK (domain IN ('progression','quest','recipe'));
CREATE TABLE character_state.recipe_intents (
  operation_id text PRIMARY KEY,
  character_id text NOT NULL,
  recipe_id text NOT NULL,
  source text NOT NULL CHECK (source IN ('trainer','scroll','discovery')),
  source_id text NOT NULL,
  container_id text NOT NULL DEFAULT '',
  expected_container_revision bigint NOT NULL DEFAULT 0,
  cost_copper bigint NOT NULL DEFAULT 0 CHECK (cost_copper >= 0),
  fingerprint text NOT NULL,
  state text NOT NULL DEFAULT 'Pending' CHECK (state IN ('Pending','Complete')),
  created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX recipe_intents_pending ON character_state.recipe_intents(created_at) WHERE state='Pending';
UPDATE character_state.schema_version SET version=4,migration_id='character-state-004',
  compatible_build_min=4,compatible_build_max=4,applied_at=now() WHERE singleton=true;
COMMIT;
