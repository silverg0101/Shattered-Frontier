-- Durable per-recipient creature reward intents. Install after Schema.sql before enabling Gameplay.Host rewards.
CREATE TABLE IF NOT EXISTS inventory.defeat_reward_intents (
    defeat_event_id text NOT NULL,
    recipient_id text NOT NULL,
    killer_id text NOT NULL,
    container_id text NOT NULL,
    operation_id text NOT NULL UNIQUE,
    killer_protected boolean NOT NULL,
    boar_protected boolean NOT NULL,
    table_id text NOT NULL,
    table_version integer NOT NULL,
    quantity bigint NOT NULL CHECK (quantity >= 0),
    item_lines jsonb NOT NULL DEFAULT '[]'::jsonb,
    state text NOT NULL CHECK (state IN ('Pending', 'Complete', 'Ineligible')),
    PRIMARY KEY (defeat_event_id, recipient_id)
);
ALTER TABLE inventory.defeat_reward_intents ADD COLUMN IF NOT EXISTS item_lines jsonb NOT NULL DEFAULT '[]'::jsonb;
CREATE INDEX IF NOT EXISTS defeat_reward_pending ON inventory.defeat_reward_intents (state) WHERE state = 'Pending';

CREATE TABLE IF NOT EXISTS inventory.defeat_loot_events (
    defeat_event_id text PRIMARY KEY,
    scope text NOT NULL CHECK (scope IN ('Personal', 'Shared'))
);
INSERT INTO inventory.defeat_loot_events (defeat_event_id, scope)
SELECT DISTINCT defeat_event_id, 'Personal' FROM inventory.defeat_reward_intents
ON CONFLICT DO NOTHING;

-- One immutable group roll and claim per unique/boss defeat. The claim intent is committed
-- before the ledger transaction; recovery completes the same ledger operation after a crash.
CREATE TABLE IF NOT EXISTS inventory.shared_defeat_drops (
    defeat_event_id text PRIMARY KEY,
    creature_id text NOT NULL,
    table_id text NOT NULL,
    table_version integer NOT NULL,
    killer_id text NOT NULL,
    killer_protected boolean NOT NULL,
    creature_protected boolean NOT NULL,
    eligible_ids text[] NOT NULL,
    item_lines jsonb NOT NULL,
    claimant_id text,
    container_id text,
    operation_id text UNIQUE,
    state text NOT NULL CHECK (state IN ('Open', 'Ineligible', 'ClaimPending', 'Complete')),
    CHECK ((state IN ('Open', 'Ineligible')) = (claimant_id IS NULL AND container_id IS NULL AND operation_id IS NULL))
);
CREATE INDEX IF NOT EXISTS shared_defeat_claim_pending ON inventory.shared_defeat_drops (state) WHERE state = 'ClaimPending';
