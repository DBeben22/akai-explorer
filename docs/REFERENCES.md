# References

## Software

- K. M. Indlekofer, *akaiutil: access to AKAI S900/S1000/S3000 filesystems*, release
  4.6.8 (6 January 2025), https://sourceforge.net/projects/akaiutil/. Source code and
  `README.txt` were the only source of information about the AKAI disk and file formats
  used in this project.
- The Qt Company, *Qt 6* (Qt Quick, Qt Quick Controls, Qt Multimedia, Qt Concurrent),
  https://doc.qt.io/qt-6/. Built and tested with Qt 6.4.2.
- CMake 3.28, GCC 13.3, Python 3 (test data generation only).

## Background

- K. M. Indlekofer, "Audio filesystems and data formats", research page at Hochschule
  RheinMain, https://hs-rm.de/en/rheinmain-university/people/indlekofer-klaus-michael/research-and-development/audio-filesystems-and-data-formats
- Project brief: K. M. Indlekofer, Scientific project "Digital Audio-Converter App",
  31 October 2026 (`Project_03_Indlekofer.pdf`).
- The references listed by akaiutil itself are reproduced in
  `third_party/akaiutil/README.txt` (section "References").

## Test material

- Template images for akaiutil (blank, pre-formatted floppies):
  https://sourceforge.net/projects/akaiutil/files/Templates/ . Not used here, because
  akaiutil can format a zero-filled file itself (see `tools/make_test_images.sh`).
- Z. Blade, "Blank S1000 disk images",
  https://notebook.zoeblade.com/Blank_S1000_disk_images.html . Listed as a further source
  of blank hard disk images; not used.
- No openly licensed AKAI image that already contains sounds was found. The test images
  of this project are therefore generated from synthesized sounds.

## Tools used for development

- Claude (Anthropic), model Claude Fable 5.2, used as the coding agent. The complete
  prompt log is in [WORKFLOW_AND_PROMPTS.md](WORKFLOW_AND_PROMPTS.md).
