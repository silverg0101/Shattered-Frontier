# Guild roster backend slice

Backend Part 2, §9.1 and §9.4 define a realm-scoped `gld_` guild, a 3–24 character name (letters, digits, single spaces), a 2–5 letter tag, ranks 0–9, one rank-0 leader, one guild per character, and `tune.backend.guild_member_max = 500`. This slice persists creation, leader invitations, recipient acceptance, and member-only roster reads. A recipient must accept an invitation with their own IdentityGateway-bound actor ticket. Only rank 0 can invite in this first rank set; accepted members enter at rank 1. Guild names use the specified ASCII alphabet and the same pinned UTS #39 skeleton as Part 1 character names.

Mail precedes guild in the candidate list. Backend Part 2 §8.2 defines immediate delivery and a 100-message mailbox, but §4.4 requires account data-key encryption for p2 text. The current IdentityGateway account schema has no data key, so a mail body cannot yet be stored to that contract. Itemization §6.4 covers escrow in design, while the current inventory ledger has no mail escrow API; attachments are out of this slice. Backend Part 2 §8.1 gives `/v1/chat` but the infrastructure documents do not specify a deploy route or port value for it.

## Account data-key prerequisite (lane c)

The owner approved the KEK provider, DEK registry, envelope, rotation, erasure and access rules on 2026-10-24 in [BACK-account-data-keys](BACK-account-data-keys.md). The prior owner-decision block is closed. Account-key implementation is in IdentityGateway; text mail follows it. Ops provisioning, restore tombstone replay and external legal-hold coordination remain release gates.

Text-only mail can follow this key service and IdentityGateway actor binding. Attachments also require InventoryLedger mail escrow; its current transfer API does not provide the designed `EscrowItems`/`TransferEscrow` pivot.

The roster protocol uses a dedicated one-time IdentityGateway guild ticket and a separate trusted service token. Invitations are durable and do not claim an expiry value absent from the design. `GuildControls` declares the 500-member shipped default in members, with an invariant-derived 1–500 safe range and next-acceptance effect; shared control publication is a follow-up. Rank permissions, member leave/kick, leader departure, dissolution state and transactional events now extend this slice; player UI remains later work.

## Owner list after the roster continuation

- Invitation expiry and revocation policy remains unspecified.
- The permission scope for nonleader `edit_ranks` and protection against permission escalation is unspecified. This slice permits rank configuration only by the leader.
- Rank titles and default permissions for ranks 2–9 are unspecified. Newly created ranks are empty; the leader can configure permissions.
- The design fixes 90 days without world entry for inactivity, but the current identity schema records no world-entry time or active account state. The owner must define an authoritative source and invocation path before inactive succession and active-account filtering can run.
- Guild asset freeze/final release, character purge/account erasure entry, and broker relay require owning service integrations. The roster records dissolution time and transactional events but does not claim those external effects.
