-- Apply after Schema.ItemInstances.sql. The repair transaction requires both
-- inventory and exchange schemas in this PostgreSQL database.
BEGIN;
ALTER TABLE inventory.item_instances ADD COLUMN IF NOT EXISTS max_durability_percent integer NOT NULL DEFAULT 100
  CHECK (max_durability_percent BETWEEN 60 AND 100);
CREATE TABLE IF NOT EXISTS inventory.repair_quotes (
  quote_id text PRIMARY KEY, owner_id text NOT NULL, cost_copper bigint NOT NULL CHECK (cost_copper > 0),
  expires_at timestamptz NOT NULL,
  tuning_fingerprint text NOT NULL,
  broken_max_loss_percent integer NOT NULL CHECK (broken_max_loss_percent BETWEEN 0 AND 10),
  durability_floor_percent integer NOT NULL CHECK (durability_floor_percent BETWEEN 50 AND 100)
);
CREATE TABLE IF NOT EXISTS inventory.repair_quote_items (
  quote_id text NOT NULL REFERENCES inventory.repair_quotes(quote_id) ON DELETE CASCADE, instance_id text NOT NULL,
  prior_durability integer NOT NULL, prior_max integer NOT NULL, cost_copper bigint NOT NULL CHECK (cost_copper > 0),
  PRIMARY KEY (quote_id,instance_id)
);
CREATE TABLE IF NOT EXISTS inventory.repair_receipts (
  operation_id text PRIMARY KEY, quote_id text NOT NULL, owner_id text NOT NULL,
  cost_copper bigint NOT NULL, item_count integer NOT NULL, committed_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
COMMIT;
