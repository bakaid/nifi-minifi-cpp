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
#ifndef LIBMINIFI_INCLUDE_KEYVALUE_AbstractCoreComponentStateManagerProvider_H_
#define LIBMINIFI_INCLUDE_KEYVALUE_AbstractCoreComponentStateManagerProvider_H_

#include "core/Core.h"
#include "core/CoreComponentState.h"

#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

 class AbstractCoreComponentStateManagerProvider : public std::enable_shared_from_this<AbstractCoreComponentStateManagerProvider>,
                                                   public core::CoreComponentStateManagerProvider {
 public:
  virtual ~AbstractCoreComponentStateManagerProvider();

  virtual std::shared_ptr<core::CoreComponentStateManager> getCoreComponentStateManager(const core::CoreComponent& component) override;

  class AbstractCoreComponentStateManager : public core::CoreComponentStateManager{
   private:
    std::shared_ptr<AbstractCoreComponentStateManagerProvider> provider_;
    std::string id_;

   public:
    AbstractCoreComponentStateManager(std::shared_ptr<AbstractCoreComponentStateManagerProvider> provider, const std::string& id);

    virtual bool set(const std::unordered_map<std::string, std::string>& kvs) override;

    virtual std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>> get() override;

    virtual bool clear() override;

    virtual bool persist() override;

    virtual bool load() override;
 };

 protected:
  virtual bool setImpl(std::string& id, const std::unordered_map<std::string, std::string>& kvs) = 0;

  virtual std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>> getImpl(std::string& id) = 0;

  virtual bool clearImpl(std::string& id) = 0;

  virtual bool persistImpl(std::string& id) = 0;

  virtual bool loadImpl(std::string& id) = 0;
};

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_KEYVALUE_AbstractCoreComponentStateManagerProvider_H_ */
