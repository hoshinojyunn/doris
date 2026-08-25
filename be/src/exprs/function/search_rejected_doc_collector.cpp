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

#include "exprs/function/search_rejected_doc_collector.h"

#include <fmt/format.h>
#include <glog/logging.h>

#include <roaring/roaring.hh>

#include "storage/index/index_iterator.h"
#include "storage/index/inverted/inverted_index_cache.h"

namespace doris {

SearchRejectedDocCollector::SearchRejectedDocCollector(
        uint32_t num_rows, const std::shared_ptr<const roaring::Roaring>& excluded_docs)
        : _num_rows(num_rows), _excluded_docs(std::make_shared<roaring::Roaring>()) {
    if (excluded_docs != nullptr) {
        *_excluded_docs |= *excluded_docs;
    }
}

Status SearchRejectedDocCollector::add_referenced_field(
        const std::string& logical_field,
        const std::unordered_map<std::string, segment_v2::IndexIterator*>& iterators) {
    if (_reject_all || !_processed_fields.insert(logical_field).second) {
        return Status::OK();
    }

    const auto iterator_it = iterators.find(logical_field);
    if (iterator_it == iterators.end() || iterator_it->second == nullptr) {
        // FieldReaderResolver owns the pre-existing missing-field and missing-iterator errors.
        return Status::OK();
    }
    auto* iterator = iterator_it->second;

    auto has_null = iterator->has_null();
    if (!has_null.has_value()) {
        return has_null.error();
    }
    if (!has_null.value()) {
        return Status::OK();
    }

    segment_v2::InvertedIndexQueryCacheHandle cache_handle;
    auto status = iterator->read_null_bitmap(&cache_handle);
    if (!status.ok()) {
        return reject_all(fmt::format("failed to read null bitmap for field '{}': {}",
                                      logical_field, status.to_string()));
    }

    auto null_bitmap = cache_handle.get_bitmap();
    if (null_bitmap == nullptr) {
        return reject_all(fmt::format("null bitmap is unavailable for field '{}'", logical_field));
    }
    *_excluded_docs |= *null_bitmap;
    return Status::OK();
}

Status SearchRejectedDocCollector::reject_all(std::string_view reason) {
    if (_reject_all) {
        return Status::OK();
    }
    LOG(WARNING) << "search: rejecting all " << _num_rows
                 << " documents in this segment: " << reason;
    _reject_all = true;
    _excluded_docs->addRange(0, _num_rows);
    return Status::OK();
}

bool SearchRejectedDocCollector::is_reject_all() const {
    return _reject_all;
}

std::shared_ptr<const roaring::Roaring> SearchRejectedDocCollector::excluded_docs() const {
    return _excluded_docs;
}

} // namespace doris
