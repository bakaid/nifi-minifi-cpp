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

#include "controllers/keyvalue/UnorderedMapPersistableKeyValueStoreService.h"

#include "utils/file/FileUtils.h"

#include <fstream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

core::Property UnorderedMapPersistableKeyValueStoreService::Directory(
    core::PropertyBuilder::createProperty("Directory")->withDescription("Path to a directory to store data")
    ->isRequired(true)->build());

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::string& id)
    : KeyValueStoreService(name, id)
    , PersistableKeyValueStoreService(name, id)
    , UnorderedMapKeyValueStoreService(name, id)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger()) {
}

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid /*= utils::Identifier()*/)
    : KeyValueStoreService(name, uuid)
    , PersistableKeyValueStoreService(name, uuid)
    , UnorderedMapKeyValueStoreService(name, uuid)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger()) {
}

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure> &configuration)
    : KeyValueStoreService(name)
    , PersistableKeyValueStoreService(name)
    , UnorderedMapKeyValueStoreService(name)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger())  {
  setConfiguration(configuration);
  initialize();
}

UnorderedMapPersistableKeyValueStoreService::~UnorderedMapPersistableKeyValueStoreService() {
}

void UnorderedMapPersistableKeyValueStoreService::initialize() {
//  ControllerService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(Directory);
  setSupportedProperties(supportedProperties);
}

void UnorderedMapPersistableKeyValueStoreService::onEnable() {
  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable UnorderedMapPersistableKeyValueStoreService");
    return;
  }

  if (!getProperty(Directory.getName(), directory_)) {
    logger_->log_error("Invalid or missing property: Directory");
  }

  logger_->log_trace("Enabled UnorderedMapPersistableKeyValueStoreService");
}

bool UnorderedMapPersistableKeyValueStoreService::persistUnlocked(const std::string& id) {
  auto it = maps_.find(id);
  if (it == maps_.end()) {
    return false;
  }
  std::ofstream ofs(utils::file::FileUtils::concat_path(directory_, id));
  if (!ofs.is_open()) {
    return false;
  }
  ofs << VERSION_KEY << "=" << std::to_string(it->second.first) << "\n";
  for (const auto& kv : it->second.second) {
    ofs << kv.first << "=" << kv.second << "\n";
  }
  return true;
}

bool UnorderedMapPersistableKeyValueStoreService::persist(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  utils::file::FileUtils::create_dir(directory_);
  return persistUnlocked(id);
}

bool UnorderedMapPersistableKeyValueStoreService::persist() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (utils::file::FileUtils::delete_dir(directory_, true /*delete_files_recursively*/) != 0) {
    return false;
  }
  if (utils::file::FileUtils::create_dir(directory_) != 0) {
    return false;
  }
  for (const auto& map : maps_) {
    if (!persistUnlocked(map.first)) {
      return false;
    }
  }
  return true;
}

bool UnorderedMapPersistableKeyValueStoreService::loadUnlocked(const std::string& id) {
  auto path = utils::file::FileUtils::concat_path(directory_, id);
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return false;
  }
  int64_t version = -1;
  std::unordered_map<std::string, std::string> map;
  std::string line;
  while (std::getline(ifs, line)) {
    size_t separator_pos = line.find('=');
    if (separator_pos == std::string::npos) {
      logger_->log_warn("None key-value line found in \"%s\": \"%s\"", path.c_str(), line.c_str());
      continue;
    }
    std::string key = line.substr(0, separator_pos);
    std::string value = line.substr(separator_pos + 1);
    if (key == VERSION_KEY) {
      try {
        version = std::stoll(value);
      } catch (...) {
        logger_->log_error("Invalid version number found in \"%s\": \"%s\"", path.c_str(), value.c_str());
        return false;
      }
    } else {
      map[key] = value;
    }
  }
  if (version == -1) {
    logger_->log_error("Version number missing from \"%s\"", path.c_str());
    return false;
  }
  auto it = maps_.find(id);
  if (it != maps_.end()) {
    if (version < it->second.first) {
      logger_->log_warn("Smaller version loaded from file \"%s\" than contained in memory: %ld < %ld", path.c_str(), version, it->second.first);
    }
  } else {
    it = maps_.emplace(id, std::make_pair(-1, std::unordered_map<std::string, std::string>())).first;
  }
  it->second = std::make_pair(version, std::move(map));
  return true;
}

bool UnorderedMapPersistableKeyValueStoreService::load(const std::string& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  return loadUnlocked(id);
}

bool UnorderedMapPersistableKeyValueStoreService::load() {
  std::lock_guard<std::mutex> lock(mutex_);
  maps_.clear();
  utils::file::FileUtils::list_dir(
      directory_,
      [this](const std::string& /*dir*/, const std::string& child) -> bool {
        return loadUnlocked(child); // TODO
      },
      logger_, false /*recursive*/);
  return true;
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
