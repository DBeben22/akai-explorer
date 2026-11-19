/*
 * AKAI Explorer - C++ facade over the akaiutil filesystem code.
 *
 * All knowledge about the AKAI on-disk formats lives in akaiutil
 * (third_party/akaiutil, Copyright (C) 2008-2025 Klaus Michael Indlekofer).
 * This file only calls into it and converts the results to C++ value types.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "AkaiSession.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "akaiutil.h"
#include "akaiutil_file.h"
#include "akaiutil_io.h"
#include "akaiutil_take.h"
#include "akaiutil_wav.h"
}

/* ------------------------------------------------------------------------- */
/* log sink for akaiutil's PRINTF_OUT / PRINTF_ERR (see akx_shim.h)          */
/* ------------------------------------------------------------------------- */

namespace {

std::mutex g_logMutex;
std::string g_log;      // everything akaiutil printed
std::string g_lastErr;  // last line printed via PRINTF_ERR

void appendLog(const char *fmt, va_list ap, bool isError)
{
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_log.size() > 256 * 1024)
        g_log.erase(0, g_log.size() / 2);
    g_log += buf;
    if (isError) {
        std::string s(buf);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        if (!s.empty())
            g_lastErr = s;
    }
}

} // namespace

extern "C" int akx_log_out(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    appendLog(fmt, ap, false);
    va_end(ap);
    return 0;
}

extern "C" int akx_log_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    appendLog(fmt, ap, true);
    va_end(ap);
    return 0;
}

namespace akx {

namespace {

std::recursive_mutex g_mutex;          // serializes every akaiutil call
std::vector<DiskEntry> g_disks;        // scanned tree
std::map<int, std::string> g_diskPath; // akaiutil disk index -> image path
bool g_cacheReady = false;

inline uint32_t le16(const u_char *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8); }
inline uint32_t le32(const u_char *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void setError(std::string *error, const std::string &fallback)
{
    if (!error)
        return;
    std::lock_guard<std::mutex> lock(g_logMutex);
    *error = g_lastErr.empty() ? fallback : (fallback + ": " + g_lastErr);
}

void clearLastError()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_lastErr.clear();
}

std::string trimRight(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
        s.pop_back();
    return s;
}

std::string osVersionString(u_int osver)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%02u", 0xff & (osver >> 8), 0xff & osver);
    return buf;
}

/* --- file type classification ------------------------------------------- */

void classify(u_int type, FileKind &kind, std::string &label)
{
    const u_char t = u_char(type);
    const char *family = "";
    char base = 0;
    if (t == u_char(AKAI_CDSETUP3000_FTYPE)) {
        kind = FileKind::System;
        label = "CD3000 CD-ROM setup";
        return;
    }
    if (t >= u_char(AKAI_S900_FTYPE_MIN) && t <= u_char(AKAI_S900_FTYPE_MAX)) {
        family = "S900";
        base = char(t);
        switch (base) {
        case 'S': kind = FileKind::Sample; label = "S900 sample"; return;
        case 'P': kind = FileKind::Program; label = "S900 program"; return;
        case 'D': kind = FileKind::Drum; label = "S900 drum settings"; return;
        case 'O': kind = FileKind::System; label = "S900 overall settings"; return;
        case 'F': kind = FileKind::System; label = "S950 fixup file"; return;
        case 'M': kind = FileKind::System; label = "S900 memory image"; return;
        default: break;
        }
    } else if (t >= u_char(AKAI_S1000_FTYPE_MIN) && t <= u_char(AKAI_S1000_FTYPE_MAX)) {
        family = "S1000";
        base = char(t);
    } else if (t >= u_char(AKAI_S3000_FTYPE_MIN) && t <= u_char(AKAI_S3000_FTYPE_MAX)) {
        family = "S3000";
        base = char(t - 0x80);
    }
    kind = FileKind::Other;
    const char *what = nullptr;
    switch (base) {
    case 's': kind = FileKind::Sample; what = "sample"; break;
    case 'h': kind = FileKind::Other; what = "CD-ROM sample parameters"; break;
    case 'p': kind = FileKind::Program; what = "program"; break;
    case 'd': kind = FileKind::Drum; what = "drum settings"; break;
    case 'm': kind = FileKind::Multi; what = "multi"; break;
    case 'x': kind = FileKind::Effects; what = "effects file"; break;
    case 'q': kind = FileKind::CueList; what = "cue list"; break;
    case 't': kind = FileKind::TakeList; what = "take list"; break;
    case 'c': kind = FileKind::System; what = "operating system"; break;
    default: break;
    }
    if (what) {
        label = std::string(family) + " " + what;
    } else {
        char buf[48];
        snprintf(buf, sizeof(buf), "%s%sfile (type 0x%02x)", family, *family ? " " : "", unsigned(t));
        label = buf;
    }
}

/* --- sample header parsing ----------------------------------------------- */

void readSampleMeta(struct file_s *fp, SampleMeta &m)
{
    const u_char t = u_char(fp->type);
    if (t == u_char(AKAI_SAMPLE900_FTYPE)) {
        struct akai_sample900_s h;
        if (fp->size < sizeof(h) || akai_read_file(-1, reinterpret_cast<u_char *>(&h), fp, 0, sizeof(h)) < 0)
            return;
        uint32_t n = le32(h.slen);
        n = 2 * ((n + 1) / 2); // akaiutil: sample count is treated as even
        m.frames = n;
        m.sampleRate = le16(h.srate);
        m.bits = 12;
        m.compressed = (fp->osver != 0);
        m.rootKey = int(le16(h.npitch) / 16);
        m.startMarker = le32(h.start);
        m.endMarker = le32(h.end);
        const uint32_t llen = le32(h.llen);
        switch (h.pmode) {
        case SAMPLE900_PMODE_ONESHOT: m.playMode = "One shot"; break;
        case SAMPLE900_PMODE_LOOP: m.playMode = "Loop"; break;
        case SAMPLE900_PMODE_ALTLOOP: m.playMode = "Alternating loop"; break;
        default: m.playMode = "Unknown"; break;
        }
        if (h.pmode != SAMPLE900_PMODE_ONESHOT && llen > 0 && m.endMarker <= n && llen <= m.endMarker) {
            m.hasLoop = true;
            m.loopCount = 1;
            m.loopEnd = m.endMarker;
            m.loopStart = m.endMarker - llen;
        }
        m.valid = true;
    } else if (t == u_char(AKAI_SAMPLE1000_FTYPE) || t == u_char(AKAI_SAMPLE3000_FTYPE)) {
        struct akai_sample1000_s h; // the S3000 header starts with the S1000 header
        if (fp->size < sizeof(h) || akai_read_file(-1, reinterpret_cast<u_char *>(&h), fp, 0, sizeof(h)) < 0)
            return;
        m.frames = le32(h.slen);
        m.sampleRate = le16(h.srate);
        m.bits = 16;
        m.rootKey = h.rkey;
        m.semitoneTune = int(static_cast<signed char>(h.stune));
        m.centTune = int(static_cast<signed char>(h.ctune));
        m.startMarker = le32(h.start);
        m.endMarker = le32(h.end);
        switch (h.pmode) {
        case SAMPLE1000_PMODE_LOOP: m.playMode = "Loop in release"; break;
        case SAMPLE1000_PMODE_LOOPNOTREL: m.playMode = "Loop until release"; break;
        case SAMPLE1000_PMODE_NOLOOP: m.playMode = "No looping"; break;
        case SAMPLE1000_PMODE_TOEND: m.playMode = "Play to sample end"; break;
        default: m.playMode = "Unknown"; break;
        }
        m.loopCount = std::min<int>(h.lnum, AKAI_SAMPLE1000_LOOPNUM);
        if (m.loopCount > 0 && h.pmode != SAMPLE1000_PMODE_NOLOOP && h.pmode != SAMPLE1000_PMODE_TOEND) {
            int li = h.lfirst < AKAI_SAMPLE1000_LOOPNUM ? h.lfirst : 0;
            const uint32_t at = le32(h.loop[li].at);
            const uint32_t len = le32(h.loop[li].len);
            if (len > 0 && at <= m.frames && len <= at) {
                m.hasLoop = true;
                m.loopEnd = at;
                m.loopStart = at - len;
            }
        }
        m.valid = true;
    }
}

/* --- stereo pair detection ------------------------------------------------ */

// AKAI convention (also used by akaiutil's WAV import): the two channels of a
// stereo recording are two mono samples whose names end in "-L" and "-R".
void linkStereoPairs(std::vector<FileEntry> &files)
{
    std::map<std::string, int> lefts;
    for (int i = 0; i < int(files.size()); ++i) {
        const FileEntry &f = files[i];
        if (f.kind == FileKind::Sample && f.title.size() >= 2 && f.title.compare(f.title.size() - 2, 2, "-L") == 0)
            lefts[f.title.substr(0, f.title.size() - 2) + "/" + f.suffix] = i;
    }
    for (int i = 0; i < int(files.size()); ++i) {
        FileEntry &r = files[i];
        if (r.kind != FileKind::Sample || r.title.size() < 2 || r.title.compare(r.title.size() - 2, 2, "-R") != 0)
            continue;
        auto it = lefts.find(r.title.substr(0, r.title.size() - 2) + "/" + r.suffix);
        if (it == lefts.end())
            continue;
        FileEntry &l = files[it->second];
        if (l.stereoPartner >= 0 || !l.meta.valid || !r.meta.valid)
            continue;
        if (l.meta.frames != r.meta.frames || l.meta.sampleRate != r.meta.sampleRate)
            continue;
        l.stereoPartner = i;
        l.stereoSide = 'L';
        r.stereoPartner = it->second;
        r.stereoSide = 'R';
    }
}

/* --- tree building -------------------------------------------------------- */

void scanVolume(struct vol_s *vp, VolumeEntry &ve)
{
    ve.index = int(vp->index);
    ve.name = trimRight(vp->name);
    switch (vp->type) {
    case AKAI_VOL_TYPE_S900: ve.typeLabel = "S900"; break;
    case AKAI_VOL_TYPE_S1000: ve.typeLabel = "S1000"; break;
    case AKAI_VOL_TYPE_S3000: ve.typeLabel = "S3000"; break;
    case AKAI_VOL_TYPE_CD3000: ve.typeLabel = "CD3000"; break;
    default: ve.typeLabel = "Unknown"; break;
    }
    const bool s900 = (vp->type == AKAI_VOL_TYPE_S900);
    for (u_int fi = 0; fi < vp->fimax; ++fi) {
        struct file_s f;
        if (akai_get_file(vp, &f, fi) < 0)
            continue;
        FileEntry fe;
        fe.index = int(fi);
        fe.name = f.name;
        char plain[AKAI_NAME_LEN + 1];
        akai2ascii_name(vp->file[fi].name, plain, s900 ? 1 : 0);
        fe.title = trimRight(plain);
        fe.suffix = fe.name.size() > fe.title.size() + 1 ? fe.name.substr(fe.title.size() + 1) : std::string();
        if (fe.name.compare(0, fe.title.size(), fe.title) != 0) { // defensive: unexpected naming
            const size_t dot = fe.name.rfind('.');
            fe.title = dot == std::string::npos ? fe.name : fe.name.substr(0, dot);
            fe.suffix = dot == std::string::npos ? std::string() : fe.name.substr(dot + 1);
        }
        classify(f.type, fe.kind, fe.typeLabel);
        fe.sizeBytes = f.size;
        if (!s900)
            fe.osVersion = osVersionString(f.osver);
        if (fe.kind == FileKind::Sample) {
            readSampleMeta(&f, fe.meta);
            if (fe.meta.compressed)
                fe.typeLabel += " (compressed)";
        }
        ve.files.push_back(std::move(fe));
    }
    linkStereoPairs(ve.files);
}

void scanTakes(struct part_s *pp, VolumeEntry &ve)
{
    ve.index = 0;
    ve.name = "DD takes";
    ve.typeLabel = "DD takes";
    ve.isTakes = true;
    for (u_int ti = 0; ti < AKAI_DDTAKE_MAXNUM; ++ti) {
        const struct akai_ddtake_s *tp = &pp->head.dd.take[ti];
        if (tp->stat == AKAI_DDTAKESTAT_FREE)
            continue;
        FileEntry fe;
        fe.index = int(ti);
        char plain[AKAI_NAME_LEN + 1];
        akai2ascii_name(const_cast<u_char *>(tp->name), plain, 0);
        fe.title = trimRight(plain);
        fe.suffix = "TK";
        fe.name = fe.title + ".TK";
        fe.kind = FileKind::Take;
        fe.typeLabel = "DD take";
        fe.meta.channels = (tp->stype == AKAI_DDTAKESTYPE_STEREO) ? 2 : 1;
        const uint32_t words = le32(tp->wend) - le32(tp->wstart);
        fe.meta.frames = words / uint32_t(fe.meta.channels);
        fe.meta.sampleRate = le16(tp->srate);
        fe.meta.bits = 16;
        fe.meta.playMode = "Hard-disk recording";
        fe.meta.valid = true;
        fe.sizeBytes = words * 2;
        ve.files.push_back(std::move(fe));
    }
}

void rebuildTree()
{
    g_disks.clear();
    for (u_int di = 0; di < disk_num; ++di) {
        DiskEntry de;
        de.index = int(di);
        de.path = g_diskPath[int(di)];
        de.sizeBytes = disk[di].totsize;
        switch (disk[di].type) {
        case DISK_TYPE_FLL: de.typeLabel = "Floppy disk, low density"; break;
        case DISK_TYPE_FLH: de.typeLabel = "Floppy disk, high density"; break;
        case DISK_TYPE_HD9: de.typeLabel = "S900 hard disk"; break;
        default: de.typeLabel = "Hard disk / CD-ROM"; break;
        }
        g_disks.push_back(std::move(de));
    }
    auto vol = std::make_unique<struct vol_s>(); // large, and must not be copied
    for (u_int pi = 0; pi < part_num; ++pi) {
        struct part_s *pp = &part[pi];
        if (!pp->valid || pp->diskp == nullptr || pp->diskp->index >= g_disks.size())
            continue;
        PartitionEntry pe;
        pe.index = int(pi);
        pe.sizeBytes = uint64_t(pp->bsize) * pp->blksize;
        pe.freeBytes = uint64_t(pp->bfree) * pp->blksize;
        if (pp->type == PART_TYPE_DD) {
            pe.letter = pp->letter == 0 ? std::string("DD") : "DD" + std::to_string(unsigned(pp->letter));
            pe.typeLabel = "Hard-disk recording partition";
            VolumeEntry ve;
            scanTakes(pp, ve);
            pe.volumes.push_back(std::move(ve));
        } else {
            pe.letter = std::string(1, pp->letter);
            switch (pp->type) {
            case PART_TYPE_FLL:
            case PART_TYPE_FLH: pe.typeLabel = "Floppy"; break;
            case PART_TYPE_HD9: pe.typeLabel = "S900 hard disk"; break;
            default: pe.typeLabel = "Sampler partition"; break;
            }
            for (u_int vi = 0; vi < pp->volnummax; ++vi) {
                if (akai_get_vol(pp, vol.get(), vi) < 0)
                    continue;
                VolumeEntry ve;
                scanVolume(vol.get(), ve);
                pe.volumes.push_back(std::move(ve));
            }
        }
        g_disks[pp->diskp->index].partitions.push_back(std::move(pe));
    }
}

void closeLocked()
{
    if (g_cacheReady) {
        flush_blk_cache(); // nothing to write (read-only), keeps akaiutil's invariants
        free_blk_cache();
    }
    close_alldisks();
    part_num = 0;
    curdiskp = nullptr;
    curpartp = nullptr;
    curvolp = nullptr;
    g_disks.clear();
    g_diskPath.clear();
}

/* --- helpers for reading akaiutil's WAV output ----------------------------- */

struct TempFile {
    FILE *f = nullptr;
    TempFile() : f(tmpfile()) {}
    ~TempFile()
    {
        if (f)
            fclose(f);
    }
    int fd() const { return f ? fileno(f) : -1; }
    bool readAll(std::vector<uint8_t> &out) const
    {
        if (!f)
            return false;
        const off_t size = lseek(fd(), 0, SEEK_END);
        if (size < 0)
            return false;
        out.resize(size_t(size));
        size_t done = 0;
        while (done < out.size()) {
            const ssize_t n = pread(fd(), out.data() + done, out.size() - done, off_t(done));
            if (n <= 0)
                return false;
            done += size_t(n);
        }
        return true;
    }
};

struct part_s *partFor(const FileRef &ref)
{
    if (ref.part < 0 || u_int(ref.part) >= part_num || !part[ref.part].valid)
        return nullptr;
    return &part[ref.part];
}

// Write the WAV for `ref` into the file descriptor `fd` using akaiutil's converters.
bool writeWavTo(const FileRef &ref, int fd, std::string *error)
{
    struct part_s *pp = partFor(ref);
    if (!pp) {
        if (error)
            *error = "the partition is no longer available";
        return false;
    }
    clearLastError();
    if (ref.take) {
        if (pp->type != PART_TYPE_DD || ref.file < 0) {
            if (error)
                *error = "not a DD take";
            return false;
        }
        if (akai_take2wav(pp, u_int(ref.file), fd, nullptr, nullptr, TAKE2WAV_CHECK | TAKE2WAV_EXPORT) != 0) {
            setError(error, "cannot convert DD take");
            return false;
        }
        return true;
    }
    auto vol = std::make_unique<struct vol_s>();
    struct file_s f;
    if (ref.volume < 0 || ref.file < 0 || akai_get_vol(pp, vol.get(), u_int(ref.volume)) < 0
        || akai_get_file(vol.get(), &f, u_int(ref.file)) < 0) {
        setError(error, "file not found in volume");
        return false;
    }
    const int r = akai_sample2wav(&f, fd, nullptr, nullptr, SAMPLE2WAV_CHECK | SAMPLE2WAV_EXPORT);
    if (r > 0) {
        if (error)
            *error = "this file is not a sample";
        return false;
    }
    if (r < 0) {
        setError(error, "cannot convert sample");
        return false;
    }
    return true;
}

int createFile(const std::string &path, std::string *error)
{
    const int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0 && error)
        *error = "cannot create \"" + path + "\": " + strerror(errno);
    return fd;
}

} // namespace

/* ------------------------------------------------------------------------- */

const char *kindName(FileKind kind)
{
    switch (kind) {
    case FileKind::Sample: return "sample";
    case FileKind::Program: return "program";
    case FileKind::Drum: return "drum";
    case FileKind::Multi: return "multi";
    case FileKind::Effects: return "effects";
    case FileKind::CueList: return "cuelist";
    case FileKind::TakeList: return "takelist";
    case FileKind::System: return "system";
    case FileKind::Take: return "take";
    default: return "other";
    }
}

Session &Session::instance()
{
    static Session s;
    return s;
}

Session::Session()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    init_blk_cache();
    blk_cache_enable = 1;
    g_cacheReady = true;
}

Session::~Session()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    closeLocked();
}

bool Session::open(const std::vector<std::string> &paths, std::string *error)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    closeLocked();
    std::string problems;
    for (const std::string &path : paths) {
        struct stat st;
        if (::stat(path.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) {
            problems += (problems.empty() ? "" : "\n") + ("cannot open \"" + path + "\"");
            continue;
        }
        const u_int before = disk_num;
        clearLastError();
        // 1: read-only, no start offset, pseudo-disk size/number determined automatically
        if (open_disk(const_cast<char *>(path.c_str()), 1, 0, 0, 0) < 0) {
            std::string e;
            setError(&e, "cannot open \"" + path + "\"");
            problems += (problems.empty() ? "" : "\n") + e;
            continue;
        }
        for (u_int i = before; i < disk_num; ++i)
            g_diskPath[int(i)] = path;
    }
    part_num = 0;
    for (u_int i = 0; i < disk_num; ++i)
        akai_scan_disk(&disk[i], 1); // 1: floppy filesystems enabled; errors are tolerated like in akaiutil
    rebuildTree();
    // an image without any recognizable AKAI filesystem is reported, but kept out of the tree
    for (auto it = g_disks.begin(); it != g_disks.end();) {
        if (it->partitions.empty()) {
            problems += (problems.empty() ? "" : "\n") + ("no AKAI filesystem found in \"" + it->path + "\"");
            it = g_disks.erase(it);
        } else {
            ++it;
        }
    }
    if (error)
        *error = problems;
    return !g_disks.empty();
}

void Session::close()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    closeLocked();
}

std::vector<DiskEntry> Session::disks()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    return g_disks;
}

bool Session::decode(const FileRef &ref, Audio &out, std::string *error)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    TempFile tmp;
    if (tmp.fd() < 0) {
        if (error)
            *error = "cannot create temporary file";
        return false;
    }
    if (!writeWavTo(ref, tmp.fd(), error))
        return false;
    std::vector<uint8_t> bytes;
    if (!tmp.readAll(bytes)) {
        if (error)
            *error = "cannot read converted audio";
        return false;
    }
    return parseWav(bytes, out, error);
}

bool Session::exportWav(const FileRef &ref, const std::string &path, std::string *error)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    const int fd = createFile(path, error);
    if (fd < 0)
        return false;
    const bool ok = writeWavTo(ref, fd, error);
    if (::close(fd) != 0 && ok) {
        if (error)
            *error = std::string("write error: ") + strerror(errno);
        return false;
    }
    if (!ok)
        ::unlink(path.c_str());
    return ok;
}

bool Session::exportStereoWav(const FileRef &left, const FileRef &right, const std::string &path,
                              std::string *error)
{
    Audio l, r;
    if (!decode(left, l, error) || !decode(right, r, error))
        return false;
    if (l.channels != 1 || r.channels != 1 || l.sampleRate != r.sampleRate || l.pcm.size() != r.pcm.size()) {
        if (error)
            *error = "left and right sample do not match";
        return false;
    }
    Audio st;
    st.sampleRate = l.sampleRate;
    st.channels = 2;
    st.pcm.resize(l.pcm.size() * 2);
    for (size_t i = 0; i < l.pcm.size(); ++i) {
        st.pcm[2 * i] = l.pcm[i];
        st.pcm[2 * i + 1] = r.pcm[i];
    }
    return writeWavFile(path, st, error);
}

bool Session::exportRaw(const FileRef &ref, const std::string &path, std::string *error)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    struct part_s *pp = partFor(ref);
    if (!pp || ref.take || pp->type == PART_TYPE_DD) {
        if (error)
            *error = "raw export is only available for sampler files";
        return false;
    }
    auto vol = std::make_unique<struct vol_s>();
    struct file_s f;
    clearLastError();
    if (ref.volume < 0 || ref.file < 0 || akai_get_vol(pp, vol.get(), u_int(ref.volume)) < 0
        || akai_get_file(vol.get(), &f, u_int(ref.file)) < 0) {
        setError(error, "file not found in volume");
        return false;
    }
    const int fd = createFile(path, error);
    if (fd < 0)
        return false;
    bool ok = akai_read_file(fd, nullptr, &f, 0, f.size) >= 0;
    if (!ok)
        setError(error, "cannot read file");
    if (::close(fd) != 0 && ok) {
        ok = false;
        if (error)
            *error = std::string("write error: ") + strerror(errno);
    }
    if (!ok)
        ::unlink(path.c_str());
    return ok;
}

std::string Session::takeLog()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::string s;
    s.swap(g_log);
    return s;
}

/* ------------------------------------------------------------------------- */

std::string safeFileName(const std::string &name)
{
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c < 0x20 || c == 0x7f || strchr("/\\:*?\"<>|", c))
            out += '_';
        else
            out += char(c);
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();
    size_t lead = 0;
    while (lead < out.size() && out[lead] == ' ')
        ++lead;
    out.erase(0, lead);
    if (out.empty() || out == "." || out == "..")
        out = "unnamed";
    return out;
}

bool writeWavFile(const std::string &path, const Audio &audio, std::string *error)
{
    const uint32_t dataBytes = uint32_t(audio.pcm.size() * 2);
    std::vector<uint8_t> buf(44 + dataBytes);
    auto put32 = [&](size_t off, uint32_t v) {
        buf[off] = v & 0xff; buf[off + 1] = (v >> 8) & 0xff; buf[off + 2] = (v >> 16) & 0xff; buf[off + 3] = (v >> 24) & 0xff;
    };
    auto put16 = [&](size_t off, uint32_t v) { buf[off] = v & 0xff; buf[off + 1] = (v >> 8) & 0xff; };
    memcpy(&buf[0], "RIFF", 4);
    put32(4, 36 + dataBytes);
    memcpy(&buf[8], "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1); // PCM
    put16(22, uint32_t(audio.channels));
    put32(24, audio.sampleRate);
    put32(28, audio.sampleRate * uint32_t(audio.channels) * 2);
    put16(32, uint32_t(audio.channels) * 2);
    put16(34, 16);
    memcpy(&buf[36], "data", 4);
    put32(40, dataBytes);
    for (size_t i = 0; i < audio.pcm.size(); ++i)
        put16(44 + 2 * i, uint16_t(audio.pcm[i]));
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) {
        if (error)
            *error = "cannot create \"" + path + "\": " + strerror(errno);
        return false;
    }
    const bool ok = fwrite(buf.data(), 1, buf.size(), f) == buf.size();
    if (fclose(f) != 0 || !ok) {
        if (error)
            *error = "write error on \"" + path + "\"";
        ::unlink(path.c_str());
        return false;
    }
    return true;
}

bool parseWav(const std::vector<uint8_t> &b, Audio &out, std::string *error)
{
    auto fail = [&](const char *msg) {
        if (error)
            *error = msg;
        return false;
    };
    auto u16 = [&](size_t o) { return uint32_t(b[o]) | (uint32_t(b[o + 1]) << 8); };
    auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
    if (b.size() < 12 || memcmp(&b[0], "RIFF", 4) != 0 || memcmp(&b[8], "WAVE", 4) != 0)
        return fail("not a RIFF/WAVE stream");
    uint32_t format = 0, channels = 0, rate = 0, bits = 0;
    bool haveFmt = false;
    size_t pos = 12;
    while (pos + 8 <= b.size()) {
        const uint32_t size = u32(pos + 4);
        const size_t body = pos + 8;
        const size_t avail = std::min<size_t>(size, b.size() - body);
        if (memcmp(&b[pos], "fmt ", 4) == 0 && avail >= 16) {
            format = u16(body);
            channels = u16(body + 2);
            rate = u32(body + 4);
            bits = u16(body + 14);
            haveFmt = true;
        } else if (memcmp(&b[pos], "data", 4) == 0) {
            if (!haveFmt)
                return fail("WAVE data before format");
            if (format != 1 || (channels != 1 && channels != 2)
                || (bits != 8 && bits != 16 && bits != 24 && bits != 32))
                return fail("unsupported WAVE format");
            const size_t bytesPer = bits / 8;
            const size_t n = avail / bytesPer;
            out.sampleRate = rate;
            out.channels = int(channels);
            out.pcm.resize(n - n % channels);
            for (size_t i = 0; i < out.pcm.size(); ++i) {
                const size_t o = body + i * bytesPer;
                int16_t v;
                if (bits == 8)
                    v = int16_t((int(b[o]) - 128) << 8);
                else // little endian: the two most significant bytes
                    v = int16_t(uint16_t(b[o + bytesPer - 2]) | (uint16_t(b[o + bytesPer - 1]) << 8));
                out.pcm[i] = v;
            }
            return true;
        }
        pos = body + size + (size & 1);
    }
    return fail("no audio data in WAVE stream");
}

} // namespace akx
