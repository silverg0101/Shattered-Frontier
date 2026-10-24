# Recent attacker memory sandbox

Scope: one defensive boar in `SB_AttackerMemory`. AI NPC Behavior and Memory v1.0 §3.1 says a defensive NPC engages after attack; §9.1 keeps actual recent attackers for **60 s after reset** and gives a seen attacker **initial threat 1**, even if defensive. §8.2 resets an evading NPC's threat and health at its anchor. Combat Attributes and Ratings v1.0, Threat & Aggro, supplies the existing sight and threat rules; this change does not alter their values.

The server records character IDs only from accepted damage, starts their 60 s memory when evade completes, and requires normal range, facing and line of sight for reacquisition. Other visible characters remain neutral. Memory stays per creature and session only. The isolated map and two-client scenario exercise attack, leash, reset, neutral control, and remembered reacquisition. No owner decision is needed for these specified rules.
