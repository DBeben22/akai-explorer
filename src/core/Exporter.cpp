/*
 * AKAI Explorer - export planning shared by the GUI and the command line tool.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "Exporter.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace akx {

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

// collapse the padding AKAI puts between a name and its "-L"/"-R" suffix
std::string stereoBaseName(const std::string &title)
{
    std::string base = title.substr(0, title.size() - 2);
    while (!base.empty() && base.back() == ' ')
        base.pop_back();
    return base;
}

std::string uniqueName(std::set<std::string> &used, const std::string &stem, const std::string &ext)
{
    std::string name = stem + ext;
    for (int n = 2; used.count(lower(name)); ++n) // compare case-insensitively (Windows/macOS targets)
        name = stem + " (" + std::to_string(n) + ")" + ext;
    used.insert(lower(name));
    return name;
}

} // namespace

std::vector<ExportJob> planExport(const PartitionEntry &part, const VolumeEntry &volume,
                                  const std::vector<int> &positions, const ExportOptions &options)
{
    std::vector<int> sel = positions;
    if (sel.empty())
        for (int i = 0; i < int(volume.files.size()); ++i)
            sel.push_back(i);
    std::sort(sel.begin(), sel.end());
    sel.erase(std::unique(sel.begin(), sel.end()), sel.end());
    const std::set<int> chosen(sel.begin(), sel.end());

    auto refOf = [&](const FileEntry &f) {
        FileRef r;
        r.part = part.index;
        r.volume = volume.index;
        r.file = f.index;
        r.take = (f.kind == FileKind::Take);
        return r;
    };

    std::vector<ExportJob> jobs;
    std::set<std::string> used;
    std::set<int> done;
    for (int pos : sel) {
        if (pos < 0 || pos >= int(volume.files.size()) || done.count(pos))
            continue;
        const FileEntry &f = volume.files[pos];
        ExportJob job;
        job.a = refOf(f);
        if (f.kind == FileKind::Sample || f.kind == FileKind::Take) {
            const int partner = f.stereoPartner;
            if (options.mergeStereo && partner >= 0 && chosen.count(partner)) {
                const FileEntry &other = volume.files[partner];
                const FileEntry &l = f.stereoSide == 'L' ? f : other;
                const FileEntry &r = f.stereoSide == 'L' ? other : f;
                job.type = ExportJob::Type::StereoWav;
                job.a = refOf(l);
                job.b = refOf(r);
                job.title = stereoBaseName(l.title);
                job.fileName = uniqueName(used, safeFileName(job.title), ".wav");
                done.insert(partner);
            } else {
                job.type = ExportJob::Type::Wav;
                job.title = f.title;
                job.fileName = uniqueName(used, safeFileName(f.title), ".wav");
            }
        } else {
            if (!options.includeRaw)
                continue;
            job.type = ExportJob::Type::Raw;
            job.title = f.name;
            // keep akaiutil's type suffix (".P1", ".S3", ...) so the file can be re-imported with "put"
            const size_t dot = f.name.rfind('.');
            const std::string stem = dot == std::string::npos ? f.name : f.name.substr(0, dot);
            const std::string ext = dot == std::string::npos ? std::string() : f.name.substr(dot);
            job.fileName = uniqueName(used, safeFileName(stem), ext);
        }
        done.insert(pos);
        jobs.push_back(std::move(job));
    }
    return jobs;
}

bool runExportJob(const ExportJob &job, const std::string &destDir, std::string *error)
{
    const std::string path = destDir + "/" + job.fileName;
    Session &s = Session::instance();
    switch (job.type) {
    case ExportJob::Type::Wav: return s.exportWav(job.a, path, error);
    case ExportJob::Type::StereoWav: return s.exportStereoWav(job.a, job.b, path, error);
    case ExportJob::Type::Raw: return s.exportRaw(job.a, path, error);
    }
    return false;
}

} // namespace akx
