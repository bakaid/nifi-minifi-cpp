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
#include <cstring>
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
#include "tools/SFTPTestServer.h"


TEST_CASE("SFTPClientTest", "[sftptest]") {
  LogTestController::getInstance().setDebug<minifi::utils::SFTPClient>();

  SFTPTestServer sftp_server("/tmp/test1");
  REQUIRE(true == sftp_server.start());

  utils::SFTPClient sftp_client("localhost", sftp_server.getPort(), "nifiuser");
  REQUIRE(true == sftp_client.setVerbose());
//  utils::HTTPProxy proxy;
//  proxy.host = "localhost";
//  proxy.port = 3128;
//  REQUIRE(true == sftp_client.setProxy(utils::SFTPClient::ProxyType::Http, proxy));
  sftp_client.setPasswordAuthenticationCredentials("nifipassword");
//  sftp_client.setPublicKeyAuthenticationCredentials("/Users/danielbakai/.ssh/id_rsa", "");
//  assert(sftp_client.setHostKeyFile("/tmp/known_hosts", false /*strict_host_checking*/));

  REQUIRE(true == sftp_client.connect());

  std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>> children;
  REQUIRE(true == sftp_client.listDirectory("", false /*follow_symlinks*/, children));
  for (const auto& child : children) {
    std::cerr << std::get<0>(child) << ", dir: " << static_cast<bool>(LIBSSH2_SFTP_S_ISDIR(std::get<2>(child).permissions))
        << ", link: " << static_cast<bool>(LIBSSH2_SFTP_S_ISLNK(std::get<2>(child).permissions)) << std::endl;
  }

  LIBSSH2_SFTP_ATTRIBUTES attrs;
  bool file_not_exists;
  REQUIRE(false == sftp_client.stat("/Users/test/foobar", false /*follow_symlinks*/, attrs, file_not_exists));
  REQUIRE(true == file_not_exists);

  REQUIRE(true == sftp_server.stop());

  LogTestController::getInstance().reset();
}
