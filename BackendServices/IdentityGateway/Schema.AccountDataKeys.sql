CREATE SCHEMA IF NOT EXISTS auth;
CREATE TABLE auth.account_data_keys (
    account_id text NOT NULL REFERENCES identity.accounts(account_id),
    key_version integer NOT NULL CHECK (key_version BETWEEN 1 AND 65535),
    kek_id text NOT NULL,
    wrapped_dek bytea NOT NULL CHECK (octet_length(wrapped_dek) = 60),
    messages_written bigint NOT NULL DEFAULT 0 CHECK (messages_written >= 0),
    created_at timestamptz NOT NULL DEFAULT now(),
    retired_at timestamptz,
    data_key_id text GENERATED ALWAYS AS (account_id || ':' || key_version::text) STORED,
    UNIQUE (account_id, data_key_id),
    PRIMARY KEY (account_id, key_version)
);
ALTER TABLE identity.accounts ADD COLUMN data_key_id text;
ALTER TABLE identity.accounts ADD CONSTRAINT accounts_data_key_fk FOREIGN KEY (account_id, data_key_id)
    REFERENCES auth.account_data_keys(account_id, data_key_id) DEFERRABLE INITIALLY DEFERRED;
CREATE TABLE auth.data_key_unwraps (
    account_id text NOT NULL,
    service text NOT NULL,
    purpose text NOT NULL,
    at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX data_key_unwraps_at_idx ON auth.data_key_unwraps(at);
CREATE TABLE auth.erasure_tombstones (
    account_id text PRIMARY KEY,
    erased_at timestamptz NOT NULL
);
CREATE TABLE auth.account_data_key_erasure (
    account_id text PRIMARY KEY,
    erase_after timestamptz NOT NULL,
    audit_hold boolean NOT NULL DEFAULT false
);
REVOKE UPDATE, DELETE, TRUNCATE ON auth.data_key_unwraps FROM PUBLIC;
