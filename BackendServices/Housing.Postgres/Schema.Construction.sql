-- Additive DEC-02 construction-decision migration; apply after Schema.Geometry.sql (v5).
-- Placement materials are now reserved into a construction hold, then debited (commit) or released (abort).
-- A v5 pending row used the old direct-debit key, so every pending placement must be reconciled first.
DO $$ BEGIN
  IF (SELECT version FROM housing.schema_version WHERE singleton=true) IS DISTINCT FROM 5 THEN
    RAISE EXCEPTION 'housing construction migration requires schema v5';
  END IF;
  IF EXISTS (SELECT 1 FROM housing.placements WHERE state='Pending') THEN
    RAISE EXCEPTION 'reconcile pending placements before the v6 migration';
  END IF;
END $$;
ALTER TABLE housing.placements ADD COLUMN decision text CHECK (decision IN ('Commit','Abort'));
UPDATE housing.placements SET decision = CASE WHEN state='Compensated' THEN 'Abort' ELSE 'Commit' END;
ALTER TABLE housing.placements ADD CONSTRAINT placements_decided_terminal CHECK (state='Pending' OR decision IS NOT NULL);
UPDATE housing.schema_version SET version=6 WHERE singleton=true AND version=5;
