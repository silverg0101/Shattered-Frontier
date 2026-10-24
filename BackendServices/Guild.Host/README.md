# Guild roster host

Install `guild` with `SchemaMigration.Tool guild` under `guild_owner`; grant `guild_svc` schema usage and DML only. The loopback HTTP host requires `GUILD_HOST_PORT`, `GUILD_CONNECTION`, `SF_GUILD_TOKEN`, `GUILD_IDENTITY_URL`, `GUILD_IDENTITY_SERVER_ID` (matching IdentityGateway `Identity__GuildServerId`, default `srvi_guild_local`), and `SF_IDENTITY_GUILD_VERIFIER_KEY` (matching `Identity__GuildVerifierKey`). The gateway key is at least 32 characters. Do not expose this host directly to clients.

Every request requires `Authorization: Bearer <SF_GUILD_TOKEN>` and a fresh `X-Guild-Ticket`. An authenticated IdentityGateway session obtains the ticket at `POST /v1/characters/{characterId}/guild-ticket`. The host consumes it through `/v1/guild/verify-ticket`, binding the actor and realm; a ticket cannot be replayed or used for chat/world scope. JSON endpoints:

| Method | Path | Body | Action |
| --- | --- | --- | --- |
| POST | `/v1/guilds` | `idempotencyKey`, `name`, `tag` | Create with ticket actor as rank-0 leader. |
| POST | `/v1/guilds/{id}/invitations` | `idempotencyKey`, `recipientCharacterId` | Rank-0 leader invites. |
| POST | `/v1/guilds/{id}/accept` | `idempotencyKey` | Ticket actor accepts their invitation on the same realm at rank 1. |
| GET | `/v1/guilds/{id}/roster` | — | Member-only roster. |

Writes persist their response against the idempotency key and reject a changed request on reuse. The schema enforces unique active guild names and tags per realm, one guild per character, and one leader; typed `GuildControls` ships the design default of 500 members with 1–500 invariant bounds. Invitations remain pending until accepted. Rank management, succession, departure, dissolution, events, shared tuning publication, and player UI are separate slices.

Focused fixture: `pwsh tools/Test-GuildRoster.ps1 -EvidenceRoot Artifacts/Persistence/GuildRoster/<fresh-child>` creates an owned PostgreSQL instance, starts IdentityGateway and Guild.Host, checks actor authorization, persistence, replay and restart, then stops all processes.
