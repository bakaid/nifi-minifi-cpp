/**
 *
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

#include <sys/stat.h>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "c2/C2Agent.h"
#include "SFTPClient.h"
#include <cstring>


TEST_CASE("SFTPClientTest", "[sftptest]") {
  LogTestController::getInstance().setInfo<minifi::FlowController>();
  LogTestController::getInstance().setDebug<minifi::utils::SFTPClient>();

  org::apache::nifi::minifi::utils::SFTPClient sftp_client("localhost", 22, "test");
  assert(sftp_client.setVerbose());
  sftp_client.setPasswordAuthenticationCredentials("<redacted>");

  assert(sftp_client.connect());

  std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>> children;
  assert(sftp_client.listDirectory("/Users/test/", false /*follow_symlinks*/, children));
  for (const auto& child : children) {
    std::cerr << std::get<0>(child) << ", dir: " << static_cast<bool>(LIBSSH2_SFTP_S_ISDIR(std::get<2>(child).permissions))
        << ", link: " << static_cast<bool>(LIBSSH2_SFTP_S_ISLNK(std::get<2>(child).permissions)) << std::endl;
  }

  LogTestController::getInstance().reset();
}
