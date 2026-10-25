# Development identity gateway

This loopback ASP.NET Core host implements the first account, session and character-select slice. It uses PostgreSQL for account credentials, token hashes, character envelopes and consumed ticket nonces. `CharacterAppearance` validates shipped v2 packets; `CharacterState.Postgres` remains the game-state store. The host does not initialize progression, starter grants, presence or queue admission. Set `Identity:WorldAddress` and `Identity:WorldServerId` to the owned development server endpoint; the default is `127.0.0.1:7777` / `srvi_dev_local`.

## Database and launch

Use the pinned SDK at `Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe` (or the same pinned binary from the main checkout when using a lane). The database administrator creates `identity_owner` (NOLOGIN) and `identity_service` (LOGIN), then grants `CREATE ON DATABASE <database>` to `identity_owner`. Apply the schema as owner:

```powershell
& tools/Invoke-SchemaMigrations.ps1 -Connection '<owner-capable connection>' -Role identity_owner -Schemas identity -Dotnet '<pinned dotnet.exe>' -Nuget '<package cache>'
```

Grant the runtime role only schema usage, marker read, and table DML:

```sql
GRANT USAGE ON SCHEMA identity TO identity_service;
GRANT SELECT ON identity.schema_version TO identity_service;
GRANT SELECT, INSERT, UPDATE, DELETE ON identity.accounts, identity.sessions, identity.characters, identity.ticket_uses TO identity_service;
GRANT USAGE ON SCHEMA auth TO identity_service;
GRANT SELECT, INSERT, UPDATE, DELETE ON auth.account_data_keys, auth.account_data_key_erasure, auth.erasure_tombstones TO identity_service;
GRANT INSERT ON auth.data_key_unwraps TO identity_service;
```

The runtime role must not own the schema, receive `CREATE`, or receive DML on `identity.applied_schema_files` or `identity.schema_version`. Start with `Identity__Connection` (service-role PostgreSQL string), `Identity__TicketSigningKey` (base64 of at least 32 random bytes), and `Identity__WorldVerifierKey` (independent random secret of at least 32 characters) in the process environment, then run `dotnet run --project BackendServices/IdentityGateway --no-launch-profile`. The host listens on `http://127.0.0.1:5084` unless `Identity__ListenUrl` is set. Set `SF_IDENTITY_VERIFIER_KEY` to the same verifier secret only on the dedicated game server. Keep this route on a trusted network; use TLS outside loopback. The signing key remains in the gateway and is never sent to Unreal.

The account-key migration `Schema.AccountDataKeys.sql` creates `auth.account_data_keys`, unwrap audit, erasure queue and tombstones, and adds `identity.accounts.data_key_id`. The migration owner needs `CREATE` on the database to create `auth`; the runtime role gets only the grants above. Set `Identity__KekId=v1`. In production, `FileKekProvider` reads exactly 32 raw bytes from `$CREDENTIALS_DIRECTORY/account-data-kek-v1`. `Identity__KekDevPath` is a local fixture path only. Account creation commits its wrapped DEK and pointer in the account transaction. No KEK enters a client response; the allowlisted chat service receives an unwrapped DEK from `/v1/internal/data-keys/{accountId}` for purpose `mail`, with an audit row for every unwrap. Set `Identity__DataKeyChatKey` and the matching `SF_IDENTITY_DATA_KEY_CHAT_KEY` only on the two services. Loopback uses the service allowlist key; non-loopback additionally requires a TLS client certificate matching `Identity__DataKeyChatClientThumbprint`. The key client does not retain DEKs across requests, keeping them under the five-minute maximum.

Key maintenance commands (run with the identity service's connection and KEK credentials): `dotnet IdentityGateway.dll --rewrap-kek v1` after selecting `Identity__KekId=v2` and mounting both credentials; `dotnet IdentityGateway.dll --finalize-key-erasure` after the seven-day deadline; `dotnet IdentityGateway.dll --replay-key-tombstones <account-id-file>` on a restore before serving. Keep old KEK credentials until `SELECT count(*) FROM auth.account_data_keys WHERE kek_id='v1'` is zero. The privacy coordinator must check `enforcement-service.GetLegalHold` fail-closed and call `ScheduleErasure(accountId, auditHold, now)`; release of an audit hold starts a fresh seven-day window. Export tombstone IDs independently to the audit WORM store and replay them before a restored database serves. The owner role schedules `DELETE FROM auth.data_key_unwraps WHERE at < now() - interval '90 days'`; runtime cannot delete audit rows. Backups retain their normal 35-day expiry.

On a confirmed DEK compromise, `dotnet IdentityGateway.dll --rotate-data-key <account-id>` marks the current version; the next write creates a new version, and reads of old ciphertext still work until it is re-encrypted on touch. Run KEK rewrap yearly or after KEK exposure.

Owner/ops deployment follow-up, **not run in this lane**. On the authorized auth host, with the owner's age recipient and offline destination substituted:

```sh
install -d -m 0700 /run/sf-key-bootstrap /etc/credstore.encrypted
dd if=/dev/urandom of=/run/sf-key-bootstrap/account-data-kek-v1 bs=32 count=1 status=none
chmod 0600 /run/sf-key-bootstrap/account-data-kek-v1
systemd-creds encrypt --with-key=host+tpm2 --name=account-data-kek-v1 /run/sf-key-bootstrap/account-data-kek-v1 /etc/credstore.encrypted/account-data-kek-v1
age -r "$OWNER_AGE_RECIPIENT" -o /mnt/owner-offline/account-data-kek-v1.age /run/sf-key-bootstrap/account-data-kek-v1
rm /run/sf-key-bootstrap/account-data-kek-v1
install -d -m 0755 /etc/systemd/system/auth-service.service.d
cat > /etc/systemd/system/auth-service.service.d/account-data-kek.conf <<'EOF'
[Service]
LoadCredentialEncrypted=account-data-kek-v1:/etc/credstore.encrypted/account-data-kek-v1
Environment=Identity__KekId=v1
EOF
systemctl daemon-reload
systemctl restart auth-service
```

The [systemd credential commands](https://www.freedesktop.org/software/systemd/man/258/systemd-creds.html) and [unit setting](https://www.freedesktop.org/software/systemd/man/258/systemd.exec.html) require a host with systemd and TPM2. Store the age copy offline with the owner; only the auth unit mounts the decrypted credential.

`IdentityOptions` declares values with startup bounds: session lifetime 900 s (60–1800), login challenge 60 s (10–300), connect ticket 30 s (5–120), login attempts 5 per 60 s for each IP and account (1–20 attempts, 10–3600 s), realm slots 8 (1–16), account slots 16 (1–32), restore hold 30 days (1–90), name quarantine 90 days (1–365). Configure these with `Identity__<Property>` before host start. Values take effect on new issuance or attempts; previously issued tokens retain their expiry. `Identity__ReservedNames__0` can supply an additional name. The rate buckets are process local for this development host.

## HTTP contract

JSON field names are `snake_case`. Send `Idempotency-Key` (1–128 characters) on every POST and DELETE. The gateway validates the header; durable replay receipts are pending. Errors use `{ "code": "<grpc_code>", "reason": "<UPPER_SNAKE_CASE>", "retry_after_s": n? }`. Send the session token as `Authorization: Bearer <session_token>` on character and logout routes.

| Route | Request | Success |
| --- | --- | --- |
| `POST /v1/accounts` | `{ "account_name": "alice", "password": "long secret" }` | 201 `{ "account_id": "acct_..." }` |
| `POST /v1/login/begin` | `{ "account_name": "alice", "password": "..." }` | 200 `{ "login_id": "opaque" }` |
| `POST /v1/login/complete` | `{ "login_id": "opaque" }` | 200 `{ "session_id": "ses_...", "session_token": "opaque", "expires_at": "ISO-8601" }` |
| `POST /v1/sessions/logout` | `{}` | 204 |
| `GET /v1/characters?realm_id=greenlands` | none | 200 `{ "characters": [Character] }` |
| `POST /v1/characters` | `{ "realm_id": "greenlands", "name": "Arwen", "appearance_v2_base64": "..." }` | 201 `Character` |
| `GET /v1/characters/{chr}` | none | 200 `Character` |
| `DELETE /v1/characters/{chr}` | none | 204 |
| `POST /v1/characters/{chr}/enter-world` | `{}` | 200 `{ "address": "127.0.0.1:7777", "ticket": "opaque", "expires_at_unix_s": 123 }` |
| `POST /v1/world/verify-ticket` | `{ "ticket": "opaque", "server_id": "srvi_dev_local" }`; `X-World-Verifier-Key` server header | 200 `{ "character_id": "chr_..." }`; 401 on invalid, expired, or replayed ticket |

`Character` is `{ "character_id": "chr_...", "realm_id": "...", "name": "...", "state": "active", "appearance_v2_base64": "..." }`. The packet is the binary v2 layout in [CharacterAppearance](../CharacterAppearance/README.md), base64 encoded. Cosmetics require a server-owned catalogue and entitlement check, so this slice accepts only packets with no cosmetic overrides. Character deletion hides the row and holds its name for the 30+90 day window; restore and purge are later work.

`TicketVerifier.VerifyAndConsume(ticket, expectedServerId)` validates HMAC-SHA256, expiry and server binding, then atomically inserts a nonce hash into `identity.ticket_uses`. The authenticated verifier route invokes it at dedicated-server PreLogin; a second verification returns 401. Ticket nonce rows can be pruned after expiry by an owner-operated maintenance job. Keep the signing key in the gateway and the verifier header secret on the server, never in the client. When chat is enabled, set `Identity__ChatVerifierKey` and `Identity__ChatServerId` (default `srvi_chat_local`). An authenticated session obtains a scoped one-time ticket from `POST /v1/characters/{characterId}/contacts-ticket`; Chat.Host consumes it at `POST /v1/chat/verify-ticket` using `X-Chat-Verifier-Key`. The chat verifier route is absent when its key is unset.

When guild roster is enabled, set `Identity__GuildVerifierKey` (at least 32 characters) and optionally `Identity__GuildServerId` (default `srvi_guild_local`, distinct from chat/world IDs). An authenticated session obtains a one-time ticket from `POST /v1/characters/{characterId}/guild-ticket`; Guild.Host consumes it at `POST /v1/guild/verify-ticket` with `X-Guild-Verifier-Key`. Both routes are absent when the guild verifier key is unset.

The development login provider deliberately has no email, MFA, third-party identity, refresh tokens or step-up. This slice also omits sanctions, account deletion state, world admission/presence, character restore, progression initialization and starter grants. Name validation enforces NFKC, length, punctuation, a single supported script, reserved prefixes and configured names, plus a Unicode UTS #39 18.0.0 confusable skeleton from the pinned [Unicode data](https://www.unicode.org/Public/UCD/latest/security/confusables.txt) under the bundled [Unicode License](LICENSE.unicode.txt). Content-pack NPC denylist integration is still needed before public name registration.

Run the focused test project with the pinned SDK: `dotnet run --project BackendServices/IdentityGateway.Tests -- content/appearance/human.json`.
For the full development handshake, run `pwsh tools/Test-IdentityGatewayWorld.ps1` from the lane root. It owns a loopback PostgreSQL fixture, gateway, Unreal editor dedicated server and clients, and stops them in `finally`. The installed Windows engine supports `UnrealEditor-Cmd -server` for this check; its standalone Server target is unavailable.
