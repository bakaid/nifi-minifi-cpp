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
#include "processors/ListSFTP.h"
#include "processors/GenerateFlowFile.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "tools/SFTPTestServer.h"

class ListSFTPTestsFixture {
 public:
  ListSFTPTestsFixture()
  : src_dir(strdup("/tmp/sftps.XXXXXX")) {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::GenerateFlowFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::ListSFTP>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();
    LogTestController::getInstance().setDebug<SFTPTestServer>();

    // Create temporary directories
    testController.createTempDirectory(src_dir);
    REQUIRE(src_dir != nullptr);

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(src_dir));
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    plan = testController.createPlan();
    list_sftp = plan->addProcessor(
        "ListSFTP",
        "ListSFTP");
    log_attribute = plan->addProcessor("LogAttribute",
        "LogAttribute",
        core::Relationship("success", "d"),
        true);

    // Configure ListSFTP processor
    plan->setProperty(list_sftp, "Listing Strategy", processors::ListSFTP::LISTING_STRATEGY_TRACKING_TIMESTAMPS);
    plan->setProperty(list_sftp, "Hostname", "localhost");
    plan->setProperty(list_sftp, "Port", std::to_string(sftp_server->getPort()));
    plan->setProperty(list_sftp, "Username", "nifiuser");
    plan->setProperty(list_sftp, "Password", "nifipassword");
    plan->setProperty(list_sftp, "Search Recursively", "false");
    plan->setProperty(list_sftp, "Follow symlink", "false");
    plan->setProperty(list_sftp, "Ignore Dotted Files", "false");
    plan->setProperty(list_sftp, "Strict Host Key Checking", "false");
    plan->setProperty(list_sftp, "Connection Timeout", "30 sec");
    plan->setProperty(list_sftp, "Data Timeout", "30 sec");
    plan->setProperty(list_sftp, "Send Keep Alive On Timeout", "true");
    plan->setProperty(list_sftp, "Target System Timestamp Precision", processors::ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT);
    plan->setProperty(list_sftp, "Minimum File Age", "0 sec");
    plan->setProperty(list_sftp, "Minimum File Size", "0 B");

    // Configure LogAttribute processor
    plan->setProperty(log_attribute, "FlowFiles To Log", "0");
  }

  virtual ~ListSFTPTestsFixture() {
    free(src_dir);
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string& relative_path, const std::string& content) {
    std::fstream file;
    std::stringstream ss;
    ss << src_dir << "/vfs/" << relative_path;
    utils::file::FileUtils::create_dir(utils::file::FileUtils::get_parent_path(ss.str()));
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
  }

 protected:
  char *src_dir;
  std::unique_ptr<SFTPTestServer> sftp_server;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::Processor> list_sftp;
  std::shared_ptr<core::Processor> log_attribute;
};

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list one file", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Remote Path", "nifi_test/");

  createFile("nifi_test/tstFile.ext", "Test content 1");

  testController.runSession(plan, true);
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list two files", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Remote Path", "nifi_test/");

  createFile("nifi_test/file1.ext", "Test content 1");
  createFile("nifi_test/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list two files, one in a subdir, no recursion", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Remote Path", "nifi_test/");

  createFile("nifi_test/file1.ext", "Test content 1");
  createFile("nifi_test/subdir/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);
}

TEST_CASE_METHOD(ListSFTPTestsFixture, "ListSFTP list two files, one in a subdir, with recursion", "[ListSFTP][basic]") {
  plan->setProperty(list_sftp, "Remote Path", "nifi_test/");
  plan->setProperty(list_sftp, "Search Recursively", "true");

  createFile("nifi_test/file1.ext", "Test content 1");
  createFile("nifi_test/subdir/file2.ext", "Test with longer content 2");

  testController.runSession(plan, true);
}