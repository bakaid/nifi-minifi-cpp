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
#include "processors/PutSFTP.h"
#include "processors/GetFile.h"
#include "tools/SFTPTestServer.h"
#include "processors/LogAttribute.h"

class PutSFTPTestsFixture {
 public:
  PutSFTPTestsFixture()
  : src_dir(strdup("/tmp/sftps.XXXXXX"))
  , dst_dir(strdup("/tmp/sftpd.XXXXXX")) {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setDebug<minifi::FlowController>();
    LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
    LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
    LogTestController::getInstance().setDebug<minifi::core::Processor>();
    LogTestController::getInstance().setTrace<minifi::core::ProcessSession>();
    LogTestController::getInstance().setDebug<processors::GetFile>();
    LogTestController::getInstance().setTrace<minifi::utils::SFTPClient>();
    LogTestController::getInstance().setTrace<processors::PutSFTP>();
    LogTestController::getInstance().setDebug<processors::LogAttribute>();

    // Create temporary directories
    testController.createTempDirectory(src_dir);
    REQUIRE(src_dir != nullptr);
    testController.createTempDirectory(dst_dir);
    REQUIRE(dst_dir != nullptr);

    // Start SFTP server
    sftp_server = std::unique_ptr<SFTPTestServer>(new SFTPTestServer(dst_dir));
    REQUIRE(true == sftp_server->start());

    // Build MiNiFi processing graph
    plan = testController.createPlan();
    getfile = plan->addProcessor(
        "GetFile",
        "GetFile");
    put = plan->addProcessor(
        "PutSFTP",
        "PutSFTP",
        core::Relationship("success", "description"),
        true);
    plan->addProcessor("LogAttribute",
        "LogAttribute",
        { core::Relationship("success", "d"),
          core::Relationship("reject", "d"),
          core::Relationship("failure", "d") },
          true);

    // Configure GetFile processor
    plan->setProperty(getfile, "Input Directory", src_dir);

    // Configure PutSFTP processor
    plan->setProperty(put, "Hostname", "localhost");
    plan->setProperty(put, "Port", std::to_string(sftp_server->getPort()));
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
  }

  ~PutSFTPTestsFixture() {
    free(src_dir);
    free(dst_dir);
    LogTestController::getInstance().reset();
  }

  // Create source file
  void createFile(const std::string &dir, const std::string& relative_path, const std::string& content) {
    std::fstream file;
    std::stringstream ss;
    ss << dir << "/" << relative_path;
    file.open(ss.str(), std::ios::out);
    file << content;
    file.close();
  }

  // Test target file
  void testFile(const std::string& relative_path, const std::string& expected_content) {
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
  }

  void testFileNotExists(const std::string& relative_path) {
    std::stringstream resultFile;
    resultFile << dst_dir << "/vfs/" << relative_path;
    std::ifstream file(resultFile.str());
    REQUIRE(false == file.is_open());
    REQUIRE(false == file.good());
  }

 protected:
  char *src_dir;
  char *dst_dir;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
  std::unique_ptr<SFTPTestServer> sftp_server;
  std::shared_ptr<core::Processor> getfile;
  std::shared_ptr<core::Processor> put;
};

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP put one file", "[testPutSFTPFile]") {
  createFile(src_dir, "tstFile.ext", "tempFile");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile.ext", "tempFile");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP put two files", "[testPutSFTPFile]") {
  createFile(src_dir, "tstFile1.ext", "content 1");
  createFile(src_dir, "tstFile2.ext", "content 2");

  testController.runSession(plan, true);

  testFile("nifi_test/tstFile1.ext", "content 1");
  testFile("nifi_test/tstFile2.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution rename", "[testPutSFTPFile]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/1.tstFile1.ext", "content 1");
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution reject", "[testPutSFTPFile]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REJECT);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship reject"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution fail", "[testPutSFTPFile]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_FAIL);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution ignore", "[testPutSFTPFile]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_IGNORE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Routing tstFile1.ext to SUCCESS despite a file with the same name already existing"));
  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution replace", "[testPutSFTPFile]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REPLACE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/tstFile1.ext", "content 1");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution none", "[testPutSFTPFile]") {
  plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_NONE);

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  createFile(utils::file::FileUtils::concat_path(dst_dir, "vfs"), "nifi_test/tstFile1.ext", "content 2");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  testFile("nifi_test/tstFile1.ext", "content 2");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP conflict resolution with directory existing at target", "[testPutSFTPFile]") {
  bool should_predetect_failure = true;
  SECTION("with conflict resolution rename") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_RENAME);
  }
  SECTION("with conflict resolution reject") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REJECT);
  }
  SECTION("with conflict resolution fail") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_FAIL);
  }
  SECTION("with conflict resolution ignore") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_IGNORE);
  }
  SECTION("with conflict resolution replace") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_REPLACE);
  }
  SECTION("with conflict resolution none") {
    plan->setProperty(put, "Conflict Resolution", processors::PutSFTP::CONFLICT_RESOLUTION_NONE);
    should_predetect_failure = false;
  }

  createFile(src_dir, "tstFile1.ext", "content 1");
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test")));
  REQUIRE(0 == utils::file::FileUtils::create_dir(utils::file::FileUtils::concat_path(dst_dir, "vfs/nifi_test/tstFile1.ext")));

  testController.runSession(plan, true);

  if (should_predetect_failure) {
    REQUIRE(LogTestController::getInstance().contains("Rejecting tstFile1.ext because a directory with the same name already exists"));
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship reject"));
  } else {
    REQUIRE(LogTestController::getInstance().contains("Failed to rename remote file \"nifi_test/.tstFile1.ext\" to \"nifi_test/tstFile1.ext\", error: LIBSSH2_FX_FILE_ALREADY_EXISTS"));
    REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship failure"));
  }
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP reject zero-byte false", "[testPutSFTPFile]") {
  plan->setProperty(put, "Reject Zero-Byte Files", "false");

  createFile(src_dir, "tstFile1.ext", "");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship success"));
  testFile("nifi_test/tstFile1.ext", "");
}

TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP reject zero-byte true", "[testPutSFTPFile]") {
  plan->setProperty(put, "Reject Zero-Byte Files", "true");

  createFile(src_dir, "tstFile1.ext", "");

  testController.runSession(plan, true);

  REQUIRE(LogTestController::getInstance().contains("Rejecting tstFile1.ext because it is zero bytes"));
  REQUIRE(LogTestController::getInstance().contains("from PutSFTP to relationship reject"));
  testFileNotExists("nifi_test/tstFile1.ext");
}

//TEST_CASE_METHOD(PutSFTPTestsFixture, "PutSFTP bad password", "[testPutSFTPFile]") {
//  plan->setProperty(put, "Password", "badpassword");
//  createFile(src_dir, "tstFile.ext", "tempFile");
//
//  testController.runSession(plan, true);
//}


// private key auth
// both auth
// host key file test (both strict and non-strict)
// disable directory listing test by setting 0100 on the directories
// create directory disable test
// conflict resolution tests
//  - directory in place of target file -> OK
//  - replace -> OK
//  - ignore -> OK
//  - rename -> OK
//  - reject -> OK
//  - fail -> OK
//  - none -> OK
// reject zero-byte -> OK
// disable dot-rename test by creating an unoverwriteable dot file
// temporary filename test (with expression language)
// permissions (non-windows)
// remote owner and group (non-windows)
// modification time
// batching tests
// proxy tests -> not really feasible, manual/docker tests