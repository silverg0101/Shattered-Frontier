CREATE TABLE chat.mail_messages (
  mail_id text PRIMARY KEY CHECK (mail_id LIKE 'mal\_%'),
  realm_id text NOT NULL,
  sender_account_id text NOT NULL,
  sender_character_id text NOT NULL,
  recipient_account_id text NOT NULL,
  recipient_character_id text NOT NULL,
  subject_ct bytea NOT NULL,
  body_ct bytea NOT NULL,
  has_attachments boolean NOT NULL DEFAULT false CHECK (has_attachments = false),
  state text NOT NULL CHECK (state IN ('delivered','read','deleted')),
  sent_at timestamptz NOT NULL DEFAULT now(),
  expires_at timestamptz NOT NULL,
  read_at timestamptz,
  version bigint NOT NULL DEFAULT 1,
  sender_idempotency_key text NOT NULL,
  request_hash bytea NOT NULL CHECK (octet_length(request_hash)=32),
  UNIQUE (sender_character_id,sender_idempotency_key)
);
CREATE INDEX mail_inbox_idx ON chat.mail_messages(recipient_character_id,state,sent_at DESC);
CREATE INDEX mail_rate_idx ON chat.mail_messages(sender_character_id,sent_at DESC);
