# Backend social slice: directed contacts

The next ready backend slice is the `chat-service` contact list. Backend Service Communication, Persistence and Content v1.0 §8.1 assigns friend and block lists to chat-service; §8.3 specifies `chat.contacts(character_id, other_character_id, kind, created_at)` with `kind` limited to `friend` or `block`. §8.1 says either direction of block prevents a whisper. Contacts are directed, so adding a friend does not require the other character to reciprocate. This slice provides service RPCs, PostgreSQL persistence, and a bilateral block lookup; chat delivery and Unreal UI remain follow-ups.

Candidates checked in order:

- Group finder/LFG: Dungeon Groups, Security and Implementation v1.0 §§2.1–2.4 and 3 specify 3,600 s queue expiry, 1,000 ms ticks, role slots (1/1/3 for five, 2/2/6 for ten), and a 30 s ready check. They name adjustable `tune.group.*` values but supply no safe bounds or effective-time rules for those controls. The tuning architecture requires those before implementation. Owner decision: bounds and effective-time rules for queue expiry, match tick, ready check, and decline penalty controls.
- Party/say/zone chat: Backend Service Communication, Persistence and Content v1.0 §8.1 defines `party` and `local` (64 m), with 30-day retention, but no `zone` channel. Owner decision: whether zone chat exists and, if so, its audience, transport, and retention.
- Friends/ignore: §8.3 gives the concrete directed `friend`/`block` data model and §8.1 gives the bilateral block rule. No adjustable value or owner decision is needed for this slice. The API uses `block` as the protocol term for ignore.
- Mail: §8.2 defines 12 attachments, a 100-message mailbox, and 30-day expiry; deferred by candidate order.
