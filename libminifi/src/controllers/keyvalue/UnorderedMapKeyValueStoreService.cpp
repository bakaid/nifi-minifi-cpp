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

void UnorderedMapKeyValueStoreService::set(const std::string& id, const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  maps_[id][key] = value;
}

void UnorderedMapKeyValueStoreService::set(const std::string& id, const std::unordered_map<std::string, std::string>& kvs) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& id_map = maps_[id];
  for (const auto& kv : kvs) {
    id_map[kv.first] = kv.second;
  }
}

std::unordered_map<std::string, std::string> UnorderedMapKeyValueStoreService::get(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = maps_.find(id);
  if (it != maps_.end()) {
    return it->second;
  } else {
    return {};
  }
}

std::string UnorderedMapKeyValueStoreService::get(const std::string& id, const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = maps_.find(id);
  if (it != maps_.end()) {
    auto& map = it->second;
    auto it2 = map.find(key);
    if (it2 != map.end()) {
      return it2->second;
    } else {
      return "";
    }
  } else {
    return "";
  }
}

bool UnorderedMapKeyValueStoreService::remove(const std::string& id, const std::string& key) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = maps_.find(id);
  if (it != maps_.end()) {
    return it->second.erase(key) != 0U;
  } else {
    return false;
  }
}

void UnorderedMapKeyValueStoreService::remove(const std::string& id, const std::unordered_set<std::string>& keys) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = maps_.find(id);
  if (it != maps_.end()) {
    for (const auto& key : keys) {
      it->second.erase(key);
    }
  }
}

void UnorderedMapKeyValueStoreService::clear(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  maps_.erase(id);
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
