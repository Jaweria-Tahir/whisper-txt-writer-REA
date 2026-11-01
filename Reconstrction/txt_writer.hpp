// txt_writer.hpp - header-only "save recognized speech to a .txt file"
// Reconstructed from behaviour observed in whisper-cli.exe (see investigation report).
// Depends only on whisper.h from whisper.cpp and the C++17 standard library.
#pragma once

#include "whisper.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>

namespace speechtxt {

// Optional per-segment prefix (e.g. a speaker label). Return "" for no prefix.
// t0/t1 are whisper segment times in units of 10 ms.
using SpeakerFn = std::function<std::string(int segment, int64_t t0, int64_t t1)>;

struct Options {
    SpeakerFn speaker;     // empty = no prefix
    bool verbose = true;   // print status/errors to stderr like whisper-cli
};

// Output name = base + suffix (whisper-cli builds "<name>" + ".txt").
inline std::string make_txt_path(const std::string &base, const std::string &suffix = ".txt") {
    return base + suffix;
}

inline const char *skip_leading_ws(const char *s) {
    while (*s == ' ' || *s == '\t') ++s;
    return s;
}

// Writes one line per recognized segment.
inline void write_segments(whisper_context *ctx, std::ostream &out, const Options &opt) {
    const int n = whisper_full_n_segments(ctx);
    for (int i = 0; i < n; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        std::string prefix;
        if (opt.speaker) {
            prefix = opt.speaker(i, whisper_full_get_segment_t0(ctx, i),
                                    whisper_full_get_segment_t1(ctx, i));
        }
        if (prefix.empty()) text = skip_leading_ws(text);  // trim leading blanks
        out << prefix << text << "\n";
    }
}

// Saves all segments of the last whisper_full() run. path "-" writes to stdout.
// Returns false (and prints the error) if the file cannot be opened.
inline bool save_txt(whisper_context *ctx, const std::string &path, const Options &opt = Options()) {
    if (path == "-") {
        write_segments(ctx, std::cout, opt);
        return true;
    }
    std::ofstream f(path);
    if (!f.is_open()) {
        if (opt.verbose) std::fprintf(stderr, "%s: failed to open '%s' for writing\n", "save_txt", path.c_str());
        return false;
    }
    if (opt.verbose) std::fprintf(stderr, "%s: saving output to '%s'\n", "save_txt", path.c_str());
    write_segments(ctx, f, opt);
    f.flush();
    return f.good();
}

}  // namespace speechtxt
