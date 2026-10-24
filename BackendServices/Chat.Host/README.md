# Chat contacts service

`Chat.Host` is a loopback gRPC service for directed friend and block contacts. It owns no chat messages or mail yet. Install `Chat.Postgres/Schema.sql` with `SchemaMigration.Tool chat` under `chat_owner`, then grant the runtime role `USAGE` on `chat` and DML on its tables only. Start with `CHAT_HOST_PORT`, `CHAT_CONNECTION`, and `SF_CHAT_TOKEN`. Trusted gateway/server callers send `authorization: Bearer <token>` and bind `character_id` to their authenticated actor; this host validates the service token but cannot authenticate a player itself.

`AddContact` and `RemoveContact` require an idempotency key, return whether the original operation changed a row, and reject key reuse with changed input. `ListContacts` returns only outgoing edges. `IsBlocked` checks both directions, as required for whisper blocking. The service does not yet deliver whispers; callers must check block status at delivery time. Character erasure must remove both incoming and outgoing edges when the privacy participant is added.

Focused check: `pwsh tools/Test-ChatContacts.ps1 -EvidenceRoot Artifacts/Persistence/ChatContacts/<fresh-child>` uses the pinned SDK and owned PostgreSQL instance, checks protocol, persistence, DML-only runtime role, and host restart. It stops all processes it starts.
