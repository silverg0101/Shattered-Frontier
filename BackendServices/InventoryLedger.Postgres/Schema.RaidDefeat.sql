-- The raid identity belongs to the immutable shared-kill eligibility snapshot.
ALTER TABLE inventory.shared_defeat_drops
  ADD COLUMN raid_group_id text NOT NULL DEFAULT '';
ALTER TABLE inventory.shared_defeat_drops
  ADD CONSTRAINT shared_defeat_raid_id_format
  CHECK (raid_group_id = '' OR raid_group_id ~ '^rgp_[a-z0-9]+$');
