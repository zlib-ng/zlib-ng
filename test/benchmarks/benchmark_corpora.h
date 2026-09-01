/* benchmark_corpora.h -- corpus file discovery for benchmarks
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Discovers files from the zlib-ng/corpora repository which contains multiple
 * test corpora (silesia, calgary, canterbury, large, snappy, etc.).
 *
 * Clone the corpora repo:
 *   git clone https://github.com/zlib-ng/corpora test/data/corpora
 */
#ifndef BENCHMARK_CORPORA_H
#define BENCHMARK_CORPORA_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dirent.h>
#  include <sys/stat.h>
#endif

/* Absolute path defined by CMake, fallback for standalone builds */
#ifndef TEST_DATA_DIR
#  define TEST_DATA_DIR "test/data"
#endif

#define CORPORA_DIR TEST_DATA_DIR "/corpora"

struct corpus_file {
    std::string path;
    uint8_t *data;
    size_t size;
};

/* List regular files in a directory (non-recursive, skips dotfiles and empty files) */
static std::vector<corpus_file> list_files(const std::string &dir) {
    std::vector<corpus_file> files;

#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
    std::string pattern = dir + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return files;
    do {
        if (ffd.cFileName[0] == '.')
            continue;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        LARGE_INTEGER fsize;
        fsize.LowPart = ffd.nFileSizeLow;
        fsize.HighPart = ffd.nFileSizeHigh;
        if (fsize.QuadPart <= 0)
            continue;
        files.push_back({dir + "\\" + ffd.cFileName, NULL, (size_t)fsize.QuadPart});
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
#else
    DIR *d = opendir(dir.c_str());
    if (d == NULL)
        return files;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        std::string fullpath = dir + "/" + ent->d_name;
        struct stat st;
        if (stat(fullpath.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0)
            files.push_back({fullpath, NULL, (size_t)st.st_size});
    }
    closedir(d);
#endif

    std::sort(files.begin(), files.end(),
              [](const corpus_file &a, const corpus_file &b) { return a.path < b.path; });
    return files;
}

/* List subdirectories in a directory (skips dotfiles) */
static std::vector<std::string> list_subdirs(const std::string &dir) {
    std::vector<std::string> subdirs;

#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
    std::string pattern = dir + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return subdirs;
    do {
        if (ffd.cFileName[0] == '.')
            continue;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            subdirs.push_back(dir + "\\" + ffd.cFileName);
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
#else
    DIR *d = opendir(dir.c_str());
    if (d == NULL)
        return subdirs;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        std::string fullpath = dir + "/" + ent->d_name;
        struct stat st;
        if (stat(fullpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            subdirs.push_back(fullpath);
    }
    closedir(d);
#endif

    std::sort(subdirs.begin(), subdirs.end());
    return subdirs;
}

/* Load a single corpus file's data on demand */
static bool load_corpus_file(corpus_file *cf) {
    if (cf->data != NULL)
        return true;

    FILE *fp = fopen(cf->path.c_str(), "rb");
    if (fp == NULL)
        return false;

    uint8_t *buf = (uint8_t *)malloc(cf->size);
    if (buf == NULL) {
        fclose(fp);
        return false;
    }

    size_t bytes_read = fread(buf, 1, cf->size, fp);
    fclose(fp);

    if (bytes_read != cf->size) {
        free(buf);
        return false;
    }

    cf->data = buf;
    return true;
}

/* Discover corpus files without loading their contents */
static std::vector<corpus_file> discover_corpora(void) {
    std::vector<corpus_file> corpora_files;

    std::vector<std::string> subdirs = list_subdirs(CORPORA_DIR);
    for (size_t s = 0; s < subdirs.size(); s++) {
        std::vector<corpus_file> files = list_files(subdirs[s]);
        corpora_files.insert(corpora_files.end(), files.begin(), files.end());
    }

    return corpora_files;
}

#endif
