/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "controllers/keyvalue/UnorderedMapKeyValueStoreService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

UnorderedMapKeyValueStoreService::UnorderedMapKeyValueStoreService(const std::string& name, const std::string& id)
    : KeyValueStoreService(name, id)
    , logger_(logging::LoggerFactory<UnorderedMapKeyValueStoreService>::getLogger()) {
}

UnorderedMapKeyValueStoreService::UnorderedMapKeyValueStoreService(const std::string& name, utils::Identifier uuid /*= utils::Identifier()*/)
    : KeyValueStoreService(name, uuid)
    , logger_(logging::LoggerFactory<UnorderedMapKeyValueStoreService>::getLogger()) {
}

UnorderedMapKeyValueStoreService::UnorderedMapKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure> &configuration)
    : KeyValueStoreService(name)
    , logger_(logging::LoggerFactory<UnorderedMapKeyValueStoreService>::getLogger())  {
  setConfiguration(configuration);
  initialize();
}

UnorderedMapKeyValueStoreService::~UnorderedMapKeyValueStoreService() {
}

bool UnorderedMapKeyValueStoreService::set(const std::string& id, int64_t expected_version, const std::unordered_map<std::string, std::string>& kvs, int64_t* new_version) {
  if (new_version != nullptr) {
    *new_version = -1;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  std::unordered_map<std::string, std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>>>::iterator it;
  bool inserted = false;
  std::tie(it, inserted) = maps_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(id),
      std::forward_as_tuple(-1, std::unordered_map<std::string, std::string>()));
  if (!inserted && expected_version != -1) {
    if (it->second.first != expected_version) {
      if (new_version != nullptr) {
        *new_version = it->second.first;
      }
      return false;
    }
  }
  it->second.second = kvs;
  it->second.first++;
  if (new_version != nullptr) {
    *new_version = it->second.first;
  }
  return true;
}

std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>> UnorderedMapKeyValueStoreService::get(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = maps_.find(id);
  if (it == maps_.end()) {
    return {-1, std::unordered_map<std::string, std::string>()};
  } else {
    return it->second;
  }
}

bool UnorderedMapKeyValueStoreService::clear(const std::string& id, int64_t expected_version) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = maps_.find(id);
  if (it == maps_.end()) {
    if (expected_version != -1) {
      return false;
    } else {
      return true;
    }
  } else {
    if (it->second.first != expected_version) {
      return false;
    }
    maps_.erase(it);
    return true;
  }
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
