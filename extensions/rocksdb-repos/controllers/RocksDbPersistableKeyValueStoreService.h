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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_UnorderedMapPersistableKeyValueStoreService_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_UnorderedMapPersistableKeyValueStoreService_H_

#include "controllers/keyvalue/PersistableKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class RocksDbPersistableKeyValueStoreService : public PersistableKeyValueStoreService {
 public:
  explicit RocksDbPersistableKeyValueStoreService(const std::string& name, const std::string& id);
  explicit RocksDbPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());
  explicit RocksDbPersistableKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure>& configuration);

  virtual ~RocksDbPersistableKeyValueStoreService();

  static core::Property Directory;

  virtual void onEnable() override;
  virtual void initialize() override;

  virtual bool set(const std::string& id, int64_t expected_version, const std::unordered_map<std::string, std::string>& kvs, int64_t* new_version) override;

  virtual std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>> get(const std::string& id) override;

  virtual bool clear(const std::string& id, int64_t expected_version) override;

  virtual bool persist(const std::string& id) override;
  virtual bool persist() override;

  virtual bool load(const std::string& id) override;
  virtual bool load() override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_UnorderedMapPersistableKeyValueStoreService_H_ */
