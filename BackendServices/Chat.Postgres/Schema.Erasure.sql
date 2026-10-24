CREATE TABLE chat.erased_characters (
  character_id text PRIMARY KEY,
  erased_at timestamptz NOT NULL DEFAULT now()
);
UPDATE chat.schema_version SET version=2, migration_id='chat-002', compatible_build_min=2, compatible_build_max=2 WHERE singleton=true;
