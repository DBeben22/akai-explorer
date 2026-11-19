# Third-party code

## akaiutil 4.6.8

- Author: Klaus Michael Indlekofer, Copyright (C) 2008-2025
- Source: https://sourceforge.net/projects/akaiutil/ (file `akaiutil-4.6.8.tar.gz`,
  SHA-256 `48ea733cc38d90c2f9f70a6ff9c4fb12a8e64ddd7024d4b436e9ebeef2bf57b1`)
- Licence: GNU General Public License, version 2 or later (`akaiutil/gpl-2.0.txt`),
  plus the disclaimers at the end of `akaiutil/README.txt`

The directory `akaiutil/` holds the release exactly as distributed. No file was changed.
AKAI Explorer compiles these sources twice:

1. as a static library (`akaiutil_core`, without `akaiutil_main.c`) that the application
   links. Console output is redirected with a force-included header
   (`src/core/akx_shim.h`), which works because `commoninclude.h` only defines
   `PRINTF_OUT` and `PRINTF_ERR` when they are not defined yet.
2. as the original `akaiutil` command line tool, used by the tests to create the test
   images and as the reference implementation.

To update akaiutil, replace the contents of `akaiutil/` with a newer release and run the
tests.
