"""(Copied from proto/wellspring-fit docs/prototypes/diffuse-tank/.) Summarise an M6 grid render dir (sidecar JSONs): worst ringing_db over the Ringing sweeps (limit 15),
steady_tone count, and howl_ok over the Howl sweeps. Usage: m6_summary.py OUT_DIR"""
import glob, json, sys

out = sys.argv[1]
ring, howl, steady, n_ring = [], [], 0, 0
steady_cells = []
for p in glob.glob(f"{out}/m6_*/*.json"):
    if p.endswith("manifest.json"):
        continue
    j = json.load(open(p))
    m = j.get("metrics", {})
    if "howl" in p.split("/")[-2]:
        howl.append((bool(m.get("howl_ok")), p.rsplit("/", 1)[1], m.get("howl_floor_db"), m.get("howl_move_pct")))
    else:
        n_ring += 1
        ring.append((m.get("ringing_db") or 0.0, p.rsplit("/", 1)[1], m.get("ringing_hz"), bool(m.get("ringing"))))
        steady += bool(m.get("steady_tone"))
        if m.get("steady_tone"):
            steady_cells.append(p.rsplit("/", 1)[1])
ring.sort(reverse=True)
print(f"Ringing cells {n_ring}: flagged {sum(r[3] for r in ring)} (ringing_db >= 15: {sum(r[0] >= 15 for r in ring)}), steady_tone {steady}; worst:")
for r in ring[:4]:
    print(f"  {r[0]:5.1f} dB @ {r[2]} Hz  {r[1]}")
for c in steady_cells:
    print("  steady_tone:", c)
print(f"Howl cells {len(howl)}: howl_ok {sum(h[0] for h in howl)}")
for h in [h for h in howl if not h[0]][:6]:
    print("  fail:", h[1:])
