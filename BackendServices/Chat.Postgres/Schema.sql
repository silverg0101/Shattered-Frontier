-- Owner-role bootstrap. The runtime chat role receives DML only.
CREATE SCHEMA IF NOT EXISTS chat;
CREATE TABLE chat.schema_version (
  singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
  version integer NOT NULL, migration_id text NOT NULL,
  compatible_build_min integer NOT NULL, compatible_build_max integer NOT NULL,
  applied_at timestamptz NOT NULL DEFAULT now()
);
INSERT INTO chat.schema_version(singleton,version,migration_id,compatible_build_min,compatible_build_max)
VALUES (true,1,'chat-001',1,1);
CREATE TABLE chat.contacts (
  character_id text NOT NULL,
  other_character_id text NOT NULL,
  kind text NOT NULL CHECK (kind IN ('friend','block')),
  created_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (character_id,other_character_id,kind),
  CHECK (character_id <> other_character_id)
);
CREATE INDEX contacts_block_reverse ON chat.contacts(other_character_id,character_id) WHERE kind='block';
CREATE TABLE chat.idempotency_keys (
  idempotency_key text PRIMARY KEY, request_hash text NOT NULL,
  applied boolean NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now()
);
