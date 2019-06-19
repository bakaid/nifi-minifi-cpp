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
#ifndef LIBMINIFI_INCLUDE_KEYVALUE_UnorderedMapPersistableKeyValueStoreService_H_
#define LIBMINIFI_INCLUDE_KEYVALUE_UnorderedMapPersistableKeyValueStoreService_H_

#include "AbstractAutoPersistingKeyValueStoreService.h"
#include "UnorderedMapKeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Resource.h"

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

class UnorderedMapPersistableKeyValueStoreService : public AbstractAutoPersistingKeyValueStoreService,
                                                    public UnorderedMapKeyValueStoreService {
 public:
  explicit UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::string& id);
  explicit UnorderedMapPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());
  explicit UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure>& configuration);

  virtual ~UnorderedMapPersistableKeyValueStoreService();

  static core::Property File;

  virtual void onEnable() override;
  virtual void initialize() override;

  virtual bool persist() override;

 protected:
  static constexpr const char* FORMAT_VERSION_KEY = "__UnorderedMapPersistableKeyValueStoreService_FormatVersion";
  static constexpr int FORMAT_VERSION = 1;

  std::string file_;

  bool load();

  std::string escape(const std::string& str);
  std::string unescape(const std::string& str);

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(UnorderedMapPersistableKeyValueStoreService, "TODO comment: UnorderedMapPersistableKeyValueStoreService");

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_KEYVALUE_UnorderedMapPersistableKeyValueStoreService_H_ */
