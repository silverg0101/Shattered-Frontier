-- Owner-role bootstrap. Runtime group_svc receives DML only.
CREATE SCHEMA IF NOT EXISTS "group";
CREATE TABLE "group".schema_version (
  singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
  version integer NOT NULL, migration_id text NOT NULL,
  compatible_build_min integer NOT NULL, compatible_build_max integer NOT NULL,
  applied_at timestamptz NOT NULL DEFAULT now()
);
INSERT INTO "group".schema_version(singleton,version,migration_id,compatible_build_min,compatible_build_max)
VALUES (true,1,'group-001',1,1);
CREATE TABLE "group".parties (
  party_id text PRIMARY KEY CHECK (party_id ~ '^pty_[a-z0-9]+$'),
  size_class text NOT NULL DEFAULT 'solo_party' CHECK (size_class='solo_party'),
  state text NOT NULL DEFAULT 'active' CHECK (state IN ('active','disbanded')),
  formed_by text NOT NULL DEFAULT 'premade' CHECK (formed_by='premade'),
  dc_region text NOT NULL,
  leader_character_id text NOT NULL,
  loot_mode text NOT NULL DEFAULT 'personal' CHECK (loot_mode='personal'),
  created_at timestamptz NOT NULL DEFAULT now(), disbanded_at timestamptz,
  version integer NOT NULL DEFAULT 0
);
CREATE TABLE "group".party_members (
  party_id text NOT NULL REFERENCES "group".parties(party_id),
  character_id text NOT NULL, account_id text NOT NULL, realm_id text NOT NULL,
  subgroup smallint NOT NULL DEFAULT 1 CHECK (subgroup=1),
  role_assigned text CHECK (role_assigned IN ('tank','healer','damage','support')),
  is_assistant boolean NOT NULL DEFAULT false,
  joined_at timestamptz NOT NULL DEFAULT now(), left_at timestamptz,
  left_reason text CHECK (left_reason IN ('left','kicked','disbanded')),
  PRIMARY KEY(party_id,character_id,joined_at)
);
CREATE UNIQUE INDEX party_members_one_party ON "group".party_members(character_id) WHERE left_at IS NULL;
CREATE TABLE "group".idempotency_keys (
  idempotency_key text PRIMARY KEY, rpc text NOT NULL, request_hash text NOT NULL, response bytea NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now()
);
