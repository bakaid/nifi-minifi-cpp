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
#ifndef LIBMINIFI_INCLUDE_KEYVALUE_PersistableKeyValueStoreService_H_
#define LIBMINIFI_INCLUDE_KEYVALUE_PersistableKeyValueStoreService_H_

#include "keyvalue/KeyValueStoreService.h"
#include "core/Core.h"
#include "properties/Configure.h"

#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace keyvalue {

class PersistableKeyValueStoreService : virtual public KeyValueStoreService {
 public:
  explicit PersistableKeyValueStoreService(const std::string& name, const std::string& id);
  explicit PersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid = utils::Identifier());

  virtual ~PersistableKeyValueStoreService();

  virtual bool persist(const std::string& id) = 0;
  virtual bool persist() = 0;

  virtual bool load(const std::string& id) = 0;
  virtual bool load() = 0;
};

} /* namespace keyvalue */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_KEYVALUE_PersistableKeyValueStoreService_H_ */
