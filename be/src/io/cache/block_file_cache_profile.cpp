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

#include "io/cache/block_file_cache_profile.h"

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "common/metrics/doris_metrics.h"

namespace doris::io {

namespace {

constexpr std::array<const char*, SNII_SECTION_COUNT> kSniiSectionNames {
        "Unknown", "Meta", "Dict", "Posting", "Bsbf", "Norms", "NullBitmap"};

RuntimeProfile::Counter* add_snii_section_counter(RuntimeProfile* profile, const char* section_name,
                                                  const char* metric_name, TUnit::type unit,
                                                  const char* parent) {
    std::string counter_name = "InvertedIndexSnii";
    counter_name += section_name;
    counter_name += metric_name;
    return ADD_CHILD_COUNTER_WITH_LEVEL(profile, counter_name, unit, parent, 1);
}

} // namespace

std::shared_ptr<AtomicStatistics> FileCacheMetrics::report() {
    std::shared_ptr<AtomicStatistics> output_stats = std::make_shared<AtomicStatistics>();
    std::lock_guard lock(_mtx);
    output_stats->num_io_bytes_read_from_cache += _statistics->num_io_bytes_read_from_cache;
    output_stats->num_io_bytes_read_from_remote += _statistics->num_io_bytes_read_from_remote;
    output_stats->num_io_bytes_read_from_peer += _statistics->num_io_bytes_read_from_peer;
    return output_stats;
}

void FileCacheMetrics::update(FileCacheStatistics* input_stats) {
    if (_statistics == nullptr) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_statistics == nullptr) {
            _statistics = std::make_shared<AtomicStatistics>();
            register_entity();
        }
    }
    _statistics->num_io_bytes_read_from_cache += input_stats->bytes_read_from_local;
    _statistics->num_io_bytes_read_from_remote += input_stats->bytes_read_from_remote;
    _statistics->num_io_bytes_read_from_peer += input_stats->bytes_read_from_peer;
}

void FileCacheMetrics::register_entity() {
    DorisMetrics::instance()->server_entity()->register_hook(
            "block_file_cache", [this]() { update_metrics_callback(); });
}

void FileCacheMetrics::update_metrics_callback() {
    std::shared_ptr<AtomicStatistics> stats = report();
    DorisMetrics::instance()->num_io_bytes_read_from_cache->set_value(
            stats->num_io_bytes_read_from_cache);
    DorisMetrics::instance()->num_io_bytes_read_from_remote->set_value(
            stats->num_io_bytes_read_from_remote);
    DorisMetrics::instance()->num_io_bytes_read_from_peer->set_value(
            stats->num_io_bytes_read_from_peer);
    DorisMetrics::instance()->num_io_bytes_read_total->set_value(
            stats->num_io_bytes_read_from_cache + stats->num_io_bytes_read_from_remote +
            stats->num_io_bytes_read_from_peer);
}

FileCacheStatistics diff_file_cache_statistics(const FileCacheStatistics& current,
                                               const FileCacheStatistics& previous) {
    FileCacheStatistics diff;
#define SUBTRACT_FIELD(field) diff.field = current.field - previous.field
    SUBTRACT_FIELD(num_local_io_total);
    SUBTRACT_FIELD(num_remote_io_total);
    SUBTRACT_FIELD(num_peer_io_total);
    SUBTRACT_FIELD(local_io_timer);
    SUBTRACT_FIELD(bytes_read_from_local);
    SUBTRACT_FIELD(bytes_read_from_remote);
    SUBTRACT_FIELD(bytes_read_from_peer);
    SUBTRACT_FIELD(remote_physical_read_count);
    SUBTRACT_FIELD(remote_physical_read_bytes);
    SUBTRACT_FIELD(peer_physical_read_count);
    SUBTRACT_FIELD(peer_physical_read_bytes);
    SUBTRACT_FIELD(remote_io_timer);
    SUBTRACT_FIELD(peer_io_timer);
    SUBTRACT_FIELD(remote_wait_timer);
    SUBTRACT_FIELD(write_cache_io_timer);
    SUBTRACT_FIELD(bytes_write_into_cache);
    SUBTRACT_FIELD(file_cache_blocks_total);
    SUBTRACT_FIELD(file_cache_blocks_hit);
    SUBTRACT_FIELD(file_cache_blocks_miss);
    SUBTRACT_FIELD(file_cache_blocks_skip);
    SUBTRACT_FIELD(file_cache_blocks_downloading);
    SUBTRACT_FIELD(num_skip_cache_io_total);
    SUBTRACT_FIELD(read_cache_file_directly_timer);
    SUBTRACT_FIELD(cache_get_or_set_timer);
    SUBTRACT_FIELD(lock_wait_timer);
    SUBTRACT_FIELD(get_timer);
    SUBTRACT_FIELD(set_timer);

    SUBTRACT_FIELD(inverted_index_num_local_io_total);
    SUBTRACT_FIELD(inverted_index_num_remote_io_total);
    SUBTRACT_FIELD(inverted_index_num_peer_io_total);
    SUBTRACT_FIELD(inverted_index_bytes_read_from_local);
    SUBTRACT_FIELD(inverted_index_bytes_read_from_remote);
    SUBTRACT_FIELD(inverted_index_bytes_read_from_peer);
    SUBTRACT_FIELD(inverted_index_remote_physical_read_count);
    SUBTRACT_FIELD(inverted_index_remote_physical_read_bytes);
    SUBTRACT_FIELD(inverted_index_peer_physical_read_count);
    SUBTRACT_FIELD(inverted_index_peer_physical_read_bytes);
    SUBTRACT_FIELD(inverted_index_bytes_write_into_cache);
    SUBTRACT_FIELD(inverted_index_file_cache_blocks_total);
    SUBTRACT_FIELD(inverted_index_file_cache_blocks_hit);
    SUBTRACT_FIELD(inverted_index_file_cache_blocks_miss);
    SUBTRACT_FIELD(inverted_index_file_cache_blocks_skip);
    SUBTRACT_FIELD(inverted_index_file_cache_blocks_downloading);
    SUBTRACT_FIELD(inverted_index_local_io_timer);
    SUBTRACT_FIELD(inverted_index_remote_io_timer);
    SUBTRACT_FIELD(inverted_index_peer_io_timer);
    SUBTRACT_FIELD(inverted_index_io_timer);
    SUBTRACT_FIELD(inverted_index_request_bytes);
    SUBTRACT_FIELD(inverted_index_read_bytes);
    SUBTRACT_FIELD(inverted_index_range_read_count);
    SUBTRACT_FIELD(inverted_index_serial_read_rounds);
#undef SUBTRACT_FIELD
    for (size_t i = 0; i < SNII_SECTION_COUNT; ++i) {
        diff.inverted_index_snii_section_read_bytes[i] =
                current.inverted_index_snii_section_read_bytes[i] -
                previous.inverted_index_snii_section_read_bytes[i];
        diff.inverted_index_snii_section_remote_physical_read_bytes[i] =
                current.inverted_index_snii_section_remote_physical_read_bytes[i] -
                previous.inverted_index_snii_section_remote_physical_read_bytes[i];
        diff.inverted_index_snii_section_bytes_write_into_cache[i] =
                current.inverted_index_snii_section_bytes_write_into_cache[i] -
                previous.inverted_index_snii_section_bytes_write_into_cache[i];
        diff.inverted_index_snii_section_file_cache_blocks_total[i] =
                current.inverted_index_snii_section_file_cache_blocks_total[i] -
                previous.inverted_index_snii_section_file_cache_blocks_total[i];
        diff.inverted_index_snii_section_file_cache_blocks_hit[i] =
                current.inverted_index_snii_section_file_cache_blocks_hit[i] -
                previous.inverted_index_snii_section_file_cache_blocks_hit[i];
        diff.inverted_index_snii_section_file_cache_blocks_miss[i] =
                current.inverted_index_snii_section_file_cache_blocks_miss[i] -
                previous.inverted_index_snii_section_file_cache_blocks_miss[i];
    }
    return diff;
}

FileCacheProfileReporter::FileCacheProfileReporter(RuntimeProfile* profile) {
    static const char* cache_profile = "FileCache";
    ADD_TIMER_WITH_LEVEL(profile, cache_profile, 2);
    num_local_io_total =
            ADD_CHILD_COUNTER_WITH_LEVEL(profile, "NumLocalIOTotal", TUnit::UNIT, cache_profile, 1);
    num_remote_io_total = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "NumRemoteIOTotal", TUnit::UNIT,
                                                       cache_profile, 1);
    num_peer_io_total =
            ADD_CHILD_COUNTER_WITH_LEVEL(profile, "NumPeerIOTotal", TUnit::UNIT, cache_profile, 1);
    local_io_timer = ADD_CHILD_TIMER_WITH_LEVEL(profile, "LocalIOUseTimer", cache_profile, 1);
    remote_io_timer = ADD_CHILD_TIMER_WITH_LEVEL(profile, "RemoteIOUseTimer", cache_profile, 1);
    peer_io_timer = ADD_CHILD_TIMER_WITH_LEVEL(profile, "PeerIOUseTimer", cache_profile, 1);
    remote_wait_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "WaitOtherDownloaderTimer", cache_profile, 1);
    write_cache_io_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "WriteCacheIOUseTimer", cache_profile, 1);
    bytes_write_into_cache = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "BytesWriteIntoCache",
                                                          TUnit::BYTES, cache_profile, 1);
    num_skip_cache_io_total = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "NumSkipCacheIOTotal",
                                                           TUnit::UNIT, cache_profile, 1);
    bytes_scanned_from_cache = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "BytesScannedFromCache",
                                                            TUnit::BYTES, cache_profile, 1);
    bytes_scanned_from_remote = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "BytesScannedFromRemote",
                                                             TUnit::BYTES, cache_profile, 1);
    bytes_scanned_from_peer = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "BytesScannedFromPeer",
                                                           TUnit::BYTES, cache_profile, 1);
    remote_physical_read_bytes = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "RemotePhysicalReadBytes",
                                                              TUnit::BYTES, cache_profile, 1);
    remote_physical_read_count = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "RemotePhysicalReadCount",
                                                              TUnit::UNIT, cache_profile, 1);
    peer_physical_read_bytes = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "PeerPhysicalReadBytes",
                                                            TUnit::BYTES, cache_profile, 1);
    peer_physical_read_count = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "PeerPhysicalReadCount",
                                                            TUnit::UNIT, cache_profile, 1);
    file_cache_blocks_total = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "FileCacheBlocksTotal",
                                                           TUnit::UNIT, cache_profile, 1);
    file_cache_blocks_hit = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "FileCacheBlocksHit", TUnit::UNIT,
                                                         cache_profile, 1);
    file_cache_blocks_miss = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "FileCacheBlocksMiss",
                                                          TUnit::UNIT, cache_profile, 1);
    file_cache_blocks_skip = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "FileCacheBlocksSkip",
                                                          TUnit::UNIT, cache_profile, 1);
    file_cache_blocks_downloading = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "FileCacheBlocksDownloading", TUnit::UNIT, cache_profile, 1);
    read_cache_file_directly_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "ReadCacheFileDirectlyTimer", cache_profile, 1);
    cache_get_or_set_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "CacheGetOrSetTimer", cache_profile, 1);
    lock_wait_timer = ADD_CHILD_TIMER_WITH_LEVEL(profile, "LockWaitTimer", cache_profile, 1);
    get_timer = ADD_CHILD_TIMER_WITH_LEVEL(profile, "GetTimer", cache_profile, 1);
    set_timer = ADD_CHILD_TIMER_WITH_LEVEL(profile, "SetTimer", cache_profile, 1);

    inverted_index_num_local_io_total = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexNumLocalIOTotal", TUnit::UNIT, cache_profile, 1);
    inverted_index_num_remote_io_total = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexNumRemoteIOTotal", TUnit::UNIT, cache_profile, 1);
    inverted_index_num_peer_io_total = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexNumPeerIOTotal", TUnit::UNIT, cache_profile, 1);
    inverted_index_bytes_scanned_from_cache = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexBytesScannedFromCache", TUnit::BYTES, cache_profile, 1);
    inverted_index_bytes_scanned_from_remote = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexBytesScannedFromRemote", TUnit::BYTES, cache_profile, 1);
    inverted_index_bytes_scanned_from_peer = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexBytesScannedFromPeer", TUnit::BYTES, cache_profile, 1);
    inverted_index_remote_physical_read_bytes = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexRemotePhysicalReadBytes", TUnit::BYTES, cache_profile, 1);
    inverted_index_remote_physical_read_count = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexRemotePhysicalReadCount", TUnit::UNIT, cache_profile, 1);
    inverted_index_peer_physical_read_bytes = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexPeerPhysicalReadBytes", TUnit::BYTES, cache_profile, 1);
    inverted_index_peer_physical_read_count = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexPeerPhysicalReadCount", TUnit::UNIT, cache_profile, 1);
    inverted_index_bytes_write_into_cache = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexBytesWriteIntoCache", TUnit::BYTES, cache_profile, 1);
    inverted_index_file_cache_blocks_total = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexFileCacheBlocksTotal", TUnit::UNIT, cache_profile, 1);
    inverted_index_file_cache_blocks_hit = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexFileCacheBlocksHit", TUnit::UNIT, cache_profile, 1);
    inverted_index_file_cache_blocks_miss = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexFileCacheBlocksMiss", TUnit::UNIT, cache_profile, 1);
    inverted_index_file_cache_blocks_skip = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexFileCacheBlocksSkip", TUnit::UNIT, cache_profile, 1);
    inverted_index_file_cache_blocks_downloading = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexFileCacheBlocksDownloading", TUnit::UNIT, cache_profile, 1);
    inverted_index_local_io_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "InvertedIndexLocalIOUseTimer", cache_profile, 1);
    inverted_index_remote_io_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "InvertedIndexRemoteIOUseTimer", cache_profile, 1);
    inverted_index_peer_io_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "InvertedIndexPeerIOUseTimer", cache_profile, 1);
    inverted_index_io_timer =
            ADD_CHILD_TIMER_WITH_LEVEL(profile, "InvertedIndexIOTimer", cache_profile, 1);
    inverted_index_request_bytes = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexRequestBytes", TUnit::BYTES, cache_profile, 1);
    inverted_index_read_bytes = ADD_CHILD_COUNTER_WITH_LEVEL(profile, "InvertedIndexReadBytes",
                                                             TUnit::BYTES, cache_profile, 1);
    inverted_index_range_read_count = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexRangeReadCount", TUnit::UNIT, cache_profile, 1);
    inverted_index_serial_read_rounds = ADD_CHILD_COUNTER_WITH_LEVEL(
            profile, "InvertedIndexSerialReadRounds", TUnit::UNIT, cache_profile, 1);
    for (size_t i = 0; i < SNII_SECTION_COUNT; ++i) {
        inverted_index_snii_section_read_bytes[i] = add_snii_section_counter(
                profile, kSniiSectionNames[i], "ReadBytes", TUnit::BYTES, cache_profile);
        inverted_index_snii_section_remote_physical_read_bytes[i] =
                add_snii_section_counter(profile, kSniiSectionNames[i], "RemotePhysicalReadBytes",
                                         TUnit::BYTES, cache_profile);
        inverted_index_snii_section_bytes_write_into_cache[i] = add_snii_section_counter(
                profile, kSniiSectionNames[i], "BytesWriteIntoCache", TUnit::BYTES, cache_profile);
        inverted_index_snii_section_file_cache_blocks_total[i] = add_snii_section_counter(
                profile, kSniiSectionNames[i], "FileCacheBlocksTotal", TUnit::UNIT, cache_profile);
        inverted_index_snii_section_file_cache_blocks_hit[i] = add_snii_section_counter(
                profile, kSniiSectionNames[i], "FileCacheBlocksHit", TUnit::UNIT, cache_profile);
        inverted_index_snii_section_file_cache_blocks_miss[i] = add_snii_section_counter(
                profile, kSniiSectionNames[i], "FileCacheBlocksMiss", TUnit::UNIT, cache_profile);
    }
}

void FileCacheProfileReporter::update(const FileCacheStatistics* statistics) const {
    COUNTER_UPDATE(num_local_io_total, statistics->num_local_io_total);
    COUNTER_UPDATE(num_remote_io_total, statistics->num_remote_io_total);
    COUNTER_UPDATE(num_peer_io_total, statistics->num_peer_io_total);
    COUNTER_UPDATE(local_io_timer, statistics->local_io_timer);
    COUNTER_UPDATE(remote_io_timer, statistics->remote_io_timer);
    COUNTER_UPDATE(peer_io_timer, statistics->peer_io_timer);
    COUNTER_UPDATE(remote_wait_timer, statistics->remote_wait_timer);
    COUNTER_UPDATE(write_cache_io_timer, statistics->write_cache_io_timer);
    COUNTER_UPDATE(bytes_write_into_cache, statistics->bytes_write_into_cache);
    COUNTER_UPDATE(num_skip_cache_io_total, statistics->num_skip_cache_io_total);
    COUNTER_UPDATE(bytes_scanned_from_cache, statistics->bytes_read_from_local);
    COUNTER_UPDATE(bytes_scanned_from_remote, statistics->bytes_read_from_remote);
    COUNTER_UPDATE(bytes_scanned_from_peer, statistics->bytes_read_from_peer);
    COUNTER_UPDATE(remote_physical_read_bytes, statistics->remote_physical_read_bytes);
    COUNTER_UPDATE(remote_physical_read_count, statistics->remote_physical_read_count);
    COUNTER_UPDATE(peer_physical_read_bytes, statistics->peer_physical_read_bytes);
    COUNTER_UPDATE(peer_physical_read_count, statistics->peer_physical_read_count);
    COUNTER_UPDATE(file_cache_blocks_total, statistics->file_cache_blocks_total);
    COUNTER_UPDATE(file_cache_blocks_hit, statistics->file_cache_blocks_hit);
    COUNTER_UPDATE(file_cache_blocks_miss, statistics->file_cache_blocks_miss);
    COUNTER_UPDATE(file_cache_blocks_skip, statistics->file_cache_blocks_skip);
    COUNTER_UPDATE(file_cache_blocks_downloading, statistics->file_cache_blocks_downloading);
    COUNTER_UPDATE(read_cache_file_directly_timer, statistics->read_cache_file_directly_timer);
    COUNTER_UPDATE(cache_get_or_set_timer, statistics->cache_get_or_set_timer);
    COUNTER_UPDATE(lock_wait_timer, statistics->lock_wait_timer);
    COUNTER_UPDATE(get_timer, statistics->get_timer);
    COUNTER_UPDATE(set_timer, statistics->set_timer);

    COUNTER_UPDATE(inverted_index_num_local_io_total,
                   statistics->inverted_index_num_local_io_total);
    COUNTER_UPDATE(inverted_index_num_remote_io_total,
                   statistics->inverted_index_num_remote_io_total);
    COUNTER_UPDATE(inverted_index_num_peer_io_total, statistics->inverted_index_num_peer_io_total);
    COUNTER_UPDATE(inverted_index_bytes_scanned_from_cache,
                   statistics->inverted_index_bytes_read_from_local);
    COUNTER_UPDATE(inverted_index_bytes_scanned_from_remote,
                   statistics->inverted_index_bytes_read_from_remote);
    COUNTER_UPDATE(inverted_index_bytes_scanned_from_peer,
                   statistics->inverted_index_bytes_read_from_peer);
    COUNTER_UPDATE(inverted_index_remote_physical_read_bytes,
                   statistics->inverted_index_remote_physical_read_bytes);
    COUNTER_UPDATE(inverted_index_remote_physical_read_count,
                   statistics->inverted_index_remote_physical_read_count);
    COUNTER_UPDATE(inverted_index_peer_physical_read_bytes,
                   statistics->inverted_index_peer_physical_read_bytes);
    COUNTER_UPDATE(inverted_index_peer_physical_read_count,
                   statistics->inverted_index_peer_physical_read_count);
    COUNTER_UPDATE(inverted_index_bytes_write_into_cache,
                   statistics->inverted_index_bytes_write_into_cache);
    COUNTER_UPDATE(inverted_index_file_cache_blocks_total,
                   statistics->inverted_index_file_cache_blocks_total);
    COUNTER_UPDATE(inverted_index_file_cache_blocks_hit,
                   statistics->inverted_index_file_cache_blocks_hit);
    COUNTER_UPDATE(inverted_index_file_cache_blocks_miss,
                   statistics->inverted_index_file_cache_blocks_miss);
    COUNTER_UPDATE(inverted_index_file_cache_blocks_skip,
                   statistics->inverted_index_file_cache_blocks_skip);
    COUNTER_UPDATE(inverted_index_file_cache_blocks_downloading,
                   statistics->inverted_index_file_cache_blocks_downloading);
    COUNTER_UPDATE(inverted_index_local_io_timer, statistics->inverted_index_local_io_timer);
    COUNTER_UPDATE(inverted_index_remote_io_timer, statistics->inverted_index_remote_io_timer);
    COUNTER_UPDATE(inverted_index_peer_io_timer, statistics->inverted_index_peer_io_timer);
    COUNTER_UPDATE(inverted_index_io_timer, statistics->inverted_index_io_timer);
    COUNTER_UPDATE(inverted_index_request_bytes, statistics->inverted_index_request_bytes);
    COUNTER_UPDATE(inverted_index_read_bytes, statistics->inverted_index_read_bytes);
    COUNTER_UPDATE(inverted_index_range_read_count, statistics->inverted_index_range_read_count);
    COUNTER_UPDATE(inverted_index_serial_read_rounds,
                   statistics->inverted_index_serial_read_rounds);
    for (size_t i = 0; i < SNII_SECTION_COUNT; ++i) {
        COUNTER_UPDATE(inverted_index_snii_section_read_bytes[i],
                       statistics->inverted_index_snii_section_read_bytes[i]);
        COUNTER_UPDATE(inverted_index_snii_section_remote_physical_read_bytes[i],
                       statistics->inverted_index_snii_section_remote_physical_read_bytes[i]);
        COUNTER_UPDATE(inverted_index_snii_section_bytes_write_into_cache[i],
                       statistics->inverted_index_snii_section_bytes_write_into_cache[i]);
        COUNTER_UPDATE(inverted_index_snii_section_file_cache_blocks_total[i],
                       statistics->inverted_index_snii_section_file_cache_blocks_total[i]);
        COUNTER_UPDATE(inverted_index_snii_section_file_cache_blocks_hit[i],
                       statistics->inverted_index_snii_section_file_cache_blocks_hit[i]);
        COUNTER_UPDATE(inverted_index_snii_section_file_cache_blocks_miss[i],
                       statistics->inverted_index_snii_section_file_cache_blocks_miss[i]);
    }
}

} // namespace doris::io
