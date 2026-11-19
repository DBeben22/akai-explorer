#!/usr/bin/env bash
# Build the synthetic AKAI test disk images used by the test-suite.
#
# Usage: tools/make_test_images.sh <akaiutil-binary> <output-dir>
#
# The images are created with the *original* akaiutil CLI (formatting and WAV
# import), so they are an independent reference for testing AKAI Explorer.
# All audio is synthesized by tools/gen_test_wavs.py - no third-party content.
set -euo pipefail

AKAIUTIL="$(realpath "$1")"
OUT="$(realpath -m "$2")"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "$OUT/wav"
python3 "$HERE/gen_test_wavs.py" "$OUT/wav"

# run akaiutil non-interactively: commands on stdin, working dir = WAV folder
run() { # run <image> <commands...>
    local img="$1"; shift
    ( cd "$OUT/wav" && printf '%s\n' "$@" exit | "$AKAIUTIL" "$img" >"$img.log" 2>&1 )
}
blank() { dd if=/dev/zero of="$1" bs="$2" count="$3" status=none; }

# 1) S900 low-density floppy (800 KB): 12-bit samples, one of them compressed
img="$OUT/floppy_s900_dd.img"; blank "$img" 1024 800
run "$img" formatfloppyl9 y \
    "wav2sample9 KICK9.wav" "wav2sample9 SNARE9.wav" "wav2sample9 SINE9.wav" \
    "wav2sample9c BASS9.wav"

# 2) S1000 high-density floppy (1.6 MB): mono + stereo pair + odd-length sample
img="$OUT/floppy_s1000_hd.img"; blank "$img" 1024 1600
run "$img" formatfloppyh1 y \
    "wav2sample1 KICK.wav" "wav2sample1 SNARE.wav" "wav2sample1 HIHAT.wav" \
    "wav2sample1 PAD_A3.wav" "wav2sample1 PLUCK_C3.wav" "wav2sample1 CLICK.wav"

# 3) S3000 high-density floppy (1.6 MB)
img="$OUT/floppy_s3000_hd.img"; blank "$img" 1024 1600
run "$img" formatfloppyh3 y \
    "wav2sample3 SINE440.wav" "wav2sample3 SINE1K.wav" "wav2sample3 XSWEEP.wav" \
    "wav2sample3 BASS_E1.wav" "wav2sample3 FULLSCALE.wav"

# 4) S1000 harddisk (6 MB): two 3 MB partitions, several volumes
img="$OUT/hd_s1000.img"; blank "$img" 8192 768
run "$img" "formatharddisk1 3M 6M" y \
    "mkvol1 /disk0/A/DRUMS" "mkvol1 /disk0/A/TONES" "mkvol1 /disk0/B/PADS" "mkvol1 /disk0/B/EMPTY" \
    "cd /disk0/A/DRUMS" "wav2sample1 KICK.wav" "wav2sample1 SNARE.wav" "wav2sample1 HIHAT.wav" \
    "cd /disk0/A/TONES" "wav2sample1 SINE440.wav" "wav2sample1 SWEEP.wav" "wav2sample1 PLUCK_C3.wav" "wav2sample1 BASS_E1.wav" \
    "cd /disk0/B/PADS" "wav2sample1 PAD_A3.wav" "wav2sample1 XSWEEP.wav"

# 5) S3000 harddisk (6 MB): one 3 MB sampler partition, rest is a DD partition with one take
img="$OUT/hd_s3000_dd.img"; blank "$img" 8192 768
run "$img" "formatharddisk3 3M 3M" y \
    "mkvol3 /disk0/A/S3K_KIT" "mkvol1 /disk0/A/S1K_MIX" \
    "cd /disk0/A/S3K_KIT" "wav2sample3 KICK.wav" "wav2sample3 SNARE.wav" "wav2sample3 PAD_A3.wav" \
    "cd /disk0/A/S1K_MIX" "wav2sample1 SINE1K.wav" "wav2sample3 FULLSCALE.wav" \
    "cd /disk0/DD" "wav2take XSWEEP.wav"

# 6) S900 harddisk (4 MB)
img="$OUT/hd_s900.img"; blank "$img" 8192 512
run "$img" "formatharddisk9 4M" y \
    "mkvol9 /disk0/A/KIT_ONE" "mkvol9 /disk0/A/BASSES" \
    "cd /disk0/A/KIT_ONE" "wav2sample9 KICK9.wav" "wav2sample9 SNARE9.wav" \
    "cd /disk0/A/BASSES" "wav2sample9c BASS9.wav" "wav2sample9 SINE9.wav"

# reference listing produced by the original tool
for img in "$OUT"/*.img; do
    ( cd "$OUT" && printf '%s\n' lsrec exit | "$AKAIUTIL" -r "$img" >"$img.lsrec.txt" 2>&1 )
done
echo "test images written to $OUT"
