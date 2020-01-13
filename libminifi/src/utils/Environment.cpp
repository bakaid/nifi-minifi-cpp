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

#include "utils/Environment.h"

#ifdef WIN32
#include <Windows.h>
#else
#include <cstdlib>
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

bool Environment::runningAsService_(false);
bool Environment::runningAsServiceSet_(false);

void Environment::accessEnvironment(const std::function<void(void)>& func) {
  static std::mutex environmentMutex;
  std::lock_guard<std::mutex> lock(environmentMutex);
  func();
}

std::pair<bool, std::string> Environment::getEnvironmentVariable(const char* name) {
  bool exists = false;
  std::string value;

  Environment::accessEnvironment([&exists, &value, name](){
#ifdef WIN32
    std::vector<char> buffer(32767U); // https://docs.microsoft.com/en-gb/windows/win32/api/processenv/nf-processenv-getenvironmentvariablea
    uint32_t ret = GetEnvironmentVariableA(name, buffer.data(), buffer.size());
    if (ret > 0U) {
      exists = true;
      value = std::string(buffer.data(), ret);
    }
#else
    char* ret = getenv(name);
    if (ret != nullptr) {
      exists = true;
      value = ret;
    }
#endif
  });

  return std::make_pair(exists, std::move(value));
}

bool Environment::setEnvironmentVariable(const char* name, const char* value) {
  bool success = false;

  Environment::accessEnvironment([&success, name, value](){
#ifdef WIN32
    success = SetEnvironmentVariableA(name, value);
#else
    int ret = setenv(name, value, 1 /*overwrite*/);
    success = ret == 0;
#endif
  });

  return success;
}

bool Environment::unsetEnvironmentVariable(const char* name) {
  bool success = false;

  Environment::accessEnvironment([&success, name](){
#ifdef WIN32
    success = SetEnvironmentVariableA(name, nullptr);
#else
    int ret = unsetenv(name);
    success = ret == 0;
#endif
  });

  return success;
}

bool /*success*/ Environment::setRunningAsService(bool runningAsService) {
  bool success = false;

  Environment::accessEnvironment([&success, runningAsService](){
    if (!runningAsServiceSet_) {
      runningAsService_ = runningAsService;
      runningAsServiceSet_ = true;
      success = true;
    }
  });

  return success;
}

bool Environment::isRunningAsService() {
  bool runningAsService = false;

  Environment::accessEnvironment([&runningAsService](){
    runningAsService = runningAsService_;
  });

  return runningAsService;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
