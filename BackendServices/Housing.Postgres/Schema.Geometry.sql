-- Additive BLD-01 identity migration; apply after Schema.Gameplay.sql (v4).
DO $$ BEGIN
  IF (SELECT version FROM housing.schema_version WHERE singleton=true) IS DISTINCT FROM 4 THEN
    RAISE EXCEPTION 'housing geometry migration requires schema v4';
  END IF;
END $$;
ALTER TABLE housing.placements
  ADD COLUMN shape_key text NOT NULL DEFAULT 'legacy.generic'
    CHECK (length(shape_key) BETWEEN 1 AND 128 AND shape_key ~ '^[a-z0-9._-]+$'),
  ADD COLUMN orientation_index integer NOT NULL DEFAULT 0
    CHECK (orientation_index BETWEEN 0 AND 23);
UPDATE housing.schema_version SET version=5 WHERE singleton=true AND version=4;
