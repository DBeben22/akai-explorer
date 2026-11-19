#!/usr/bin/env bash
# Verification test: every WAV written by AKAI Explorer's core (akx-cli --mono)
# must be byte-identical to the WAV written by the original akaiutil CLI
# ("sample2wavall" / "take2wavall") for the same disk image.
#
# Usage: compare_with_akaiutil.sh <akaiutil> <akx-cli> <testdata-dir> <work-dir>
set -euo pipefail
AKAIUTIL="$(realpath "$1")"; AKXCLI="$(realpath "$2")"; DATA="$(realpath "$3")"; WORK="$(realpath -m "$4")"

rm -rf "$WORK"; mkdir -p "$WORK"
total=0; bad=0

# check <image> <akaiutil volume path> <akx-cli relative folder> <akaiutil export command>
check() {
    local img="$DATA/$1" vol="$2" rel="$3" cmd="$4"
    # akaiutil's command parser splits at blanks: keep its export folder free of spaces
    local ref="$WORK/ref/$1/${rel// /_}"
    mkdir -p "$ref"
    printf '%s\n' "cd $vol" "lcd $ref" "$cmd" exit | "$AKAIUTIL" -r "$img" >"$WORK/akaiutil.log" 2>&1
    local out="$WORK/akx/$1/disk0/$rel"
    local n=0
    shopt -s nullglob
    for f in "$ref"/*.wav; do
        n=$((n+1)); total=$((total+1))
        if ! cmp -s "$f" "$out/$(basename "$f")"; then
            echo "MISMATCH: $1 $rel/$(basename "$f")"; bad=$((bad+1))
        fi
    done
    # and no extra / missing files on our side
    local m; m=$(find "$out" -maxdepth 1 -name '*.wav' 2>/dev/null | wc -l)
    if [ "$n" -ne "$m" ]; then echo "COUNT MISMATCH: $1 $rel (akaiutil $n, akx $m)"; bad=$((bad+1)); fi
    echo "  $1 $rel: $n file(s) compared"
}

for img in "$DATA"/*.img; do
    "$AKXCLI" export --mono "$WORK/akx/$(basename "$img")" "$img" >/dev/null
done

check floppy_s900_dd.img   "/disk0/A/VOLUME_001" "A/VOLUME 001" sample2wavall
check floppy_s1000_hd.img  "/disk0/A/NOT_NAMED"  "A/NOT NAMED"  sample2wavall
check floppy_s3000_hd.img  "/disk0/A/NOT_NAMED"  "A/NOT NAMED"  sample2wavall
check hd_s1000.img         "/disk0/A/DRUMS"      "A/DRUMS"      sample2wavall
check hd_s1000.img         "/disk0/A/TONES"      "A/TONES"      sample2wavall
check hd_s1000.img         "/disk0/B/PADS"       "B/PADS"       sample2wavall
check hd_s3000_dd.img      "/disk0/A/S3K_KIT"    "A/S3K KIT"    sample2wavall
check hd_s3000_dd.img      "/disk0/A/S1K_MIX"    "A/S1K MIX"    sample2wavall
check hd_s3000_dd.img      "/disk0/DD"           "DD/DD takes"  take2wavall
check hd_s900.img          "/disk0/A/KIT_ONE"    "A/KIT ONE"    sample2wavall
check hd_s900.img          "/disk0/A/BASSES"     "A/BASSES"     sample2wavall

echo "$total file(s) compared with akaiutil, $bad mismatch(es)"
[ "$total" -gt 0 ] && [ "$bad" -eq 0 ]
