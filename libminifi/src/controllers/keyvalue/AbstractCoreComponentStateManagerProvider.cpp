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

#include "controllers/keyvalue/AbstractCoreComponentStateManagerProvider.h"

#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::AbstractCoreComponentStateManager(
    std::shared_ptr<AbstractCoreComponentStateManagerProvider> provider,
    const std::string& id)
    : provider_(std::move(provider))
    , id_(id) {
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::set(const std::unordered_map<std::string, std::string>& kvs) {
  return provider_->setImpl(id_, kvs);
}

std::pair<int64_t /*version*/, std::unordered_map<std::string, std::string>> AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::get() {
  return provider_->getImpl(id_);
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::clear() {
  return provider_->clearImpl(id_);
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::persist() {
  return provider_->persistImpl(id_);
}

bool AbstractCoreComponentStateManagerProvider::AbstractCoreComponentStateManager::load() {
  return provider_->loadImpl(id_);
}

AbstractCoreComponentStateManagerProvider::~AbstractCoreComponentStateManagerProvider() {
}

std::shared_ptr<core::CoreComponentStateManager> AbstractCoreComponentStateManagerProvider::getCoreComponentStateManager(const core::CoreComponent& component) {
  return std::make_shared<AbstractCoreComponentStateManager>(shared_from_this(), component.getUUIDStr());
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
