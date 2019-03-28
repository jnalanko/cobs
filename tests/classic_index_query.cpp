/*******************************************************************************
 * tests/classic_index_query.cpp
 *
 * Copyright (c) 2018 Florian Gauger
 *
 * All rights reserved. Published under the MIT License in the LICENSE file.
 ******************************************************************************/

#include "test_util.hpp"
#include <cobs/query/classic_index/mmap.hpp>
#include <cobs/util/parameters.hpp>
#include <gtest/gtest.h>
#include <iostream>

namespace fs = cobs::fs;

static fs::path input_dir("test/classic_index_query/input");
static fs::path index_dir("test/classic_index_query/index");
static fs::path index_path(index_dir.string() + "/index.cla_idx.cobs");
static std::string query = cobs::random_sequence(50000, 2);

class classic_index_query : public ::testing::Test
{
protected:
    void SetUp() final {
        cobs::error_code ec;
        fs::remove_all(index_dir, ec);
        fs::remove_all(input_dir, ec);
    }
    void TearDown() final {
        cobs::error_code ec;
        fs::remove_all(index_dir, ec);
        fs::remove_all(input_dir, ec);
    }
};

TEST_F(classic_index_query, all_included_small_batch) {
    // generate
    auto documents = generate_documents_all(query);
    generate_test_case(documents, input_dir.string());

    // construct classic index and mmap query
    cobs::classic_index::construct(input_dir, index_dir, 16, 3, 0.1);
    cobs::query::classic_index::mmap s_mmap(index_path);

    // execute query and check results
    std::vector<std::pair<uint16_t, std::string> > result;
    s_mmap.search(query, 31, result);
    ASSERT_EQ(documents.size(), result.size());
    for (auto& r : result) {
        int index = std::stoi(r.second.substr(r.second.size() - 2));
        ASSERT_GE(r.first, documents[index].data().size());
    }
}

TEST_F(classic_index_query, one_included_small_batch) {
    // generate
    auto documents = generate_documents_one(query);
    generate_test_case(documents, input_dir.string());

    // construct classic index and mmap query
    cobs::classic_index::construct(input_dir, index_dir, 32, 3, 0.1);
    cobs::query::classic_index::mmap s_mmap(index_path);

    // execute query and check results
    std::vector<std::pair<uint16_t, std::string> > result;
    s_mmap.search(query, 31, result);
    ASSERT_EQ(documents.size(), result.size());
    for (auto& r : result) {
        ASSERT_EQ(r.first, 1);
    }
}

TEST_F(classic_index_query, one_included_large_batch) {
    // generate
    auto documents = generate_documents_one(query);
    generate_test_case(documents, input_dir.string());

    // construct classic index and mmap query
    cobs::classic_index::construct(input_dir, index_dir, 8, 3, 0.1);
    cobs::query::classic_index::mmap s_mmap(index_path);

    // execute query and check results
    std::vector<std::pair<uint16_t, std::string> > result;
    s_mmap.search(query, 31, result);
    ASSERT_EQ(documents.size(), result.size());
    for (auto& r : result) {
        ASSERT_EQ(r.first, 1);
    }
}

TEST_F(classic_index_query, false_positive) {
    // generate
    auto documents = generate_documents_all(query);
    generate_test_case(documents, input_dir.string());

    // construct classic index and mmap query
    cobs::classic_index::construct(input_dir, index_dir, 32, 3, 0.1);
    cobs::query::classic_index::mmap s_mmap(index_path);

    // execute query and check results
    size_t num_tests = 10000;
    std::map<std::string, uint64_t> num_positive;
    std::vector<std::pair<uint16_t, std::string> > result;
    for (size_t i = 0; i < num_tests; i++) {
        std::string query_2 = cobs::random_sequence(31, i);
        s_mmap.search(query_2, 31, result);

        for (auto& r : result) {
            num_positive[r.second] += r.first;
            ASSERT_TRUE(r.first == 0 || r.first == 1);
        }
    }

    for (auto& np : num_positive) {
        ASSERT_LE(np.second, 1070U);
    }
}

/******************************************************************************/