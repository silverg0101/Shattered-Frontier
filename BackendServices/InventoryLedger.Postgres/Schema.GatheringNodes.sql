-- GAT-01 durable node state and grant intents. Apply after Schema.Outbox.sql.
-- This additive migration has its own marker; inventory's INT-06 marker stays at version 2.
BEGIN;
DO $$ BEGIN
  IF NOT EXISTS (SELECT 1 FROM inventory.schema_version WHERE singleton=true AND version=2) THEN
    RAISE EXCEPTION 'expected inventory schema version 2';
  END IF;
END $$;
CREATE TABLE inventory.gathering_schema_version (
  singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
  version integer NOT NULL CHECK (version=1),
  migration_id text NOT NULL CHECK (migration_id='gathering-nodes-001')
);
INSERT INTO inventory.gathering_schema_version(singleton,version,migration_id)
  VALUES (true,1,'gathering-nodes-001');
CREATE TABLE inventory.gathering_node_state (
  realm_id text PRIMARY KEY,
  state jsonb NOT NULL
);
CREATE TABLE inventory.gathering_harvest_receipts (
  operation_id text PRIMARY KEY,
  character_id text NOT NULL,
  node_id text NOT NULL,
  generation integer NOT NULL,
  container_id text NOT NULL,
  payload_version integer NOT NULL CHECK (payload_version=1),
  payload bytea NOT NULL,
  payload_digest text NOT NULL CHECK (payload_digest ~ '^[0-9a-f]{64}$'),
  result bytea NOT NULL,
  resource_id text,
  quantity integer NOT NULL CHECK (quantity>=0),
  delivered_at timestamptz,
  created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX gathering_harvest_pending ON inventory.gathering_harvest_receipts(created_at,operation_id)
  WHERE resource_id IS NOT NULL AND delivered_at IS NULL;
COMMIT;
