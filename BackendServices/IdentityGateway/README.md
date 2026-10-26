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
GRANT SELECT, INSERT ON identity.character_create_requests TO identity_service;
GRANT USAGE ON SCHEMA auth TO identity_service;
GRANT SELECT, INSERT, UPDATE, DELETE ON auth.account_data_keys, auth.account_data_key_erasure TO identity_service;
GRANT SELECT, INSERT ON auth.data_key_unwraps, auth.erasure_tombstones TO identity_service;
```

The runtime role must not own the schema, receive `CREATE`, or receive DML on `identity.applied_schema_files` or `identity.schema_version`. Create a separate `identity_maint` LOGIN role for `--maintain-data-keys`, with its own password and only these grants after the account-key migration:

```sql
GRANT CONNECT ON DATABASE <database> TO identity_maint;
GRANT USAGE ON SCHEMA identity, auth TO identity_maint;
GRANT SELECT(account_id), UPDATE(data_key_id) ON identity.accounts TO identity_maint;
GRANT SELECT, UPDATE, DELETE ON auth.account_data_key_erasure TO identity_maint;
GRANT SELECT(account_id), DELETE ON auth.account_data_keys TO identity_maint;
GRANT SELECT(account_id), INSERT(account_id, erased_at) ON auth.erasure_tombstones TO identity_maint;
GRANT SELECT(at), DELETE ON auth.data_key_unwraps TO identity_maint;
```

`identity_owner` remains NOLOGIN and is used only for migrations. For local development without `CREDENTIALS_DIRECTORY`, start with `Identity__Connection` (service-role PostgreSQL string), `Identity__TicketSigningKey` (base64 of at least 32 random bytes), and `Identity__WorldVerifierKey` (independent random secret of at least 32 characters) in the process environment, then run `dotnet run --project BackendServices/IdentityGateway --no-launch-profile`. The host listens on `http://127.0.0.1:5084` unless `Identity__ListenUrl` is set. Set `SF_IDENTITY_VERIFIER_KEY` to the same verifier secret only on the dedicated game server. Keep this route on a trusted network; use TLS outside loopback. The signing key remains in the gateway and is never sent to Unreal.

The account-key migration `Schema.AccountDataKeys.sql` creates `auth.account_data_keys`, unwrap audit, erasure queue and tombstones, and adds `identity.accounts.data_key_id`. The migration owner needs `CREATE` on the database to create `auth`; the runtime role gets only the grants above. Set `Identity__KekId=v1`. In production, `FileKekProvider` reads exactly 32 raw bytes from `$CREDENTIALS_DIRECTORY/account-data-kek-v1`. `Identity__KekDevPath` is rejected outside Development. Startup logs the provider name and KEK ID, never the key. Account creation commits its wrapped DEK and pointer in the account transaction. No KEK enters a client response. The internal data-key routes require a separate HTTPS listener, a pinned client certificate SHA256 (`Identity__DataKeyChatClientSha256`), and `Identity__DataKeyChatKey`. Chat uses `CHAT_IDENTITY_INTERNAL_URL`, `CHAT_IDENTITY_CLIENT_CERT_PATH`, and `CHAT_IDENTITY_SERVER_CERT_SHA256`. The key client does not retain DEKs across requests. Each request to unwrap a key creates one audit row; inbox reuses a version within the request.

Key maintenance commands: `dotnet IdentityGateway.dll --rewrap-kek v1` after selecting `Identity__KekId=v2` and mounting both credentials; `dotnet IdentityGateway.dll --replay-key-tombstones <account-id-file>` on a restore before serving. Keep old KEK credentials until `SELECT count(*) FROM auth.account_data_keys WHERE kek_id='v1'` is zero. The coordinator submits a verified, post-grace erasure handoff to `POST /v1/internal/privacy/accounts/{accountId}/erasure` with `request_id`, `verified_at`, `legal_hold_checked_at` (within five minutes), and `audit_hold` from a fail-closed `enforcement-service.GetLegalHold`. Missing `verified_at` or `audit_hold` is rejected. The handoff requires its own pinned client certificate (`Identity__PrivacyClientSha256`) and `X-Privacy-Workflow-Key` (`Identity__PrivacyWorkflowKey`). A hold-release recheck submits `audit_hold=false` and starts a fresh seven-day window; repeating a completed handoff returns 202 without changing the tombstone. This handoff is the phase-4 key step; player intake, step-up and the 14-day grace remain with the privacy workflow. The daily restricted-role `--maintain-data-keys` job finalizes due erasures and deletes unwrap audits older than 90 days. Chat purges expired mail ciphertext hourly. Export tombstone IDs independently to the audit WORM store and replay them before a restored database serves. Backups retain their normal 35-day expiry.

On a confirmed DEK compromise, `dotnet IdentityGateway.dll --rotate-data-key <account-id>` marks the current version; the next write creates a new version, and reads of old ciphertext still work until it is re-encrypted on touch. Run KEK rewrap yearly or after KEK exposure.

The Compose delivery in [ops/compose.yaml](ops/compose.yaml) is separate from `ops/steam-server/compose.yaml`, which deploys only the game server. Build the default `sf-identity:local` and `sf-chat:local` images from the repository root with `docker build -f BackendServices/IdentityGateway/Dockerfile -t sf-identity:local .` and `docker build -f BackendServices/Chat.Host/Dockerfile -t sf-chat:local .`; set `SF_IDENTITY_IMAGE` and `SF_CHAT_IMAGE` only for separately verified images. On the authorized backend host, copy Compose and `decrypt-secrets.sh` to `/opt/shattered-frontier/backend/` and the four units in `ops/` to `/etc/systemd/system/`. Install `decrypt-secrets.sh` root-owned at mode 0755; it must not be writable by any other user or group. Set `/etc/shattered-frontier/backend.env` with only the non-secret Compose variables (`SF_UID`, `SF_GID`, service URLs and certificate pins). Supply credential files described below; no secret value belongs in Compose environment or backend.env. The KEK is attached only to `identity`. The internal key listener binds solely to the Compose `keyplane` address; no host port is published. Configure the identity server and chat client PFX files at the paths in Compose and give only their container UIDs read access. The maintenance job connects as `identity_maint` and receives no KEK. Validate with `docker compose --env-file /etc/shattered-frontier/backend.env -f compose.yaml config` before starting.

Pre-flight on the authorized host: run `systemd-creds has-tpm2`, `command -v age`, and `mountpoint -q /mnt/owner-offline`. Stop if age or the offline mount is missing. The owner chooses whether a host without TPM2 may use `systemd-creds encrypt --with-key=host` instead of `--with-key=host+tpm2`; record that decision before provisioning. Keep the offline mount disconnected after backup.

On that host, with the owner's age recipient and offline destination substituted:

```sh
install -d -m 0700 /run/sf-key-bootstrap /etc/credstore.encrypted
dd if=/dev/urandom of=/run/sf-key-bootstrap/account-data-kek-v1 bs=32 count=1 status=none
chmod 0600 /run/sf-key-bootstrap/account-data-kek-v1
systemd-creds encrypt --with-key=host+tpm2 --name=account-data-kek-v1 /run/sf-key-bootstrap/account-data-kek-v1 /etc/credstore.encrypted/account-data-kek-v1
age -r "$OWNER_AGE_RECIPIENT" -o /mnt/owner-offline/account-data-kek-v1.age /run/sf-key-bootstrap/account-data-kek-v1
shred -u /run/sf-key-bootstrap/account-data-kek-v1
```

Provision the other encrypted credential names the same way: `identity-connection` (identity_service DB string), `identity-maintenance-connection` (identity_maint DB string), `chat-connection` (chat DB string), `ticket-signing-key`, `world-verifier-key`, `chat-verifier-key`, `data-key-chat-key`, `privacy-workflow-key`, `chat-token`, `chat-privacy-token`, `gameplay-token`, and `guild-token`. Keep every private credential input under `/run/sf-key-bootstrap` on tmpfs. Use `systemd-creds encrypt --with-key=host+tpm2 --name=<name> <private-input> /etc/credstore.encrypted/<name>` or the owner-approved `--with-key=host` fallback, then `shred -u <private-input>` after encryption. `sf-auth-kek.service` mounts `/run/sf-secrets` as tmpfs and invokes `decrypt-secrets.sh`; each decrypted file is owned by the container UID/GID at mode 0400. The daily timer runs erasure finalization and audit retention with `identity_maint`. Store the age copy offline with the owner. These host steps have not been run by this lane.

After every encrypted credential is provisioned, start the units:

```sh
systemctl daemon-reload
systemctl enable --now sf-backend-compose.service sf-auth-maintenance.timer
```

The mTLS pins are SHA256 hashes of the certificate raw bytes, so expiry alone does not invalidate a pin. Before either identity-server or chat-client certificate expires, issue a replacement certificate, compute its SHA256 pin, update the corresponding `SF_IDENTITY_SERVER_CERT_SHA256` or `SF_CHAT_CLIENT_CERT_SHA256` in backend.env, install the matching PFX, and restart the stack in one maintenance window. For the privacy client certificate, coordinate the new `SF_PRIVACY_CLIENT_CERT_SHA256` and PFX with the privacy service. Verify mutual TLS and handoff/key calls after rotation, then retire the old PFX and pin.

`IdentityOptions` declares values with startup bounds: session lifetime 900 s (60–1800), login challenge 60 s (10–300), connect ticket 30 s (5–120), login attempts 5 per 60 s for each IP and account (1–20 attempts, 10–3600 s), realm slots 8 (1–16), account slots 16 (1–32), restore hold 30 days (1–90), name quarantine 90 days (1–365). Configure these with `Identity__<Property>` before host start. Values take effect on new issuance or attempts; previously issued tokens retain their expiry. `Identity__ReservedNames__0` can supply an additional name. The rate buckets are process local for this development host.

## HTTP contract

JSON field names are `snake_case`. Send `Idempotency-Key` (1–128 characters) on every POST and DELETE. `POST /v1/characters` retains the key for the account and atomically stores the normalized request fingerprint and original 201 response with the character; exact retries return that response and changed requests return `IDEMPOTENCY_KEY_CONFLICT`. Other routes only validate the header. Errors use `{ "code": "<grpc_code>", "reason": "<UPPER_SNAKE_CASE>", "retry_after_s": n? }`. Send the session token as `Authorization: Bearer <session_token>` on character and logout routes.

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

Run the focused test project with the pinned SDK: `dotnet run --project BackendServices/IdentityGateway.Tests -- content/appearance/human.json`. `pwsh tools/Test-IdentityGatewayCreate.ps1` provisions an owned loopback PostgreSQL instance, applies the identity owner migration and runtime grants, and checks concurrent create, changed-key payload, and gateway restart replay without Unreal.
For the full development handshake, run `pwsh tools/Test-IdentityGatewayWorld.ps1` from the lane root. It owns a loopback PostgreSQL fixture, gateway, Unreal editor dedicated server and clients, and stops them in `finally`. The installed Windows engine supports `UnrealEditor-Cmd -server` for this check; its standalone Server target is unavailable.
