-- Installed by the owner role. Runtime receives DML only.
CREATE SCHEMA IF NOT EXISTS guild;
CREATE TABLE guild.schema_version (
  singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
  version integer NOT NULL, migration_id text NOT NULL,
  compatible_build_min integer NOT NULL, compatible_build_max integer NOT NULL,
  applied_at timestamptz NOT NULL DEFAULT now()
);
INSERT INTO guild.schema_version(singleton,version,migration_id,compatible_build_min,compatible_build_max)
VALUES (true,1,'guild-001',1,1);
CREATE TABLE guild.guilds (
  guild_id text PRIMARY KEY CHECK (guild_id ~ '^gld_[a-z0-9]+$'),
  realm_id text NOT NULL,
  name text NOT NULL,
  name_key text NOT NULL,
  tag text NOT NULL,
  state text NOT NULL CHECK (state IN ('active','disbanded')),
  created_at timestamptz NOT NULL DEFAULT now(),
  disbanded_at timestamptz,
  assets_release_at timestamptz,
  version bigint NOT NULL DEFAULT 1
);
CREATE UNIQUE INDEX guilds_name ON guild.guilds(realm_id,name_key) WHERE state='active';
CREATE UNIQUE INDEX guilds_tag ON guild.guilds(realm_id,tag) WHERE state='active';
CREATE TABLE guild.ranks (
  guild_id text NOT NULL REFERENCES guild.guilds,
  rank smallint NOT NULL CHECK (rank BETWEEN 0 AND 9),
  title text NOT NULL,
  permissions text[] NOT NULL,
  gold_withdraw_daily_copper bigint NOT NULL DEFAULT 0,
  PRIMARY KEY (guild_id,rank)
);
CREATE TABLE guild.members (
  character_id text PRIMARY KEY,
  guild_id text NOT NULL REFERENCES guild.guilds,
  rank smallint NOT NULL CHECK (rank BETWEEN 0 AND 9),
  joined_at timestamptz NOT NULL DEFAULT now(),
  rank_since timestamptz NOT NULL DEFAULT now(),
  FOREIGN KEY (guild_id,rank) REFERENCES guild.ranks(guild_id,rank)
);
CREATE UNIQUE INDEX one_leader ON guild.members(guild_id) WHERE rank=0;
CREATE INDEX guild_members_roster ON guild.members(guild_id,rank,joined_at,character_id);
CREATE TABLE guild.invitations (
  guild_id text NOT NULL REFERENCES guild.guilds,
  recipient_character_id text NOT NULL,
  invited_by text NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (guild_id,recipient_character_id)
);
CREATE TABLE guild.idempotency_keys (
  idempotency_key text PRIMARY KEY,
  operation text NOT NULL,
  request_hash text NOT NULL,
  response bytea NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now()
);
