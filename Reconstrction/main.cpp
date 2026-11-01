// main.cpp - WAV (any rate/channels, 16-bit PCM) -> whisper.cpp -> <name>.txt
#include "txt_writer.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

static bool read_wav(const std::string &path, std::vector<float> &pcm) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char riff[12];
    f.read(riff, 12);
    if (!f || std::memcmp(riff, "RIFF", 4) || std::memcmp(riff + 8, "WAVE", 4)) return false;
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    while (f) {
        char id[4];
        uint32_t sz = 0;
        f.read(id, 4);
        f.read(reinterpret_cast<char *>(&sz), 4);
        if (!f) break;
        if (!std::memcmp(id, "fmt ", 4)) {
            if (sz < 16) return false;
            std::vector<char> b(sz);
            f.read(b.data(), sz);
            std::memcpy(&fmt, &b[0], 2);
            std::memcpy(&ch, &b[2], 2);
            std::memcpy(&rate, &b[4], 4);
            std::memcpy(&bits, &b[14], 2);
            if (fmt == 0xFFFE && sz >= 26) std::memcpy(&fmt, &b[24], 2);  // extensible WAV
            if (sz & 1) f.seekg(1, std::ios::cur);
        } else if (!std::memcmp(id, "data", 4)) {
            if (fmt != 1 || bits != 16 || ch < 1 || rate == 0) return false;
            std::streampos pos = f.tellg();
            f.seekg(0, std::ios::end);
            uint64_t remain = static_cast<uint64_t>(f.tellg() - pos);
            f.seekg(pos);
            uint64_t bytes = std::min<uint64_t>(sz, remain);  // some files store a wrong size
            std::vector<int16_t> s(bytes / 2);
            f.read(reinterpret_cast<char *>(s.data()), s.size() * 2);

            std::vector<float> mono;
            for (size_t i = 0; i + ch <= s.size(); i += ch) {
                float v = 0;
                for (uint16_t c = 0; c < ch; ++c) v += s[i + c];
                mono.push_back(v / ch / 32768.0f);
            }
            if (mono.empty()) return false;

            if (rate == 16000) {
                pcm = std::move(mono);
            } else {  // simple linear resample to 16 kHz
                size_t n = static_cast<size_t>(mono.size() * 16000.0 / rate);
                pcm.resize(n);
                for (size_t i = 0; i < n; ++i) {
                    double src = i * static_cast<double>(rate) / 16000.0;
                    size_t i0 = std::min(static_cast<size_t>(src), mono.size() - 1);
                    size_t i1 = std::min(i0 + 1, mono.size() - 1);
                    float fr = static_cast<float>(src - i0);
                    pcm[i] = mono[i0] * (1.0f - fr) + mono[i1] * fr;
                }
            }
            return !pcm.empty();
        } else {
            f.seekg(sz + (sz & 1), std::ios::cur);
        }
    }
    return false;
}

int main(int argc, char **argv) {
    std::string model, wav, outbase, lang = "en";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-m" && i + 1 < argc) model = argv[++i];
        else if (a == "-f" && i + 1 < argc) wav = argv[++i];
        else if (a == "-of" && i + 1 < argc) outbase = argv[++i];
        else if (a == "-l" && i + 1 < argc) lang = argv[++i];
    }
    if (model.empty() || wav.empty()) {
        std::fprintf(stderr, "usage: %s -m model.bin -f audio.wav [-of outbase] [-l lang]\n", argv[0]);
        return 1;
    }
    std::vector<float> pcm;
    if (!read_wav(wav, pcm)) {
        std::fprintf(stderr, "error: failed to read '%s' (need a 16-bit PCM WAV)\n", wav.c_str());
        return 2;
    }
    std::fprintf(stderr, "audio length: %.1f seconds\n", pcm.size() / 16000.0);
    if (pcm.size() < 17600) pcm.resize(17600, 0.0f);  // Whisper needs at least 1 second

    whisper_context *ctx = whisper_init_from_file_with_params(model.c_str(), whisper_context_default_params());
    if (!ctx) {
        std::fprintf(stderr, "error: failed to load model '%s'\n", model.c_str());
        return 3;
    }
    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.language = lang.c_str();
    p.print_progress = false;
    if (whisper_full(ctx, p, pcm.data(), static_cast<int>(pcm.size())) != 0) {
        std::fprintf(stderr, "failed to process audio\n");
        whisper_free(ctx);
        return 4;
    }
    if (whisper_full_n_segments(ctx) == 0) {
        std::fprintf(stderr, "warning: no speech was recognized (silent audio or wrong language?)\n");
        whisper_free(ctx);
        return 6;
    }
    const std::string out = speechtxt::make_txt_path(outbase.empty() ? wav : outbase);
    const bool ok = speechtxt::save_txt(ctx, out);
    whisper_free(ctx);
    return ok ? 0 : 5;
}