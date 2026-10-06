#!/bin/sh
# Encode the hero frames (render_hero.py) into the site's videos + posters.
#   site/renders/make_hero_video.sh FRAMES_DIR OUT_DIR [REVIEW_DIR]
# hero_reveal      frames 1-210  (7 s)
# hero_hold_loop   frames 211-306 (3.2 s, one bar, loops seamlessly)
# Each as MP4 (H.264, CRF 20, yuv420p, faststart) and WebM (VP9, CRF 32),
# plus posters: first and last frame of the reveal, first frame of the loop.
# REVIEW_DIR (optional): a 640-px MP4 of the reveal + hold for a phone.
set -e
F="$1"; O="$2"; RV="$3"
mkdir -p "$O"
enc() { # name start count
  ffmpeg -y -loglevel error -framerate 30 -start_number "$2" -i "$F/f_%04d.jpg" -frames:v "$3" \
    -c:v libx264 -crf 20 -preset slow -pix_fmt yuv420p -movflags +faststart "$O/$1.mp4"
  ffmpeg -y -loglevel error -framerate 30 -start_number "$2" -i "$F/f_%04d.jpg" -frames:v "$3" \
    -c:v libvpx-vp9 -crf 32 -b:v 0 -row-mt 1 -pix_fmt yuv420p "$O/$1.webm"
}
enc hero_reveal 1 210
enc hero_hold_loop 211 96
cp "$F/f_0001.jpg" "$O/hero_reveal_first.jpg"
cp "$F/f_0210.jpg" "$O/hero_reveal_poster.jpg"
cp "$F/f_0211.jpg" "$O/hero_hold_loop_poster.jpg"
if [ -n "$RV" ]; then
  mkdir -p "$RV"
  ffmpeg -y -loglevel error -framerate 30 -start_number 1 -i "$F/f_%04d.jpg" -frames:v 306 \
    -vf scale=640:-2 -c:v libx264 -crf 23 -pix_fmt yuv420p -movflags +faststart "$RV/hero_reveal_and_hold_640.mp4"
fi
ls -la "$O"
