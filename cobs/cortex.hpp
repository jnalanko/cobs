/*******************************************************************************
 * cobs/cortex.hpp
 *
 * Copyright (c) 2018 Florian Gauger
 *
 * All rights reserved. Published under the MIT License in the LICENSE file.
 ******************************************************************************/

#ifndef COBS_CORTEX_HEADER
#define COBS_CORTEX_HEADER
#pragma once

#include <string>
#include <vector>

#include <cobs/sample.hpp>
#include <cobs/util/file.hpp>
#include <cobs/util/fs.hpp>
#include <cobs/util/timer.hpp>
#include <cstring>
#include <iomanip>

namespace cobs::cortex {

struct header {
    uint32_t version;
    uint32_t kmer_size;
    uint32_t num_words_per_kmer;
    uint32_t num_colors;
    std::string name;
};

std::vector<char> v;
timer t;

template <typename size_type, typename ForwardIterator>
size_type cast(ForwardIterator iter) {
    return *reinterpret_cast<const size_type*>(&(*iter));
}

template <typename Type, typename ForwardIterator>
Type cast_advance(ForwardIterator& iter) {
    Type t = *reinterpret_cast<const Type*>(&(*iter));
    std::advance(iter, sizeof(Type));
    return t;
}

template <typename ForwardIterator>
void check_magic_number(ForwardIterator& iter) {
    std::string magic_word = "CORTEX";
    for (size_t i = 0; i < magic_word.size(); i++) {
        if (*iter != magic_word[i]) {
            throw std::invalid_argument("magic number does not match");
        }
        std::advance(iter, 1);
    }
}

template <typename ForwardIterator>
header skip_header(ForwardIterator& iter) {
    header h;
    check_magic_number(iter);
    h.version = cast_advance<uint32_t>(iter);
    if (h.version != 6) {
        throw std::invalid_argument(
                  "invalid .ctx file version (" + std::to_string(h.version));
    }
    h.kmer_size = cast_advance<uint32_t>(iter);
    assert(h.kmer_size == 31);
    h.num_words_per_kmer = cast_advance<uint32_t>(iter);
    h.num_colors = cast<uint32_t>(iter);
    if (h.num_colors != 1) {
        throw std::invalid_argument(
                  "invalid number of colors (" + std::to_string(h.num_colors) + "), must be 1");
    }
    std::advance(iter, 16 * h.num_colors);
    for (size_t i = 0; i < h.num_colors; i++) {
        auto sample_name_length = cast_advance<uint32_t>(iter);
        h.name.assign(iter, iter + sample_name_length);
        std::advance(iter, sample_name_length);
    }
    std::advance(iter, 16 * h.num_colors);
    for (size_t i = 0; i < h.num_colors; i++) {
        std::advance(iter, 12);
        auto length_graph_name = cast_advance<uint32_t>(iter);
        std::advance(iter, length_graph_name);
    }
    check_magic_number(iter);
    return h;
}

template <typename ForwardIterator, unsigned int N>
void read_sample(ForwardIterator iter, ForwardIterator end, header h, sample<N>& sample) {
    auto sample_data = reinterpret_cast<uint8_t*>(sample.data().data());
    size_t num_uint8_ts_per_kmer = 8 * h.num_words_per_kmer;

    while (iter != end) {
        if (std::distance(iter, end) < (int64_t)num_uint8_ts_per_kmer + 5 * h.num_colors) {
            throw std::invalid_argument("corrupted .ctx file");
        }
        std::copy(iter, std::next(iter, num_uint8_ts_per_kmer), sample_data);
        std::advance(iter, num_uint8_ts_per_kmer + 5 * h.num_colors);
        std::advance(sample_data, num_uint8_ts_per_kmer);
    }

    t.active("sort");
    // std::sort(reinterpret_cast<uint64_t*>(&(*sample.data().begin())),
    //           reinterpret_cast<uint64_t*>(&(*sample.data().end())));
    // disabled sorting -tb 2018-09-17 (is this only needed for frequency counting?)
    // std::sort(sample.data().begin(), sample.data().end());
}

template <unsigned int N>
void process_file(const fs::path& in_path, const fs::path& out_path, sample<N>& s) {
    t.active("read");
    read_file(in_path, v);
    if (!v.empty()) {
        t.active("iter");
        auto iter = v.begin();
        auto h = skip_header(iter);
        s.data().resize(
            std::distance(iter, v.end()) / (8 * h.num_words_per_kmer + 5 * h.num_colors));

        read_sample(iter, v.end(), h, s);
        t.active("write");
        file::serialize<31>(out_path, s, h.name);
    }
    t.stop();
}

template <unsigned int N>
void process_all_in_directory(const fs::path& in_dir, const fs::path& out_dir) {
    sample<N> sample;
    t.reset();
    size_t i = 0;
    for (fs::recursive_directory_iterator end, it(in_dir); it != end; it++) {
        fs::path out_path = out_dir / it->path().stem().concat(file::sample_header::file_extension);
        if (fs::is_regular_file(*it) &&
            it->path().extension().string() == ".ctx" &&
            it->path().string().find("uncleaned") == std::string::npos &&
            !fs::exists(out_path))
        {
            std::cout << "BE - " << std::setfill('0') << std::setw(7) << i
                      << " - " << it->path().string() << std::flush;
            bool success = true;
            try {
                process_file(it->path(), out_path, sample);
            }
            catch (const std::exception& e) {
                std::cerr << it->path().string() << " - " << e.what()
                          << " - " << std::strerror(errno) << std::endl;
                success = false;
                t.stop();
            }
            std::cout << "\r" << (success ? "OK" : "ER")
                      << " - " << std::setfill('0') << std::setw(7) << i
                      << " - " << it->path().string() << std::endl;
            i++;
        }
    }
    std::cout << t;
}

} // namespace cobs::cortex

#endif // !COBS_CORTEX_HEADER

/******************************************************************************/