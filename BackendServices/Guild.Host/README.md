# Guild roster host

Install `guild` with `SchemaMigration.Tool guild` under `guild_owner`; grant `guild_svc` schema usage, table DML and sequence usage. The loopback HTTP host requires `GUILD_HOST_PORT`, `GUILD_CONNECTION`, `SF_GUILD_TOKEN`, `GUILD_IDENTITY_URL`, `GUILD_IDENTITY_SERVER_ID` (matching IdentityGateway `Identity__GuildServerId`, default `srvi_guild_local`), and `SF_IDENTITY_GUILD_VERIFIER_KEY` (matching `Identity__GuildVerifierKey`). The gateway key is at least 32 characters. Do not expose this host directly to clients.

Player requests require `Authorization: Bearer <SF_GUILD_TOKEN>` and a fresh `X-Guild-Ticket`. The service-only membership lookup requires the service token but no player ticket. An authenticated IdentityGateway session obtains the ticket at `POST /v1/characters/{characterId}/guild-ticket`. The host consumes it through `/v1/guild/verify-ticket`, binding the actor and realm; a ticket cannot be replayed or used for chat/world scope. JSON endpoints:

| Method | Path | Body | Action |
| --- | --- | --- | --- |
| POST | `/v1/guilds` | `idempotencyKey`, `name`, `tag` | Create with ticket actor as rank-0 leader. |
| POST | `/v1/guilds/{id}/invitations` | `idempotencyKey`, `recipientCharacterId` | Member with invite permission invites. |
| POST | `/v1/guilds/{id}/accept` | `idempotencyKey` | Ticket actor accepts their invitation on the same realm at rank 1. |
| GET | `/v1/guilds/{id}/roster` | — | Member-only roster. |
| GET | `/v1/guild-membership/{characterId}` | — | Service-only lookup; 204 when absent. |
| POST | `/v1/guilds/{id}/ranks/{rank}/permissions` | `idempotencyKey`, `permissions` | Leader configures rank 1–9. |
| POST | `/v1/guilds/{id}/members/{characterId}/rank` | `idempotencyKey`, `rank` | Rank change under permission rules. |
| POST | `/v1/guilds/{id}/members/{characterId}/kick` | `idempotencyKey` | Kick lower member. |
| POST | `/v1/guilds/{id}/leave` | `idempotencyKey` | Leave; leader passes to successor. |
| POST | `/v1/guilds/{id}/disband` | `idempotencyKey` | Leader disbands. |

Writes persist their response against the idempotency key and reject a changed request on reuse. The schema enforces unique active guild names and tags per realm, one guild per character, and one leader; typed `GuildControls` ships the design default of 500 members with 1–500 invariant bounds. Invitations remain pending until accepted. Guild creation and membership changes write design events transactionally to guild.outbox. Schema.RosterEvents.sql upgrades existing stores to version 2. Broker relay, inactive-leader detection, shared tuning publication, and player UI remain separate work.

Focused fixture: `pwsh tools/Test-GuildRoster.ps1 -EvidenceRoot Artifacts/Persistence/GuildRoster/<fresh-child>` creates an owned PostgreSQL instance, starts IdentityGateway and Guild.Host, checks actor authorization, persistence, replay and restart, then stops all processes.
