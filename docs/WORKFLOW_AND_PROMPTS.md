# Workflow and prompts

This file documents how AKAI Explorer was produced, as required by the project brief
("documentation of workflow and prompts").

## Setup

- **Method:** vibe coding. All code, tests and documentation in this repository (except
  `third_party/akaiutil/`) were written by an AI coding agent from natural-language
  prompts. No line was typed by hand so far.
- **Agent:** Claude (Anthropic), model Claude Fable 5.2, in the Claude desktop app,
  working in a Linux sandbox (Ubuntu 24.04) with shell access, and writing the result to
  the project folder. The brief names OpenCode / Ollama / Qwen as an example toolchain;
  a different agent was used here.
- **Session:** one conversation on the evening of 16 November 2026.
- **Human input:** four messages and two rounds of multiple-choice answers, all listed
  below verbatim.

## Prompt log

### Prompt 1

Attachment: `Project_03_Indlekofer.pdf` (the project brief).

> Read this project file and build a modern looking application as described in the pdf
> file. It should fit into the aesthetic of modern music applications. Ask me about
> details you need to have clarified before you start

The agent read the brief and asked four questions. Answers chosen:

| Question | Answer |
| --- | --- |
| Which backend: akaiutil, cmios9 or both? | akaiutil |
| Which Qt GUI technology? | Qt 6 Quick/QML |
| Which features in the first version? | Browse and export; audio preview. (Not chosen: write to images; batch conversion.) |
| Where will the app be compiled and run? | Linux, this computer |

The agent then found that the computer runs Windows and that its sandbox could not
download from SourceForge, and asked three more questions:

| Question | Answer |
| --- | --- |
| Which Linux environment on the Windows machine? | WSL2 |
| How to get the akaiutil source (4.6.7 was the version the agent had found)? | "There is also a version 4.6.8, should I attach that for you?" |
| Link akaiutil's C sources or call the CLI binary? | Link the C sources |

The agent asked for the 4.6.8 tarball, one or two test images, and a project folder.

### Prompt 2

Attachment: `akaiutil-4.6.8.tar.gz`. The project folder was connected at the same time.

> Where can I find open source test images for akaiutil?

The agent searched the web and answered that it had found blank template images only,
no openly licensed image with sound content, and proposed to generate test images from
synthesized sounds. It then built the application without further prompts.

## What the agent did, in order

1. **Read the brief** and extracted goal, means, to-dos and expected results.
2. **Checked the licence** of akaiutil (GPL-2.0-or-later, plus disclaimers in
   `README.txt`) and compared the vendored copy with the tarball (identical).
3. **Studied akaiutil**: headers, `akai_sample2wav`, `akai_take2wav`, `akai_read_file`,
   the start-up sequence in `akaiutil_main.c`, and the format commands in `README.txt`.
   Findings that shaped the design: the sources link without `akaiutil_main.c`; state is
   global; console output goes through overridable macros.
4. **Built the original CLI** and used it to create six test images from synthesized
   WAVs (`tools/`).
5. **Wrote the core library** (`src/core`) and `akx-cli`, then the tests, and ran them
   until all passed. One test script bug was found and fixed here: akaiutil's command
   parser splits arguments at blanks, so export folders with spaces in their names
   received no files.
6. **Wrote the Qt application** (`src/app`): models, player, painted items, QML.
7. **Looked at the result** through offscreen screenshots and fixed what they showed:
   - the waveform stayed empty because the item's property `player` shadowed the context
     object of the same name (renamed to `source`);
   - scroll bars were visible when nothing could be scrolled;
   - sizes were shown as "KiB", sidebar labels were cut off, a 21-sample click showed a
     length of "0.00 s" (now "0.5 ms").
8. **Checked playback** against a silent PulseAudio device. The playhead moved in
   steps of about 0.1 s because the sound server fetches audio in chunks; it now runs on
   a clock that is steered towards the audible position.
9. **Checked export through the GUI code path** with a self-test option.
10. **Wrote the documentation** and delivered the project folder.

## Design decisions made by the agent

These were not specified in the prompts. They are listed so they can be reviewed.

| Decision | Reason |
| --- | --- |
| akaiutil sources are vendored unmodified; output is redirected by a force-included header | Updating akaiutil stays a file copy; no fork to maintain |
| Images are opened read-only, no write features | Matches the chosen scope; rules out damage to archive images |
| Decoding reuses akaiutil's WAV writer through a temporary file | No second implementation of the sample formats; export is identical to the CLI |
| Stereo pairs (`-L`/`-R`) are joined on export and in the preview, switchable | Convenient for use in a DAW; the akaiutil-identical mono output remains available |
| One session, one mutex, work in background threads | akaiutil uses global state |
| Core library without Qt, plus `akx-cli` | Core can be tested headlessly and reused |
| Test images generated at build time from synthesized sounds | No redistributable real images were found |
| Dark interface with LCD-blue accent and amber playhead; icons and waveform painted in C++ | "Aesthetic of modern music applications"; no bundled fonts or icon assets to license |
| Programs are listed but not interpreted | Out of the chosen scope |

## How to continue this log

Append each further prompt verbatim, with the date, the agent and model used, and a
one-line note on what changed. Keep failed attempts too: the brief asks for the workflow,
not only for the successful path.
