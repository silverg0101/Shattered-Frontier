-- Apply after Schema.DefeatRewards.sql. Condition is per equipped definition in the current
-- definition-quantity equipment model; a later instance ledger must migrate these rows.
BEGIN;
CREATE TABLE inventory.gear_condition (
  character_id text NOT NULL,
  definition_id text NOT NULL,
  durability integer NOT NULL DEFAULT 100 CHECK (durability BETWEEN 0 AND 100),
  max_durability integer NOT NULL DEFAULT 100 CHECK (max_durability = 100),
  PRIMARY KEY (character_id, definition_id)
);
CREATE TABLE inventory.death_wear_events (
  death_event_id text PRIMARY KEY,
  character_id text NOT NULL,
  level_at_death integer NOT NULL CHECK (level_at_death BETWEEN 1 AND 1000),
  min_level integer NOT NULL CHECK (min_level BETWEEN 1 AND 1000),
  wear_percent integer NOT NULL CHECK (wear_percent BETWEEN 0 AND 100),
  applied_at timestamptz NOT NULL DEFAULT now()
);
COMMIT;
