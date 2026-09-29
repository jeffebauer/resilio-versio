# 0029 — Earlier first echo (pickups closer to the drive end)

**Status:** Accepted, 29 Sep 2026 (owner). Amends the pickup position (SpringModes.h, M8 item 5) and the high path (Spring.h).

**Context:** The owner heard a slapback-like gap between the dry signal and the reverb, distracting at low MIX, and not there before TENSION (ADR 0026). Before TENSION, L followed DECAY (30–100 ms), so short and medium tails also had a short gap; now the loose tank stays long (110 ms) whatever DECAY does. Measured on clicks (first sound = within 20 dB of the first 250 ms's peak): Wellspring 32 ms first / 41 ms loudest; ours at TENSION 0 45 / 61 ms, noon 28 / 38, tight 13 / 18. Two paths set it: the Loop's pickup at 0.52 L, and the high path, whose output was read after its full delay (0.43 L).

**Options put to the owner:** earlier first echo at every TENSION, same echo spacing; a shorter loose tank (110 → ~85 ms, faster repeats); tie the gap to DECAY again; keep it. **Chosen: earlier first echo.**

**Decision:** Loop pickup at 0.36 L (`kPickupArrival`, was 0.52); the high path gets a pickup too, reading its line at 0.70 of L_hf (≈ 0.30 L, `kHighPickup`) while its feedback still repeats every L_hf. Every echo moves earlier by the same amount; L, L_hf and so the echo spacing are unchanged. Now 32 / 43 ms loose (Wellspring 32 / 41), 20 / 27 noon, 9 / 13 tight. The high path still arrives a little ahead of the Loop (0.30 vs 0.36 L; was 0.43 vs 0.52 L).

**Cost:** one more interpolated read per Spring per sample (high path pickup).
