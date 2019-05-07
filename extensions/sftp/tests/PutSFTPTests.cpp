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
#include "processors/PutSFTP.h"
#include "processors/GetFile.h"
#include "tools/SFTPTestServer.h"

TEST_CASE("PutSFTP put file", "[testPutSFTPFile]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GetFile>();
  LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
  LogTestController::getInstance().setTrace<processors::PutSFTP>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Create temporary directories
  char format1[] = "/tmp/gt.XXXXXX";
  char *src_dir = testController.createTempDirectory(format1);
  REQUIRE(src_dir != nullptr);

  char format2[] = "/tmp/ft.XXXXXX";
  char *dst_dir = testController.createTempDirectory(format2);
  REQUIRE(dst_dir != nullptr);

  // Start SFTP server
  SFTPTestServer sftp_server(dst_dir);
  REQUIRE(true == sftp_server.start());

  // Build MiNiFi processing graph
  auto getfile = plan->addProcessor(
      "GetFile",
      "GetFile");
  auto put = plan->addProcessor(
      "PutSFTP",
      "PutSFTP",
      core::Relationship("success", "description"),
      true);
  plan->addProcessor("LogAttribute", "LogAttribute", { core::Relationship("success", "d"), core::Relationship("reject", "d"), core::Relationship("failure", "d") }, true);

  // Configure GetFile processor
  plan->setProperty(getfile, "Input Directory", src_dir);

  // Configure PutSFTP processor
  plan->setProperty(put, "Hostname", "localhost");
  plan->setProperty(put, "Port", std::to_string(sftp_server.getPort()));
  plan->setProperty(put, "Username", "nifiuser");
  plan->setProperty(put, "Password", "nifipassword");
  plan->setProperty(put, "Remote Path", "nifi_test/");
  plan->setProperty(put, "Create Directory", "true");
  plan->setProperty(put, "Batch Size", "2");
  plan->setProperty(put, "Connection Timeout", "30 sec");
  plan->setProperty(put, "Data Timeout", "30 sec");
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);
  plan->setProperty(put, "Strict Host Key Checking", "false");
  plan->setProperty(put, "Send Keep Alive On Timeout", "false");
  plan->setProperty(put, "Use Compression", "false");
  plan->setProperty(put, "Reject Zero-Byte Files", "true");

  // Create source file
  auto createFile = [&](const std::string& relative_path, const std::string& content) {
    std::fstream file;
    std::stringstream ss;
    ss << src_dir << "/" << relative_path;
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
  };

  // Test target file
  auto testFile = [&](const std::string& relative_path, const std::string& expected_content) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    std::ifstream file(resultFile.str());
    REQUIRE(true == file.good());
    std::stringstream content;
    std::vector<char> buffer(1024U);
    while (file) {
      file.read(buffer.data(), buffer.size());
      content << std::string(buffer.data(), file.gcount());
    }
    REQUIRE(expected_content == content.str());
  };

  SECTION("Put one file") {
    createFile("tstFile.ext", "tempFile");

    testController.runSession(plan, true);

    testFile("nifi_test/tstFile.ext", "tempFile");
  }

  SECTION("Put two files") {
    createFile("tstFile1.ext", "content 1");
    createFile("tstFile2.ext", "content 2");

    testController.runSession(plan, true);

    testFile("nifi_test/tstFile1.ext", "content 1");
    testFile("nifi_test/tstFile2.ext", "content 2");
  }

  // private key auth
  // both auth
  // host key file test (both strict and non-strict)
  // disable directory listing test by setting 0100 on the directories
  // create directory disable test
  // conflict resolution tests
  //  - directory in place of target file
  //  - replace
  //  - ignore
  //  - rename
  //  - reject
  //  - fail
  // reject zero-byte
  // disable dot-rename test by creating an unoverwriteable dot file
  // temporary filename test (with expression language)
  // permissions (non-windows)
  // remote owner and group (non-windows)
  // modification time
  // batching tests
  // proxy tests -> not really feasible, manual/docker tests
}