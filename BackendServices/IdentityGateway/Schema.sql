CREATE TABLE identity.schema_version (singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton), version integer NOT NULL CHECK (version = 1));
INSERT INTO identity.schema_version(singleton, version) VALUES (true, 1);
CREATE TABLE identity.accounts (
    account_id text PRIMARY KEY,
    account_name_key text NOT NULL UNIQUE,
    password_hash text NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE identity.sessions (
    token_hash bytea PRIMARY KEY,
    session_id text NOT NULL UNIQUE,
    account_id text NOT NULL REFERENCES identity.accounts(account_id),
    expires_at timestamptz NOT NULL,
    ended_at timestamptz
);
CREATE INDEX sessions_account_idx ON identity.sessions(account_id);
CREATE TABLE identity.characters (
    character_id text PRIMARY KEY,
    account_id text NOT NULL REFERENCES identity.accounts(account_id),
    realm_id text NOT NULL,
    name text NOT NULL,
    name_key text NOT NULL,
    appearance bytea NOT NULL,
    state text NOT NULL CHECK (state IN ('active','deleted')),
    created_at timestamptz NOT NULL DEFAULT now(),
    deleted_at timestamptz,
    name_hold_until timestamptz
);
CREATE UNIQUE INDEX characters_realm_name_live_idx ON identity.characters(realm_id, name_key) WHERE state='active';
CREATE INDEX characters_name_hold_idx ON identity.characters(realm_id,name_key,name_hold_until);
CREATE INDEX characters_account_idx ON identity.characters(account_id, realm_id);
CREATE TABLE identity.ticket_uses (
    nonce_hash bytea PRIMARY KEY,
    expires_at timestamptz NOT NULL
);
