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
#include <cstring>
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
#include <algorithm>
#include <functional>
#include <iterator>
#include <random>

#include "TestBase.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "processors/FetchSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/ExtractText.h"
#include "processors/UpdateAttribute.h"
#include "tools/SFTPTestServer.h"

class FetchSFTPTestsFixture {
 public:
  FetchSFTPTestsFixture()
  : src_dir(strdup("/tmp/sftps.XXXXXX")) {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::FetchSFTP>();
    LogTestController::getInstance().setTrace<processors::ExtractText>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    // Create temporary directory
    testController.createTempDirectory(src_dir);
    REQUIRE(src_dir != nullptr);

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(src_dir));
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    plan = testController.createPlan();
    generate_flow_file = plan->addProcessor(
        "GenerateFlowFile",
        "GenerateFlowFile");
    update_attribute = plan->addProcessor("UpdateAttribute",
         "UpdateAttribute",
         core::Relationship("success", "d"),
         true);
    fetch_sftp = plan->addProcessor(
        "FetchSFTP",
        "FetchSFTP",
        core::Relationship("success", "d"),
        true);
    plan->addProcessor("LogAttribute",
        "LogAttribute",
        { core::Relationship("success", "d"),
          core::Relationship("comms.failure", "d"),
          core::Relationship("not.found", "d"),
          core::Relationship("permission.denied", "d")},
          true);

    // Configure GetFile processor
    plan->setProperty(generate_flow_file, "File Size", "0B");

    // Configure FetchSFTP processor
    plan->setProperty(fetch_sftp, "Hostname", "localhost");
    plan->setProperty(fetch_sftp, "Port", std::to_string(sftp_server->getPort()));
    plan->setProperty(fetch_sftp, "Username", "nifiuser");
    plan->setProperty(fetch_sftp, "Password", "nifipassword");
    plan->setProperty(fetch_sftp, "Completion Strategy", processors::FetchSFTP::COMPLETION_STRATEGY_NONE);
    plan->setProperty(fetch_sftp, "Connection Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Data Timeout", "30 sec");
    plan->setProperty(fetch_sftp, "Strict Host Key Checking", "false");
    plan->setProperty(fetch_sftp, "Send Keep Alive On Timeout", "true");
    plan->setProperty(fetch_sftp, "Use Compression", "false");
  }

  virtual ~FetchSFTPTestsFixture() {
    free(src_dir);
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content) {
    std::fstream file;
    std::stringstream ss;
    ss << src_dir << "/vfs/" << relative_path;
    REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::get_parent_path(ss.str())));
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
  }

 protected:
  char *src_dir;
  std::unique_ptr<SFTPTestServer> sftp_server;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::Processor> generate_flow_file;
  std::shared_ptr<core::Processor> update_attribute;
  std::shared_ptr<core::Processor> fetch_sftp;
};

TEST_CASE_METHOD(FetchSFTPTestsFixture, "FetchSFTP fetch one file", "[PutSFTP][basic]") {
  createFile("nifi_test/tstFile.ext", "Test content 1");

  plan->setProperty(fetch_sftp, "Remote File", "nifi_test/tstFile.ext");

  testController.runSession(plan, true);
}
