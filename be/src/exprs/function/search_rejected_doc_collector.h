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

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "common/status.h"

namespace roaring {
class Roaring;
}

namespace doris::segment_v2 {
class IndexIterator;
} // namespace doris::segment_v2

namespace doris {

class SearchRejectedDocCollector {
public:
    explicit SearchRejectedDocCollector(
            uint32_t num_rows,
            const std::shared_ptr<const roaring::Roaring>& excluded_docs = nullptr);

    Status add_referenced_field(
            const std::string& logical_field,
            const std::unordered_map<std::string, segment_v2::IndexIterator*>& iterators);
    Status reject_all(std::string_view reason);

    bool is_reject_all() const;
    std::shared_ptr<const roaring::Roaring> excluded_docs() const;

private:
    uint32_t _num_rows;
    bool _reject_all = false;
    std::shared_ptr<roaring::Roaring> _excluded_docs;
    std::unordered_set<std::string> _processed_fields;
};

} // namespace doris
