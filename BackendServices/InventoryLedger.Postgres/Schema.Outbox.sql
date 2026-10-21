-- INT-06. Apply once, after Schema.sql, using the migration identity.
-- migration_checksum is SHA-256 of this file's UTF-8 prefix before the UPDATE below.
BEGIN;
DO $$ BEGIN
  IF (SELECT version FROM inventory.schema_version WHERE singleton=true) IS DISTINCT FROM 1 THEN
    RAISE EXCEPTION 'expected inventory schema version 1';
  END IF;
END $$;
CREATE TABLE inventory.receipt_outbox (
  character_order bigint NOT NULL CHECK (character_order > 0),
  source_operation_id text NOT NULL REFERENCES inventory.operation_receipts(operation_id),
  destination text NOT NULL CHECK (destination = 'progression'),
  character_id text NOT NULL,
  payload_version integer NOT NULL CHECK (payload_version = 1),
  payload bytea NOT NULL,
  payload_digest text NOT NULL CHECK (payload_digest ~ '^[0-9a-f]{64}$'),
  created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
  next_attempt_at timestamptz NOT NULL DEFAULT now(),
  attempts integer NOT NULL DEFAULT 0 CHECK (attempts >= 0),
  lease_owner text,
  lease_until timestamptz,
  delivered_at timestamptz,
  parked_at timestamptz,
  last_error_code text,
  PRIMARY KEY (source_operation_id, destination),
  UNIQUE (character_id, character_order)
);
CREATE INDEX receipt_outbox_due ON inventory.receipt_outbox(next_attempt_at, created_at, source_operation_id)
  WHERE delivered_at IS NULL AND parked_at IS NULL;
CREATE INDEX receipt_outbox_character_order ON inventory.receipt_outbox(character_id, character_order)
  WHERE delivered_at IS NULL;
ALTER TABLE inventory.schema_version ADD COLUMN migration_checksum text;
UPDATE inventory.schema_version SET version=2,
  migration_checksum='66ae3e7c362f9062575bc97da4fa5e50d48548e10928b2f74b13f1f12c7bd8fb'
  WHERE singleton=true;
COMMIT;
