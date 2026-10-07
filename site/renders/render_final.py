"""Final stills for the site (owner's round 7 picks), full quality.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup \
        --python site/renders/render_final.py -- [--samples 160] [--only ID,ID]

2880 x 1620 PNGs + index.html into renders/minisite_final/ (main checkout,
gitignored). Same scenes and cameras as render_clay.py (one source of truth);
this file only picks the shots and the quality.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import render_clay  # noqa: E402

FINAL_OUT = os.path.join(render_clay.MAIN_CHECKOUT, "renders", "minisite_final")

FINAL_IDS = [
    "R2_tank_macro_a_moody", "R2_tank_macro_b_moody",   # charcoal tank, dark and low-key
    "R3_module_c",
    "R5_panel_macro_a",                                 # f/11: the controls read
    "R6_front_a", "R6_front_c",                         # panel ~58 % of frame height
    "R6_tape_a", "R6_tape_b",                           # ribbon tape v2; deep focus on the reel
    "R7_monolith_a", "R7_monolith_b", "R7_monolith_c",
    "R8_cables_all",                                    # black/red L+R pairs
]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    samples = "160"
    only = ",".join(FINAL_IDS)
    if "--samples" in argv:
        samples = argv[argv.index("--samples") + 1]
    if "--only" in argv:
        only = argv[argv.index("--only") + 1]
    sys.argv = [sys.argv[0], "--", "--exact", "--only", only, "--width", "2880", "--height", "1620",
                "--samples", samples, "--out", FINAL_OUT, "--title", "Minisite renders — final stills"]
    render_clay.main()


if __name__ == "__main__":
    main()
