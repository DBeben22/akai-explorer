/*
 * AKAI Explorer - C++ facade over the akaiutil filesystem code.
 *
 * This header is deliberately free of Qt and of akaiutil's C headers so that
 * the core can be unit-tested headlessly and reused by other front ends.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace akx {

enum class FileKind {
    Sample,
    Program,
    Drum,
    Multi,
    Effects,
    CueList,
    TakeList,
    System,
    Take,   // hard-disk recording ("DD take") on S1100/S3000 disks
    Other
};

const char *kindName(FileKind kind);

/** Parameters read from an AKAI sample header (or DD take directory entry). */
struct SampleMeta {
    bool valid = false;
    uint32_t sampleRate = 0;   // Hz
    uint32_t frames = 0;       // sample frames per channel
    int channels = 1;          // samples are mono; DD takes can be stereo
    int bits = 16;             // 12 for S900, 16 for S1000/S3000
    bool compressed = false;   // S900 compressed sample format
    int rootKey = -1;          // MIDI note, -1 if unknown
    int semitoneTune = 0;
    int centTune = 0;
    std::string playMode;      // human readable
    uint32_t startMarker = 0;
    uint32_t endMarker = 0;
    bool hasLoop = false;
    uint32_t loopStart = 0;    // in frames
    uint32_t loopEnd = 0;      // in frames (exclusive)
    int loopCount = 0;

    double seconds() const { return sampleRate ? double(frames) / sampleRate : 0.0; }
};

struct FileEntry {
    int index = -1;            // index in the volume directory (or take index)
    std::string name;          // as shown by akaiutil, e.g. "KICK.S1"
    std::string title;         // name without type suffix, e.g. "KICK"
    std::string suffix;        // e.g. "S1"
    FileKind kind = FileKind::Other;
    std::string typeLabel;     // e.g. "S1000 sample"
    uint32_t sizeBytes = 0;
    std::string osVersion;     // e.g. "4.40", empty if n/a
    SampleMeta meta;           // valid for samples and takes
    int stereoPartner = -1;    // position (in VolumeEntry::files) of the other channel
    char stereoSide = 0;       // 'L', 'R' or 0
};

struct VolumeEntry {
    int index = -1;            // index in the partition's root directory
    std::string name;
    std::string typeLabel;     // "S900", "S1000", "S3000", "CD3000", "DD takes"
    bool isTakes = false;
    std::vector<FileEntry> files;
};

struct PartitionEntry {
    int index = -1;            // index into akaiutil's global partition table
    std::string letter;        // "A", "B", ... or "DD"
    std::string typeLabel;
    uint64_t sizeBytes = 0;
    uint64_t freeBytes = 0;
    std::vector<VolumeEntry> volumes;
};

struct DiskEntry {
    int index = -1;
    std::string path;          // image file
    std::string typeLabel;     // "Floppy DD", "Floppy HD", "S900 hard disk", "Hard disk / CD-ROM"
    uint64_t sizeBytes = 0;
    std::vector<PartitionEntry> partitions;
};

/** Address of a file inside the currently opened images. */
struct FileRef {
    int part = -1;     // PartitionEntry::index
    int volume = -1;   // VolumeEntry::index
    int file = -1;     // FileEntry::index
    bool take = false; // true: `file` is a DD take index
};

/** Decoded 16-bit PCM audio. */
struct Audio {
    uint32_t sampleRate = 0;
    int channels = 1;
    std::vector<int16_t> pcm;  // interleaved
    size_t frames() const { return channels ? pcm.size() / size_t(channels) : 0; }
};

/**
 * Process-wide session. akaiutil keeps its state in global variables, so there
 * is exactly one session and every call is serialized by an internal mutex.
 * All images are opened READ-ONLY: this class never modifies a disk image.
 */
class Session {
public:
    static Session &instance();

    /** Close everything and open the given image files. Returns false if none could be opened. */
    bool open(const std::vector<std::string> &paths, std::string *error = nullptr);
    void close();

    /** Snapshot of the scanned directory tree. */
    std::vector<DiskEntry> disks();

    /** Decode a sample or DD take to PCM (uses akaiutil's converters). */
    bool decode(const FileRef &ref, Audio &out, std::string *error = nullptr);

    /** Export as WAV exactly as akaiutil's "sample2wav"/"take2wav" would write it. */
    bool exportWav(const FileRef &ref, const std::string &path, std::string *error = nullptr);

    /** Export two mono samples (left/right) as a single stereo WAV. */
    bool exportStereoWav(const FileRef &left, const FileRef &right, const std::string &path,
                         std::string *error = nullptr);

    /** Export the unmodified file contents (like akaiutil's "get"). */
    bool exportRaw(const FileRef &ref, const std::string &path, std::string *error = nullptr);

    /** Return and clear the messages akaiutil printed since the last call. */
    std::string takeLog();

private:
    Session();
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;
};

/** Replace characters that are unsafe in host file names. */
std::string safeFileName(const std::string &name);

/** Write interleaved 16-bit PCM as a canonical RIFF/WAVE file. */
bool writeWavFile(const std::string &path, const Audio &audio, std::string *error = nullptr);

/** Parse a RIFF/WAVE file image held in memory (8/16/24/32-bit PCM -> 16-bit). */
bool parseWav(const std::vector<uint8_t> &bytes, Audio &out, std::string *error = nullptr);

} // namespace akx
