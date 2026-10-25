# BACK account data keys — owner decision, 2026-10-24

Approved decisions:
1. KEK store now: systemd-creds (host+tpm2) mounted only into auth-service; offline age-encrypted copy held by the owner. OpenBao Transit at launch scale (same table and envelope; only the wrapper changes). Code: a KEK provider interface with a file/credential provider (reads $CREDENTIALS_DIRECTORY or a dev path) used by tests and dev; an OpenBao provider is NOT built now.
2. DEK store: auth.account_data_keys(account_id, key_version, kek_id, wrapped_dek bytea, messages_written bigint, created_at, retired_at); accounts.data_key_id points at it. Wrap = AES-256-GCM under the KEK, AAD = account_id.
3. Envelope (bytea): 0x01 | key_version uint16 BE | 12-byte random nonce (RandomNumberGenerator) | ciphertext | 16-byte tag. AAD = UTF-8 "{account_id}|{table}.{column}|{record_id}", rebuilt at read, never stored. System.Security.Cryptography.AesGcm.
4. KEK rotation yearly or on exposure, rewrap only (keep old KEK until no rows use it). DEK rotation lazy: on compromise or at 2^32 messages per key (counter); write newest version, decrypt any version, re-encrypt on next touch.
5. Erasure: retire the key row (retired_at) for 7 days, then hard delete it and null data_key_id, write erasure tombstones; skip while an audit hold applies, per the Privacy design. Backups expire naturally (35 d); restores replay tombstones before serving. Ledger, item ownership, audit hash chain, tombstone ids stay plaintext; mail from an erased sender shows "erased".
6. Audit table auth.data_key_unwraps(account_id, service, purpose, at), append-only, 90-day retention, no key material. GetDataKey restricted by the existing internal mTLS / client allowlist; cache 5 min as the design states.
