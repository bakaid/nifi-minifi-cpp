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

#include "controllers/keyvalue/PersistableKeyValueStoreService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {
PersistableKeyValueStoreService::PersistableKeyValueStoreService(const std::string& name, const std::string& id)
    : KeyValueStoreService(name, id) {
}

PersistableKeyValueStoreService::PersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid /*= utils::Identifier()*/)
    : KeyValueStoreService(name, uuid) {
}

PersistableKeyValueStoreService::~PersistableKeyValueStoreService() {
}

bool PersistableKeyValueStoreService::setImpl(std::string& id, const std::unordered_map<std::string, std::string>& kvs) {
  return set(id, -1 /*expected_version*/, kvs, nullptr /*new_version*/);
}

std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>> PersistableKeyValueStoreService::getImpl(std::string& id) {
  return get(id);
}

bool PersistableKeyValueStoreService::clearImpl(std::string& id) {
  return clear(id, -1 /*expected_version*/);
}

bool PersistableKeyValueStoreService::persistImpl(std::string& id) {
  return persist(id);
}

bool PersistableKeyValueStoreService::loadImpl(std::string& id) {
  return load(id);
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
