CREATE TABLE guild.outbox (
  seq bigserial PRIMARY KEY,
  event_id text NOT NULL UNIQUE CHECK (event_id LIKE 'evt\_%'),
  topic text NOT NULL,
  partition_key text NOT NULL,
  envelope jsonb NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now(),
  published_at timestamptz
);
CREATE INDEX guild_outbox_unpublished ON guild.outbox(seq) WHERE published_at IS NULL;
UPDATE guild.schema_version SET version=2,migration_id='guild-002' WHERE singleton=true;
