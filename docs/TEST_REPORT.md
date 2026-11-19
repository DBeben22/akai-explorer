# Test report

Date: 16 November 2026. Version 0.1.0.

Environment: Ubuntu 24.04 (x86-64, Linux container), GCC 13.3.0, CMake 3.28.3, Qt 6.4.2.
The application has **not yet been run under WSL2 or on a physical desktop**; see
"Not tested" below.

## Summary

| Test | What it proves | Result |
| --- | --- | --- |
| `core_test` | Directory tree, sample parameters, decoding and export are correct for six test images | 206 checks, 0 failed |
| `compare_with_akaiutil` | WAV export is byte-identical to the original akaiutil command line tool | 39 files compared, 0 differences |
| Export through the GUI code path | Planning, folder layout, worker thread, stereo join | 34 files written, 0 errors |
| Playback through the GUI code path | Decoding in the worker thread, audio sink, playhead, end of sample | pass (silent sound device) |
| Rendering | Window, sidebar, file list, player and empty state draw without QML errors | pass (2 screenshots) |

Run the first two with `ctest --test-dir build --output-on-failure`.

## Test data

No openly licensed AKAI disk image with sound content was found, and commercial sample
CD-ROMs cannot be redistributed. The test images are therefore built from scratch on
every build:

1. `tools/gen_test_wavs.py` synthesizes 16 WAV files (sine tones, a sweep, drum-like
   sounds, a plucked string, a stereo pad, a 21-sample click, a full-scale square wave)
   with a fixed random seed.
2. `tools/make_test_images.sh` formats empty images and imports the WAVs **with the
   original akaiutil command line tool**. The images are thus produced by the reference
   implementation, not by the code under test.

| Image | Format | Content |
| --- | --- | --- |
| `floppy_s900_dd.img` | S900 floppy, 800 KB | 3 samples, 12-bit, plus 1 in the compressed format |
| `floppy_s1000_hd.img` | S1000 floppy, 1.6 MB | 5 mono samples and 1 stereo pair, incl. the 21-sample click |
| `floppy_s3000_hd.img` | S3000 floppy, 1.6 MB | 4 mono samples and 1 stereo pair |
| `hd_s1000.img` | S1000 hard disk, 6 MB | 2 partitions, 4 volumes (one empty), 11 samples |
| `hd_s3000_dd.img` | S3000 hard disk, 6 MB | S3000 and S1000 volume, DD partition with 1 stereo take |
| `hd_s900.img` | S900 hard disk, 4 MB | 2 volumes, 4 samples (one compressed) |

## Test 1: core library (`tests/core_test.cpp`)

For every image the test opens it through `akx::Session` and checks:

- the tree: number and names of partitions and volumes, volume type, number of files;
- sample parameters from the header: sample rate, length, root key, OS version, type
  label, compressed flag, stereo pair detection;
- **audio content**: every decoded sample is compared, value by value, with the WAV file
  it was imported from.

| Format | Samples compared | Largest deviation from the source | Allowed |
| --- | --- | --- | --- |
| S1000 (16-bit) | 9 | 0 | 0 |
| S3000 (16-bit) | 7 | 0 | 0 |
| DD take (16-bit stereo) | 1 | 0 | 0 |
| S900 (12-bit) | 3 | 15 of 32768 | 16 |
| S900 compressed (12-bit) | 2 | 15 of 32768 | 16 |

S1000, S3000 and DD audio comes back bit-exact. S900 samples store 12 bits, so the four
lowest bits of the 16-bit source are lost on import; a deviation below 16 is the
theoretical limit and shows that the compressed format is decoded without further loss.

Further checks in the same test:

- a joined stereo export equals the original stereo WAV exactly;
- selecting only one channel of a pair exports that channel as mono;
- raw export writes exactly the file size given in the directory;
- three images can be open at once;
- a missing file, a directory and a file without AKAI filesystem are rejected with a
  message; a good image opened together with a bad one stays usable;
- invalid file references fail cleanly and leave no partial output file;
- **all six image files are byte-identical before and after the whole test run**
  (read-only guarantee).

## Test 2: comparison with akaiutil (`tests/compare_with_akaiutil.sh`)

For each volume the original tool exports with `sample2wavall` (or `take2wavall` for the
DD partition) and `akx-cli export --mono` exports the same volume. The files are compared
with `cmp`, and the number of files must match.

Result: 39 files in 11 volumes, all byte-identical, including the AKAI header chunk that
akaiutil appends to its WAV files.

## Test 3: application code paths

These use options of the application that exist for automated checks.

- `akai-explorer --selftest-export <dir> build/testdata/*.img` runs "Export every open
  image" through `AppController` and its worker thread: 34 WAV files in the expected
  folder layout (`<image>/<partition>/<volume>/`, partition level omitted for
  single-partition images), stereo pairs joined (for example `PAD A3.wav`: 2 channels,
  44100 Hz, 88200 frames).
- `akai-explorer --selftest-play <n> <image>` loads and plays a file and logs the
  playhead. With a PulseAudio null sink (a device that discards the audio) the 0.45 s
  kick and the 1.2 s plucked string played to the end and the player returned to the
  stopped state; a 15 kHz compressed S900 sample played as well. After the first chunk
  the null sink paused for about one second before it continued at real-time speed. This
  looks like start-up latency of that device, but it has to be checked on real hardware.
- `akai-explorer --screenshot <png>` rendered the empty state and the main window with
  three images open and a stereo pair loaded (`docs/images/`). No QML warnings.

## Not tested

- **Running under WSL2 / WSLg**, and any real display or sound card. Nobody has listened
  to the audio output yet; only its timing against a silent device was checked.
- **Mouse and keyboard interaction**: clicks, drag and drop, the file and folder dialogs,
  shortcuts and the options popup were written but only rendered, not operated.
- **Real-world images**: original floppies, hard disks and CD-ROMs, large images
  (hundreds of MB), images with programs, drum settings, multis or several pseudo-disks,
  damaged images. The test set contains samples and one DD take only, so the "Other"
  filter and raw export of non-audio files have no test image.
- Samples with loops: loop points are read from the header and drawn, but none of the
  test samples has a loop.
- Qt versions other than 6.4.2, distributions other than Ubuntu 24.04, and other
  operating systems.
