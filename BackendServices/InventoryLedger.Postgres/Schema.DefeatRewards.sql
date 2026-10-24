-- Durable boar reward intents. Install after Schema.sql before enabling Gameplay.Host rewards.
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
    state text NOT NULL CHECK (state IN ('Pending', 'Complete', 'Ineligible')),
    PRIMARY KEY (defeat_event_id, recipient_id)
);
CREATE INDEX IF NOT EXISTS defeat_reward_pending ON inventory.defeat_reward_intents (state) WHERE state = 'Pending';
