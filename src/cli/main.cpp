/*
 * akx-cli - small command line front end for the AKAI Explorer core.
 * Mainly used by the automated tests, but handy for batch conversion too.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "AkaiSession.h"
#include "Exporter.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <sys/stat.h>

using namespace akx;

static int usage()
{
    fprintf(stderr,
            "usage:\n"
            "  akx-cli list   <image>...\n"
            "  akx-cli export [--mono] [--raw] <dest-dir> <image>...\n"
            "\n"
            "list    print the directory tree (disks, partitions, volumes, files)\n"
            "export  convert every sample / DD take to WAV, one folder per volume\n"
            "        --mono  keep -L/-R sample pairs as two mono WAVs (akaiutil-identical output)\n"
            "        --raw   also write non-audio files (programs etc.) unmodified\n");
    return 2;
}

static void mkdirs(const std::string &path)
{
    for (size_t i = 1; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0777);
}

int main(int argc, char **argv)
{
    if (argc < 3)
        return usage();
    const std::string cmd = argv[1];
    ExportOptions opt;
    std::string dest;
    std::vector<std::string> images;
    int i = 2;
    if (cmd == "export") {
        for (; i < argc && argv[i][0] == '-'; ++i) {
            if (!strcmp(argv[i], "--mono"))
                opt.mergeStereo = false;
            else if (!strcmp(argv[i], "--raw"))
                opt.includeRaw = true;
            else
                return usage();
        }
        if (i >= argc)
            return usage();
        dest = argv[i++];
    } else if (cmd != "list") {
        return usage();
    }
    for (; i < argc; ++i)
        images.push_back(argv[i]);
    if (images.empty())
        return usage();

    Session &s = Session::instance();
    std::string err;
    const bool opened = s.open(images, &err);
    if (!err.empty())
        fprintf(stderr, "%s\n", err.c_str());
    if (!opened)
        return 1;

    int failures = 0;
    for (const DiskEntry &d : s.disks()) {
        if (cmd == "list")
            printf("disk%d  %s  [%s, %llu bytes]\n", d.index, d.path.c_str(), d.typeLabel.c_str(),
                   (unsigned long long)d.sizeBytes);
        for (const PartitionEntry &p : d.partitions) {
            if (cmd == "list")
                printf("  partition %s  [%s, %llu bytes, %llu free]\n", p.letter.c_str(), p.typeLabel.c_str(),
                       (unsigned long long)p.sizeBytes, (unsigned long long)p.freeBytes);
            for (const VolumeEntry &v : p.volumes) {
                if (cmd == "list") {
                    printf("    volume \"%s\"  [%s, %zu files]\n", v.name.c_str(), v.typeLabel.c_str(), v.files.size());
                    for (const FileEntry &f : v.files) {
                        printf("      %-18s %-28s %8u bytes", f.name.c_str(), f.typeLabel.c_str(), f.sizeBytes);
                        if (f.meta.valid)
                            printf("  %u Hz  %u frames  %dch  %d-bit%s%s", f.meta.sampleRate, f.meta.frames,
                                   f.meta.channels, f.meta.bits, f.stereoSide ? "  stereo-" : "",
                                   f.stereoSide ? std::string(1, f.stereoSide).c_str() : "");
                        printf("\n");
                    }
                    continue;
                }
                const std::string dir = dest + "/disk" + std::to_string(d.index) + "/" + safeFileName(p.letter) + "/"
                                        + safeFileName(v.name);
                const std::vector<ExportJob> jobs = planExport(p, v, {}, opt);
                if (jobs.empty())
                    continue;
                mkdirs(dir);
                for (const ExportJob &job : jobs) {
                    std::string e;
                    if (runExportJob(job, dir, &e)) {
                        printf("%s/%s\n", dir.c_str(), job.fileName.c_str());
                    } else {
                        fprintf(stderr, "FAILED %s: %s\n", job.title.c_str(), e.c_str());
                        ++failures;
                    }
                }
            }
        }
    }
    return failures ? 1 : 0;
}
