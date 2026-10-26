-- A successful create and its replay response commit in the same transaction.
-- Account-scoped keys remain reserved until account erasure; no time-based retry window.
CREATE TABLE identity.character_create_requests (
    account_id text NOT NULL REFERENCES identity.accounts(account_id) ON DELETE CASCADE,
    idempotency_key text NOT NULL CHECK (length(idempotency_key) BETWEEN 1 AND 128),
    request_hash bytea NOT NULL CHECK (octet_length(request_hash) = 32),
    response jsonb NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (account_id, idempotency_key)
);
