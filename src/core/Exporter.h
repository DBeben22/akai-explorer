/*
 * AKAI Explorer - export planning shared by the GUI and the command line tool.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "AkaiSession.h"

#include <string>
#include <vector>

namespace akx {

struct ExportOptions {
    bool mergeStereo = true;   // write "-L"/"-R" sample pairs as one stereo WAV
    bool includeRaw = false;   // also export non-audio files (programs, ...) unmodified
};

struct ExportJob {
    enum class Type { Wav, StereoWav, Raw };
    Type type = Type::Wav;
    FileRef a;             // the file (left channel for StereoWav)
    FileRef b;             // right channel for StereoWav
    std::string fileName;  // host file name, unique within one plan
    std::string title;     // for progress display
};

/**
 * Plan the export of files from one volume.
 * @param positions positions in volume.files; empty means "all files"
 */
std::vector<ExportJob> planExport(const PartitionEntry &part, const VolumeEntry &volume,
                                  const std::vector<int> &positions, const ExportOptions &options);

/** Execute one job, writing into the directory destDir (must exist). */
bool runExportJob(const ExportJob &job, const std::string &destDir, std::string *error = nullptr);

} // namespace akx
