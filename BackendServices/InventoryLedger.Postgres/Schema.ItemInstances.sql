-- Apply after Schema.DeathWear.sql. A single transaction converts each legacy gear
-- quantity into stable copies at its character/definition condition.
BEGIN;
DO $$ BEGIN
  IF (SELECT version FROM inventory.schema_version WHERE singleton=true) NOT IN (2,3) THEN
    RAISE EXCEPTION 'item instance migration requires inventory schema version 2 or 3';
  END IF;
END $$;
CREATE TABLE IF NOT EXISTS inventory.gear_definitions (
  definition_id text PRIMARY KEY
);
INSERT INTO inventory.gear_definitions(definition_id) VALUES
  ('item/hide_cap'),('item/hide_jerkin'),('item/hide_gloves'),('item/hide_leggings'),
  ('item/hide_boots'),('item/hide_cloak'),('item/iron_ring'),('item/iron_buckler'),
  ('item/iron_shortsword'),('item/wolfhide_bracers'),('item/wolfhide_cloak'),
  ('item/fangtooth_dagger'),('item/alpha_pelt_mantle'),('item/cudgel'),
  ('item/padded_jack'),('item/brigand_blade'),('item/leather_coif'),
  ('item/archers_gloves'),('item/studded_jerkin'),('item/reinforced_greaves'),
  ('item/captains_longsword')
ON CONFLICT DO NOTHING;
CREATE TABLE IF NOT EXISTS inventory.item_instances (
  instance_id text PRIMARY KEY,
  container_id text NOT NULL REFERENCES inventory.containers(id),
  definition_id text NOT NULL,
  durability_percent integer NOT NULL DEFAULT 100 CHECK (durability_percent BETWEEN 0 AND 100)
);
CREATE INDEX IF NOT EXISTS item_instances_container ON inventory.item_instances(container_id,definition_id,instance_id);
INSERT INTO inventory.item_instances(instance_id,container_id,definition_id,durability_percent)
SELECT 'itm_' || md5('legacy|' || e.container_id || '|' || e.definition_id || '|' || n::text),
       e.container_id,e.definition_id,coalesce(g.durability,100)
FROM inventory.entries e
JOIN inventory.containers c ON c.id=e.container_id
JOIN inventory.gear_definitions d ON d.definition_id=split_part(e.definition_id,'/',1) || '/' || split_part(e.definition_id,'/',2)
CROSS JOIN LATERAL generate_series(1,e.quantity) AS n
LEFT JOIN inventory.gear_condition g ON g.character_id=c.owner_id AND g.definition_id=e.definition_id
ON CONFLICT (instance_id) DO NOTHING;
UPDATE inventory.schema_version SET version=3 WHERE singleton=true;
COMMIT;
