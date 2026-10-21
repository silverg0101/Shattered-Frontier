-- Apply with the migration identity to existing version 1 stores before starting Gameplay.Host.
-- The original five columns and operation IDs remain unchanged.
DO $$ BEGIN
  IF EXISTS (SELECT 1 FROM character_state.reward_receipts) THEN
    RAISE EXCEPTION 'reward destination backfill required before migration';
  END IF;
END $$;
ALTER TABLE character_state.reward_receipts
  ADD COLUMN IF NOT EXISTS container_id text NOT NULL DEFAULT '',
  ADD COLUMN IF NOT EXISTS state text NOT NULL DEFAULT 'Pending',
  ADD COLUMN IF NOT EXISTS created_at timestamptz NOT NULL DEFAULT now();
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conname = 'reward_receipts_state_check'
      AND connamespace = 'character_state'::regnamespace) THEN
    ALTER TABLE character_state.reward_receipts ADD CONSTRAINT reward_receipts_state_check
      CHECK (state IN ('Pending','Complete'));
  END IF;
END $$;
