-- Owner-role migration. A raid links active same-realm parties; each party has one raid.
CREATE TABLE "group".raid_groups (
  raid_group_id text PRIMARY KEY CHECK (raid_group_id ~ '^rgp_[a-z0-9]+$'),
  realm_id text NOT NULL,
  leader_party_id text NOT NULL REFERENCES "group".parties(party_id),
  created_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE "group".raid_parties (
  raid_group_id text NOT NULL REFERENCES "group".raid_groups(raid_group_id),
  party_id text PRIMARY KEY REFERENCES "group".parties(party_id),
  joined_at timestamptz NOT NULL DEFAULT now()
);
