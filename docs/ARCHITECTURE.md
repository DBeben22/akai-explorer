# Architecture

```
            QML interface (src/app/qml)
                     |
   AppController, LibraryModel, FileModel, Player, WaveformItem   (src/app, Qt 6)
                     |
        akx_core: Session, Exporter                               (src/core, C++17, no Qt)
                     |
        akaiutil_core: akaiutil 4.6.8 C sources, unmodified       (third_party/akaiutil)
```

## akaiutil as a library

akaiutil is a command line program, but its filesystem code is cleanly separated from the
command interpreter (`akaiutil_main.c`). The application compiles every source file except
`akaiutil_main.c` into a static library and calls the same functions the interpreter
calls:

| Purpose | akaiutil function |
| --- | --- |
| open an image, find partitions | `open_disk`, `akai_scan_disk` |
| read a volume and its directory | `akai_get_vol`, `akai_get_file` |
| read file contents | `akai_read_file` |
| sample to WAV (all formats, incl. S900 compressed) | `akai_sample2wav` |
| DD take to WAV | `akai_take2wav` |

Two properties of akaiutil shape the design:

- **Global state.** Disks, partitions and the block cache are global variables. There is
  therefore exactly one `akx::Session`, and every call into akaiutil is serialized by a
  mutex. The GUI calls the session from worker threads (opening an image, decoding a
  sample, exporting), so the window stays responsive.
- **Console output.** akaiutil reports through the macros `PRINTF_OUT` and `PRINTF_ERR`,
  which `commoninclude.h` defines only if they are undefined. The library build
  force-includes `src/core/akx_shim.h`, which points them at a log buffer. The last error
  line becomes the error message shown in the interface. No akaiutil file is changed.

Images are always opened with akaiutil's read-only flag. The session has no function that
writes to an image.

## Audio path

Decoding does not reimplement any AKAI format. `Session::decode` lets akaiutil write its
normal WAV output into an anonymous temporary file and parses that WAV. Export of a mono
sample hands akaiutil the destination file directly, which is why the result is identical
to the command line tool. Only the stereo join is done by the application: it decodes
both channels and interleaves them.

`Player` converts the decoded PCM to 16-bit stereo, resamples only if the sound device
rejects the sample's own rate, and feeds a `QAudioSink` from a small `QIODevice` that
implements looping. The playhead runs on a clock and is steered towards the audible
position (read position minus queued audio), because sound servers fetch audio in chunks.

## Interface

- `LibraryModel` flattens image, partition and volume into one collapsible list. Images
  with a single partition (floppies, S900 hard disks) show their volumes directly.
- `FileModel` holds the selected volume with text filter, kind filter and multi-selection.
- `WaveformItem` and `IconItem` are `QQuickPaintedItem`s drawn with `QPainter`, so the
  application needs no icon font, no SVG module and no bundled assets.
- `Theme.qml` is the single place for colours, type sizes and spacing.

## Portability

The target is Linux. The core uses POSIX file calls and the library build uses the GCC
and Clang option `-include`. akaiutil itself also builds with Visual C++; a Windows port
of the application would need an equivalent of that option (`/FI`) and replacements for
the few POSIX calls in `AkaiSession.cpp`.
