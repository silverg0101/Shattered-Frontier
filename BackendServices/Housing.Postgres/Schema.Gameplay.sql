-- Additive INT-04 gameplay migration. Apply after Schema.sql to a fresh v3 store.
-- Existing claims lack saved geometry and require a separate backfill before this migration.
DO $$ BEGIN
  IF EXISTS (SELECT 1 FROM housing.claims) THEN
    RAISE EXCEPTION 'claim geometry backfill required';
  END IF;
END $$;
CREATE TABLE housing.claim_geometry (
  claim_id text PRIMARY KEY REFERENCES housing.claims(id),
  center_x double precision NOT NULL,
  center_y double precision NOT NULL,
  side_metres integer NOT NULL CHECK (side_metres BETWEEN 10 AND 40)
);
ALTER TABLE housing.placements DROP CONSTRAINT placements_claim_id_cell_x_cell_y_cell_z_key;
ALTER TABLE housing.placements DROP CONSTRAINT placements_state_check;
ALTER TABLE housing.placements ADD CONSTRAINT placements_state_check CHECK (state IN ('Pending','Committed','Compensated','Removed'));
CREATE UNIQUE INDEX placements_live_cell ON housing.placements (claim_id,cell_x,cell_y,cell_z)
  WHERE state IN ('Pending','Committed');
CREATE TABLE housing.removals (
  operation_id text PRIMARY KEY,
  fingerprint text NOT NULL,
  placement_operation_id text NOT NULL REFERENCES housing.placements(operation_id),
  state text NOT NULL CHECK (state IN ('Pending','Committed','Compensated')),
  revision bigint NOT NULL CHECK (revision >= 0),
  refund_quantity bigint NOT NULL CHECK (refund_quantity >= 0),
  expected_container_revision bigint NOT NULL CHECK (expected_container_revision >= 0),
  refund_receipt jsonb
);
CREATE UNIQUE INDEX removals_live_placement ON housing.removals (placement_operation_id)
  WHERE state IN ('Pending','Committed');
UPDATE housing.schema_version SET version=4 WHERE singleton=true AND version=3;
