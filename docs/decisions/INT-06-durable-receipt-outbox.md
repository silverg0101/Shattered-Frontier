# INT-06: Durable follow-up receipt outbox (backend-first design)

Status: specified; implementation and acceptance are pending.

## Boundary

The originating owner stores the immutable follow-up intent in its own PostgreSQL schema in **the same transaction** as the source operation receipt. For a gather, `inventory.operation_receipts` and `inventory.receipt_outbox` commit together. A unique `(source_operation_id, destination)` key prevents a replay from enqueuing different work. The intent contains the exact serialized `ApplyReceiptRequest` (including character, skill inputs, quest evidence and occurrence time), its format version and digest. The grant rejects an intent whose source operation or digest conflicts; it never commits an item with an omitted required intent. A separate local transaction in CharacterState applies the progression and quest receipts through their existing source IDs. There is no cross-schema transaction.

`inventory.receipt_outbox` needs `source_operation_id`, `destination`, `payload_version`, `payload`, `payload_digest`, `created_at`, `next_attempt_at`, `attempts`, `lease_owner`, `lease_until`, `delivered_at`, and `last_error_code`. An ordered, checksum-verified additive migration advances the inventory schema version; runtime refuses an incompatible marker or absent table. Runtime identities receive only table DML. The unique source key and delivered tombstone survive ordinary queue cleanup.

## Relay

Each Gameplay host drains on startup and on a bounded interval. Workers claim due rows with `FOR UPDATE SKIP LOCKED` and a finite lease, commit the claim, call Progression with a deadline, then mark delivery in a new transaction. Lost replies and expired leases resend the **same bytes and original source ID**. CharacterState's existing source receipts provide exactly-once effects. Transient failure uses bounded exponential backoff with jitter; after the attempt limit, the row remains parked and visible for operator repair, never discarded. A permanent payload or identity conflict is parked immediately. A failed mark-delivered only causes a safe replay. No network call holds an inventory transaction open.

Ordering is per character: a worker must not claim a later receipt while an earlier undelivered receipt for that character exists. Independent characters can progress in parallel. This preserves progression anti-grind and quest step order. Restart, restore and multiple hosts use the persisted due time and lease; a restored point may resend, but receivers deduplicate. A backup must contain source receipt, pending rows and delivered tombstones from one inventory recovery point. Retain pending and parked rows indefinitely until resolved. Retain delivered tombstones and the source operation association for at least the supported replay, backup and restore horizon; compaction needs an owner proof that no eligible replay or reference remains. No arbitrary TTL is selected.

## Controls and failure cases

Gameplay Operations owns three typed, release-declared operating controls in the shared configuration workflow: **receipt batch size** (integer rows, default 32, bounds 1–256), **poll interval** (duration, default 2 s, bounds 1–60 s), and **max attempts** (integer deliveries before parking, default 12, bounds 1–100). The default is the fallback. Server operators may draft them for a realm; normal LiveOps approval and publication apply, with the next poll as effective time. Both operator surfaces display effective values and pending/parked counts. No console expression, environment override, or live formula is added. Lease length, backoff formula, identity validation, schema and ordering remain release-bound code.

If the origin transaction rolls back, neither item nor intent exists. If the host dies after commit but before send, startup finds the row. If it dies after receiver commit, retry reads the original receiver receipt. If CharacterState is unavailable, pending work remains and gameplay recovery does not invent success. If an earlier character receipt is parked, later receipts remain blocked until repair. A stale restore incarnation must be fenced before any relay resumes, following B-04; a numeric lease alone is not a restore fence.

## Integration gate

The current `GrantRequest` has only item, source and causation fields. Unreal constructs the skill payload, quest step, node and occurrence time **after** the grant reply. The backend cannot reconstruct those exact inputs from the committed ledger row. INT-06 therefore requires a producer contract carrying the immutable follow-up intent to the ledger **before** grant commit, plus a matching Unreal call-site change. A backend-only relay or a post-commit enqueue would leave the reported kill window open. Until that contract is allowed, the existing gather path does not satisfy this decision; a synthetic backend fixture cannot establish player-path acceptance.
