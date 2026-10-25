# Backpedal sandbox scope

World, *Cells, Movement and Handoffs* §Movement modes and base speeds sets `run` to 7.0 m/s, backpedal to 0.6 × mode speed when input is more than 120° behind facing, and `M_dir` to that factor. Security continues to use `M_dir = 1` as its upper bound. This task implements the grounded, on-foot E1 player case in `SB_Backpedal`; it uses the existing release-only movement settings with a typed factor, default 0.6, bounded to (0, 1], effective at next process startup. The server evaluates each received movement acceleration against authoritative facing. The owning client uses the same calculation for prediction; other clients receive the resulting movement.

Acceptance: an owning client runs forward, reverses while still facing forward, then runs forward again. The dedicated server measures about 700, 420, 700 cm/s and the second client observes the same ordered travel. Focused movement automation checks the angle boundary and default. No Greenlands map changes.

Owner decisions for this scope: none. Sprint is deferred because the design names a Stamina drain but gives no drain rate; shared boar loot remains owner-undecided.
