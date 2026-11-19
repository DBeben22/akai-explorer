/*
 * AKAI Explorer - headless tests of the core library against the synthetic
 * test images (see tools/make_test_images.sh).
 *
 * Usage: akx_core_test <testdata-dir>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "AkaiSession.h"
#include "Exporter.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace akx;

static int g_checks = 0, g_failed = 0;
static std::string g_data;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(cond)) {                                                                    \
            ++g_failed;                                                                   \
            printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                    \
        }                                                                                 \
    } while (0)

static std::vector<uint8_t> readFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static Audio loadWav(const std::string &path)
{
    Audio a;
    std::string err;
    if (!parseWav(readFile(path), a, &err))
        printf("  cannot load %s: %s\n", path.c_str(), err.c_str());
    return a;
}

static Audio channel(const Audio &a, int ch)
{
    Audio m;
    m.sampleRate = a.sampleRate;
    for (size_t i = size_t(ch); i < a.pcm.size(); i += size_t(a.channels))
        m.pcm.push_back(a.pcm[i]);
    return m;
}

// largest absolute difference; `b` may be shorter than `a` by `pad` zero-padded samples
static int maxDiff(const Audio &a, const Audio &b, size_t pad = 0)
{
    if (a.sampleRate != b.sampleRate || a.channels != b.channels || a.pcm.size() < b.pcm.size()
        || a.pcm.size() - b.pcm.size() > pad)
        return 1 << 20;
    int d = 0;
    for (size_t i = 0; i < a.pcm.size(); ++i) {
        const int ref = i < b.pcm.size() ? b.pcm[i] : 0;
        d = std::max(d, std::abs(int(a.pcm[i]) - ref));
    }
    return d;
}

static const VolumeEntry *findVolume(const std::vector<DiskEntry> &disks, const std::string &letter,
                                     const std::string &name, const PartitionEntry **partOut = nullptr)
{
    for (const DiskEntry &d : disks)
        for (const PartitionEntry &p : d.partitions)
            if (p.letter == letter)
                for (const VolumeEntry &v : p.volumes)
                    if (v.name == name) {
                        if (partOut)
                            *partOut = &p;
                        return &v;
                    }
    return nullptr;
}

static int findFile(const VolumeEntry &v, const std::string &name)
{
    for (int i = 0; i < int(v.files.size()); ++i)
        if (v.files[i].name == name)
            return i;
    return -1;
}

static FileRef refOf(const PartitionEntry &p, const VolumeEntry &v, int pos)
{
    FileRef r;
    r.part = p.index;
    r.volume = v.index;
    r.file = v.files[pos].index;
    r.take = v.files[pos].kind == FileKind::Take;
    return r;
}

// decode `name` from the volume and compare with the reference WAV it was imported from
static void checkSample(const PartitionEntry &p, const VolumeEntry &v, const std::string &name,
                        const Audio &reference, int tolerance, size_t pad = 0)
{
    const int pos = findFile(v, name);
    CHECK(pos >= 0);
    if (pos < 0) {
        printf("    (missing file %s)\n", name.c_str());
        return;
    }
    const FileEntry &f = v.files[pos];
    CHECK(f.meta.valid);
    CHECK(f.meta.sampleRate == reference.sampleRate);
    Audio a;
    std::string err;
    const bool ok = Session::instance().decode(refOf(p, v, pos), a, &err);
    CHECK(ok);
    if (!ok) {
        printf("    decode %s: %s\n", name.c_str(), err.c_str());
        return;
    }
    CHECK(a.frames() == f.meta.frames);
    const int d = maxDiff(a, reference, pad * size_t(reference.channels));
    CHECK(d <= tolerance);
    printf("    %-20s %6u Hz %7zu frames  max. deviation from source: %d LSB (allowed %d)\n", name.c_str(),
           a.sampleRate, a.frames(), d, tolerance);
}

static bool openOne(const std::string &image)
{
    std::string err;
    const bool ok = Session::instance().open({g_data + "/" + image}, &err);
    if (!ok || !err.empty())
        printf("  open %s: %s\n", image.c_str(), err.c_str());
    return ok;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <testdata-dir>\n", argv[0]);
        return 2;
    }
    g_data = argv[1];
    const std::string wav = g_data + "/wav/";
    Session &s = Session::instance();
    const char *images[] = {"floppy_s900_dd.img", "floppy_s1000_hd.img", "floppy_s3000_hd.img",
                            "hd_s1000.img", "hd_s3000_dd.img", "hd_s900.img"};
    std::vector<std::vector<uint8_t>> before;
    for (const char *img : images)
        before.push_back(readFile(g_data + "/" + img));

    // 12-bit S900 data: the 4 low bits of the 16-bit source are lost
    const int TOL12 = 16;

    printf("[1] S1000 HD floppy\n");
    CHECK(openOne("floppy_s1000_hd.img"));
    {
        const auto disks = s.disks();
        CHECK(disks.size() == 1 && disks[0].partitions.size() == 1);
        CHECK(disks[0].typeLabel == "Floppy disk, high density");
        const PartitionEntry *p = nullptr;
        const VolumeEntry *v = findVolume(disks, "A", "NOT NAMED", &p);
        CHECK(v != nullptr);
        if (v) {
            CHECK(v->typeLabel == "S1000");
            CHECK(v->files.size() == 7);
            checkSample(*p, *v, "KICK.S1", loadWav(wav + "KICK.wav"), 0);
            checkSample(*p, *v, "SNARE.S1", loadWav(wav + "SNARE.wav"), 0);
            checkSample(*p, *v, "HIHAT.S1", loadWav(wav + "HIHAT.wav"), 0);
            checkSample(*p, *v, "PLUCK C3.S1", loadWav(wav + "PLUCK_C3.wav"), 0);
            checkSample(*p, *v, "CLICK.S1", loadWav(wav + "CLICK.wav"), 0);
            const Audio pad = loadWav(wav + "PAD_A3.wav");
            checkSample(*p, *v, "PAD A3    -L.S1", channel(pad, 0), 0);
            checkSample(*p, *v, "PAD A3    -R.S1", channel(pad, 1), 0);
            const int l = findFile(*v, "PAD A3    -L.S1"), r = findFile(*v, "PAD A3    -R.S1");
            CHECK(l >= 0 && r >= 0 && v->files[l].stereoPartner == r && v->files[r].stereoPartner == l);
            CHECK(l >= 0 && v->files[l].stereoSide == 'L');
            const int k = findFile(*v, "KICK.S1");
            CHECK(k >= 0 && v->files[k].stereoPartner == -1 && v->files[k].kind == FileKind::Sample);
            CHECK(k >= 0 && v->files[k].meta.frames == 19845 && v->files[k].meta.rootKey == 60);
            CHECK(k >= 0 && v->files[k].typeLabel == "S1000 sample" && v->files[k].osVersion == "4.40");

            // export plan: stereo pair merged into one file, 7 files -> 6 jobs
            const auto jobs = planExport(*p, *v, {}, ExportOptions());
            CHECK(jobs.size() == 6);
            char tmpl[] = "/tmp/akx_test_XXXXXX";
            const std::string dir = mkdtemp(tmpl);
            bool sawStereo = false;
            for (const ExportJob &j : jobs) {
                std::string err;
                CHECK(runExportJob(j, dir, &err));
                if (j.type == ExportJob::Type::StereoWav) {
                    sawStereo = true;
                    CHECK(j.fileName == "PAD A3.wav");
                    const Audio st = loadWav(dir + "/" + j.fileName);
                    CHECK(st.channels == 2 && maxDiff(st, pad) == 0);
                    printf("    stereo export %-10s identical to the stereo source WAV: %s\n", j.fileName.c_str(),
                           maxDiff(st, pad) == 0 ? "yes" : "NO");
                }
            }
            CHECK(sawStereo);
            // selecting only one channel exports that channel as mono
            const auto single = planExport(*p, *v, {l}, ExportOptions());
            CHECK(single.size() == 1 && single[0].type == ExportJob::Type::Wav);
            // raw export keeps the file size
            ExportOptions rawOpt;
            rawOpt.includeRaw = true;
            std::string err;
            CHECK(s.exportRaw(refOf(*p, *v, k), dir + "/KICK.S1", &err));
            CHECK(readFile(dir + "/KICK.S1").size() == v->files[k].sizeBytes);
        }
    }

    printf("[2] S3000 HD floppy\n");
    CHECK(openOne("floppy_s3000_hd.img"));
    {
        const auto disks = s.disks();
        const PartitionEntry *p = nullptr;
        const VolumeEntry *v = findVolume(disks, "A", "NOT NAMED", &p);
        CHECK(v != nullptr);
        if (v) {
            CHECK(v->typeLabel == "S3000" && v->files.size() == 6);
            checkSample(*p, *v, "SINE440.S3", loadWav(wav + "SINE440.wav"), 0);
            checkSample(*p, *v, "SINE1K.S3", loadWav(wav + "SINE1K.wav"), 0);
            checkSample(*p, *v, "BASS E1.S3", loadWav(wav + "BASS_E1.wav"), 0);
            checkSample(*p, *v, "FULLSCALE.S3", loadWav(wav + "FULLSCALE.wav"), 0);
            const Audio x = loadWav(wav + "XSWEEP.wav");
            checkSample(*p, *v, "XSWEEP    -L.S3", channel(x, 0), 0);
            checkSample(*p, *v, "XSWEEP    -R.S3", channel(x, 1), 0);
        }
    }

    printf("[3] S900 DD floppy (12-bit, incl. compressed sample)\n");
    CHECK(openOne("floppy_s900_dd.img"));
    {
        const auto disks = s.disks();
        CHECK(disks.size() == 1 && disks[0].typeLabel == "Floppy disk, low density");
        const PartitionEntry *p = nullptr;
        const VolumeEntry *v = disks.empty() || disks[0].partitions.empty() || disks[0].partitions[0].volumes.empty()
                                   ? nullptr : &disks[0].partitions[0].volumes[0];
        CHECK(v != nullptr);
        if (v) {
            p = &disks[0].partitions[0];
            CHECK(v->typeLabel == "S900" && v->files.size() == 4);
            // S900 sample length is even: odd-length sources are zero-padded by one sample
            checkSample(*p, *v, "KICK9.S9", loadWav(wav + "KICK9.wav"), TOL12, 1);
            checkSample(*p, *v, "SNARE9.S9", loadWav(wav + "SNARE9.wav"), TOL12, 1);
            checkSample(*p, *v, "SINE9.S9", loadWav(wav + "SINE9.wav"), TOL12, 1);
            checkSample(*p, *v, "BASS9.S9C", loadWav(wav + "BASS9.wav"), TOL12, 1);
            const int c = findFile(*v, "BASS9.S9C");
            CHECK(c >= 0 && v->files[c].meta.compressed && v->files[c].meta.bits == 12);
        }
    }

    printf("[4] S1000 hard disk, 2 partitions\n");
    CHECK(openOne("hd_s1000.img"));
    {
        const auto disks = s.disks();
        CHECK(disks.size() == 1 && disks[0].partitions.size() == 2);
        const PartitionEntry *p = nullptr;
        const VolumeEntry *drums = findVolume(disks, "A", "DRUMS", &p);
        CHECK(drums && drums->files.size() == 3);
        const VolumeEntry *tones = findVolume(disks, "A", "TONES", &p);
        CHECK(tones && tones->files.size() == 4);
        if (tones)
            checkSample(*p, *tones, "SWEEP.S1", loadWav(wav + "SWEEP.wav"), 0);
        const VolumeEntry *pads = findVolume(disks, "B", "PADS", &p);
        CHECK(pads && pads->files.size() == 4);
        if (pads) {
            const Audio x = loadWav(wav + "XSWEEP.wav");
            checkSample(*p, *pads, "XSWEEP    -R.S1", channel(x, 1), 0);
            CHECK(planExport(*p, *pads, {}, ExportOptions()).size() == 2);
        }
        const VolumeEntry *empty = findVolume(disks, "B", "EMPTY", &p);
        CHECK(empty && empty->files.empty());
    }

    printf("[5] S3000 hard disk with DD partition\n");
    CHECK(openOne("hd_s3000_dd.img"));
    {
        const auto disks = s.disks();
        CHECK(disks.size() == 1 && disks[0].partitions.size() == 2);
        const PartitionEntry *p = nullptr;
        const VolumeEntry *kit = findVolume(disks, "A", "S3K KIT", &p);
        CHECK(kit && kit->typeLabel == "S3000" && kit->files.size() == 4);
        const VolumeEntry *mix = findVolume(disks, "A", "S1K MIX", &p);
        CHECK(mix && mix->typeLabel == "S1000" && mix->files.size() == 2);
        const VolumeEntry *dd = findVolume(disks, "DD", "DD takes", &p);
        CHECK(dd && dd->isTakes && dd->files.size() == 1);
        if (dd && dd->files.size() == 1) {
            CHECK(dd->files[0].kind == FileKind::Take && dd->files[0].meta.channels == 2);
            checkSample(*p, *dd, "XSWEEP.TK", loadWav(wav + "XSWEEP.wav"), 0);
        }
    }

    printf("[6] S900 hard disk\n");
    CHECK(openOne("hd_s900.img"));
    {
        const auto disks = s.disks();
        CHECK(disks.size() == 1 && disks[0].typeLabel == "S900 hard disk");
        const PartitionEntry *p = nullptr;
        const VolumeEntry *kit = findVolume(disks, "A", "KIT ONE", &p);
        CHECK(kit && kit->files.size() == 2);
        const VolumeEntry *bass = findVolume(disks, "A", "BASSES", &p);
        CHECK(bass && bass->files.size() == 2);
        if (bass)
            checkSample(*p, *bass, "BASS9.S9C", loadWav(wav + "BASS9.wav"), TOL12, 1);
    }

    printf("[7] several images at once\n");
    {
        std::string err;
        CHECK(s.open({g_data + "/hd_s1000.img", g_data + "/floppy_s3000_hd.img", g_data + "/hd_s900.img"}, &err));
        CHECK(err.empty());
        const auto disks = s.disks();
        CHECK(disks.size() == 3);
        const PartitionEntry *p = nullptr;
        const VolumeEntry *v = findVolume(disks, "A", "NOT NAMED", &p);
        CHECK(v != nullptr);
        if (v)
            checkSample(*p, *v, "SINE440.S3", loadWav(wav + "SINE440.wav"), 0);
    }

    printf("[8] error handling\n");
    {
        std::string err;
        CHECK(!s.open({g_data + "/does-not-exist.img"}, &err));
        CHECK(!err.empty());
        err.clear();
        CHECK(!s.open({wav + "KICK.wav"}, &err)); // a file without AKAI filesystem
        CHECK(!err.empty());
        printf("    non-AKAI file rejected with: %s\n", err.c_str());
        CHECK(s.disks().empty());
        err.clear();
        CHECK(!s.open({g_data}, &err)); // a directory
        // good image + bad image: the good one is still usable, the problem is reported
        err.clear();
        CHECK(s.open({g_data + "/hd_s1000.img", wav + "KICK.wav"}, &err));
        CHECK(!err.empty() && s.disks().size() == 1);
        Audio a;
        FileRef bogus;
        CHECK(!s.decode(bogus, a, &err));
        bogus.part = 0;
        bogus.volume = 99;
        bogus.file = 3;
        CHECK(!s.decode(bogus, a, &err));
        bogus.volume = 0;
        bogus.file = 100; // free directory entry
        CHECK(!s.decode(bogus, a, &err));
        CHECK(!s.exportWav(bogus, "/tmp/akx_should_not_exist.wav", &err));
        CHECK(access("/tmp/akx_should_not_exist.wav", F_OK) != 0);
        CHECK(safeFileName("A/B:C*?") == "A_B_C__");
        CHECK(safeFileName("  ..") == "unnamed");
    }
    s.close();

    printf("[9] images are never modified (read-only access)\n");
    for (size_t i = 0; i < before.size(); ++i)
        CHECK(readFile(g_data + "/" + images[i]) == before[i]);

    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
