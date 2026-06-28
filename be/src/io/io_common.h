// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <gen_cpp/Types_types.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace doris {

enum class ReaderType : uint8_t {
    READER_QUERY = 0,
    READER_ALTER_TABLE = 1,
    READER_BASE_COMPACTION = 2,
    READER_CUMULATIVE_COMPACTION = 3,
    READER_CHECKSUM = 4,
    READER_COLD_DATA_COMPACTION = 5,
    READER_SEGMENT_COMPACTION = 6,
    READER_FULL_COMPACTION = 7,
    READER_BINLOG_COMPACTION = 8,
    READER_BINLOG = 9,
    UNKNOWN = 10
};

namespace io {

enum SniiSectionType : uint8_t {
    SNII_SECTION_UNKNOWN = 0,
    SNII_SECTION_META = 1,
    SNII_SECTION_DICT = 2,
    SNII_SECTION_POSTING = 3,
    SNII_SECTION_BSBF = 4,
    SNII_SECTION_NORMS = 5,
    SNII_SECTION_NULL_BITMAP = 6,
    SNII_SECTION_COUNT = 7,
};

struct FileReaderStats {
    size_t read_calls = 0;
    size_t read_bytes = 0;
    int64_t read_time_ns = 0;
    size_t read_rows = 0;
};

struct FileCacheStatistics {
    int64_t num_local_io_total = 0;
    int64_t num_remote_io_total = 0;
    int64_t num_peer_io_total = 0;
    int64_t local_io_timer = 0;
    int64_t bytes_read_from_local = 0;
    int64_t bytes_read_from_remote = 0;
    int64_t bytes_read_from_peer = 0;
    int64_t remote_physical_read_count = 0;
    int64_t remote_physical_read_bytes = 0;
    int64_t peer_physical_read_count = 0;
    int64_t peer_physical_read_bytes = 0;
    int64_t remote_io_timer = 0;
    int64_t peer_io_timer = 0;
    int64_t remote_wait_timer = 0;
    int64_t write_cache_io_timer = 0;
    int64_t bytes_write_into_cache = 0;
    int64_t file_cache_blocks_total = 0;
    int64_t file_cache_blocks_hit = 0;
    int64_t file_cache_blocks_miss = 0;
    int64_t file_cache_blocks_skip = 0;
    int64_t file_cache_blocks_downloading = 0;
    int64_t num_skip_cache_io_total = 0;
    int64_t read_cache_file_directly_timer = 0;
    int64_t cache_get_or_set_timer = 0;
    int64_t lock_wait_timer = 0;
    int64_t get_timer = 0;
    int64_t set_timer = 0;

    int64_t inverted_index_num_local_io_total = 0;
    int64_t inverted_index_num_remote_io_total = 0;
    int64_t inverted_index_num_peer_io_total = 0;
    int64_t inverted_index_bytes_read_from_local = 0;
    int64_t inverted_index_bytes_read_from_remote = 0;
    int64_t inverted_index_bytes_read_from_peer = 0;
    int64_t inverted_index_remote_physical_read_count = 0;
    int64_t inverted_index_remote_physical_read_bytes = 0;
    int64_t inverted_index_peer_physical_read_count = 0;
    int64_t inverted_index_peer_physical_read_bytes = 0;
    int64_t inverted_index_bytes_write_into_cache = 0;
    int64_t inverted_index_file_cache_blocks_total = 0;
    int64_t inverted_index_file_cache_blocks_hit = 0;
    int64_t inverted_index_file_cache_blocks_miss = 0;
    int64_t inverted_index_file_cache_blocks_skip = 0;
    int64_t inverted_index_file_cache_blocks_downloading = 0;
    int64_t inverted_index_local_io_timer = 0;
    int64_t inverted_index_remote_io_timer = 0;
    int64_t inverted_index_peer_io_timer = 0;
    int64_t inverted_index_io_timer = 0;
    int64_t inverted_index_request_bytes = 0;
    int64_t inverted_index_read_bytes = 0;
    int64_t inverted_index_range_read_count = 0;
    int64_t inverted_index_serial_read_rounds = 0;

    std::array<int64_t, SNII_SECTION_COUNT> inverted_index_snii_section_read_bytes {};
    std::array<int64_t, SNII_SECTION_COUNT>
            inverted_index_snii_section_remote_physical_read_bytes {};
    std::array<int64_t, SNII_SECTION_COUNT> inverted_index_snii_section_bytes_write_into_cache {};
    std::array<int64_t, SNII_SECTION_COUNT> inverted_index_snii_section_file_cache_blocks_total {};
    std::array<int64_t, SNII_SECTION_COUNT> inverted_index_snii_section_file_cache_blocks_hit {};
    std::array<int64_t, SNII_SECTION_COUNT> inverted_index_snii_section_file_cache_blocks_miss {};
};

struct IOContext {
    ReaderType reader_type = ReaderType::UNKNOWN;
    // FIXME(plat1ko): Seems `is_disposable` can be inferred from the `reader_type`?
    bool is_disposable = false;
    bool is_index_data = false;
    bool read_file_cache = true;
    // TODO(lightman): use following member variables to control file cache
    bool is_persistent = false;
    // stop reader when reading, used in some interrupted operations
    bool should_stop = false;
    int64_t expiration_time = 0;
    const TUniqueId* query_id = nullptr;             // Ref
    FileCacheStatistics* file_cache_stats = nullptr; // Ref
    FileReaderStats* file_reader_stats = nullptr;    // Ref
    bool is_inverted_index = false;
    uint8_t snii_section_type = SNII_SECTION_UNKNOWN;
    // if is_dryrun, read IO will download data to cache but return no data to reader
    // useful to skip cache data read from local disk to accelarate warm up
    bool is_dryrun = false;
    // if `is_warmup` == true, this I/O request is from a warm up task
    bool is_warmup {false};
    int64_t condition_cache_filtered_rows = 0;
};

} // namespace io
} // namespace doris
