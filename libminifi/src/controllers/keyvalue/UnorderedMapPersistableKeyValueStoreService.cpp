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
#include "utils/StringUtils.h"

#include <fstream>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

constexpr int UnorderedMapPersistableKeyValueStoreService::FORMAT_VERSION;

core::Property UnorderedMapPersistableKeyValueStoreService::File(
    core::PropertyBuilder::createProperty("File")->withDescription("Path to a file to store state")
        ->isRequired(true)->build());

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::string& id)
    : KeyValueStoreService(name, id)
    , AbstractAutoPersistingKeyValueStoreService(name, id)
    , UnorderedMapKeyValueStoreService(name, id)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger()) {
}

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, utils::Identifier uuid /*= utils::Identifier()*/)
    : KeyValueStoreService(name, uuid)
    , AbstractAutoPersistingKeyValueStoreService(name, uuid)
    , UnorderedMapKeyValueStoreService(name, uuid)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger()) {
}

UnorderedMapPersistableKeyValueStoreService::UnorderedMapPersistableKeyValueStoreService(const std::string& name, const std::shared_ptr<Configure> &configuration)
    : KeyValueStoreService(name)
    , AbstractAutoPersistingKeyValueStoreService(name)
    , UnorderedMapKeyValueStoreService(name)
    , logger_(logging::LoggerFactory<UnorderedMapPersistableKeyValueStoreService>::getLogger())  {
  setConfiguration(configuration);
  initialize();
}

UnorderedMapPersistableKeyValueStoreService::~UnorderedMapPersistableKeyValueStoreService() {
}

std::string UnorderedMapPersistableKeyValueStoreService::escape(const std::string& str) {
  std::stringstream escaped;
  for (const auto c : str) {
    switch (c) {
      case '\\':
        escaped << "\\\\";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '=':
        escaped << "\\=";
        break;
      default:
        escaped << c;
        break;
    }
  }
  return escaped.str();
}

std::string UnorderedMapPersistableKeyValueStoreService::unescape(const std::string& str) {
  std::stringstream unescaped;
  bool in_escape_sequence = false;
  for (const auto c : str) {
    if (in_escape_sequence) {
      switch (c) {
        case '\\':
          unescaped << '\\';
          break;
        case 'n':
          unescaped << '\n';
          break;
        case '=':
          unescaped << '=';
          break;
        default:
          logger_->log_error("Invalid escape sequence in \"%s\": \"\\%c\"", str.c_str(), c);
          break;
      }
      in_escape_sequence = false;
    } else {
      if (c == '\\') {
        in_escape_sequence = true;
      } else {
        unescaped << c;
      }
    }
  }
  if (in_escape_sequence) {
    logger_->log_error("Unterminated escape sequence in \"%s\"", str.c_str());
  }
  return unescaped.str();
}

void UnorderedMapPersistableKeyValueStoreService::initialize() {
  AbstractAutoPersistingKeyValueStoreService::initialize();
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(File);
  updateSupportedProperties(supportedProperties);
}

void UnorderedMapPersistableKeyValueStoreService::onEnable() {
  if (configuration_ == nullptr) {
    logger_->log_debug("Cannot enable UnorderedMapPersistableKeyValueStoreService");
    return;
  }

  if (!getProperty(File.getName(), file_)) {
    logger_->log_error("Invalid or missing property: File");
    return;
  }

  /* We must not start the persistence thread until we attempted to load the state */
  load();

  AbstractAutoPersistingKeyValueStoreService::onEnable();

  logger_->log_trace("Enabled UnorderedMapPersistableKeyValueStoreService");
}

bool UnorderedMapPersistableKeyValueStoreService::set(const std::string& key, const std::string& value) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::set(key, value);
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::remove(const std::string& key) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::remove(key);
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::update(const std::string& key, const std::function<bool(bool /*exists*/, std::string& /*value*/)>& update_func) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  bool res = UnorderedMapKeyValueStoreService::update(key, update_func);
  if (always_persist_ && res) {
    return persist();
  }
  return res;
}

bool UnorderedMapPersistableKeyValueStoreService::persist() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::ofstream ofs(file_);
  if (!ofs.is_open()) {
    return false;
  }
  ofs << escape(FORMAT_VERSION_KEY) << "=" << escape(std::to_string(FORMAT_VERSION)) << "\n";
  for (const auto& kv : map_) {
    ofs << escape(kv.first) << "=" << escape(kv.second) << "\n";
  }
  return true;
}

bool UnorderedMapPersistableKeyValueStoreService::load() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::ifstream ifs(file_);
  if (!ifs.is_open()) {
    return false;
  }
  std::unordered_map<std::string, std::string> map;
  std::string line;
  while (std::getline(ifs, line)) {
    size_t separator_pos = 0U;
    while ((separator_pos = line.find('=', separator_pos)) != std::string::npos) {
      if (separator_pos == 0U) {
        logger_->log_warn("Line with empty key found in \"%s\": \"%s\"", file_.c_str(), line.c_str());
        continue;
      }
      if (line[separator_pos - 1] != '\\' || (separator_pos > 1U && line[separator_pos - 2] == '\\')) {
        break;
      }
      separator_pos += 1;
    }
    if (separator_pos == std::string::npos) {
      logger_->log_warn("None key-value line found in \"%s\": \"%s\"", file_.c_str(), line.c_str());
      continue;
    }
    std::string key = unescape(line.substr(0, separator_pos));
    std::string value = unescape(line.substr(separator_pos + 1));
    if (key == FORMAT_VERSION_KEY) {
      int format_version = 0;
      try {
        format_version = std::stoi(value);
      } catch (...) {
        logger_->log_error("Invalid format version number found in \"%s\": \"%s\"", file_.c_str(), value.c_str());
        return false;
      }
      if (format_version > FORMAT_VERSION) {
        logger_->log_error("\"%s\" has been serialized with a larger format version than currently known: %d > %d", file_.c_str(), format_version, FORMAT_VERSION);
        return false;
      }
    } else {
        map[key] = value;
    }
  }
  map_ = std::move(map);
  return true;
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
