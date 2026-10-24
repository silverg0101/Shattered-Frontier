-- Apply after Schema.GatheringSkills.sql. Death receipts survive clearing so a replay cannot kill a respawned character.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM character_state.schema_version WHERE singleton=true AND version=6 AND migration_id='character-state-006') THEN
    RAISE EXCEPTION 'expected character-state schema version 6';
  END IF;
END $$;
CREATE TABLE character_state.deaths (
  character_id text PRIMARY KEY,
  death_event_id text NOT NULL,
  is_dead boolean NOT NULL,
  occurred_at_unix_ms bigint NOT NULL,
  respawn_at_unix_ms bigint NOT NULL,
  death_zone_id text NOT NULL,
  death_x double precision NOT NULL, death_y double precision NOT NULL, death_z double precision NOT NULL,
  respawn_zone_id text NOT NULL,
  respawn_x double precision NOT NULL, respawn_y double precision NOT NULL, respawn_z double precision NOT NULL,
  updated_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE character_state.death_events (
  death_event_id text PRIMARY KEY,
  character_id text NOT NULL,
  fingerprint text NOT NULL
);
UPDATE character_state.schema_version SET version=7,migration_id='character-state-007',
  compatible_build_min=7,compatible_build_max=7,applied_at=now() WHERE singleton=true;
COMMIT;
