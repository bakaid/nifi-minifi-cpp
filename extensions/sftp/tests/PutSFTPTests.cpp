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
#include "processors/PutSFTP.h"
#include "processors/GenerateFlowFile.h"


TEST_CASE("PutSFTP simple test", "[PutSFTPSimple]") {
  TestController testController;

  LogTestController::getInstance().setTrace<TestPlan>();
  LogTestController::getInstance().setTrace<processors::GenerateFlowFile>();
  LogTestController::getInstance().setDebug<minifi::utils::SFTPClient>();
  LogTestController::getInstance().setTrace<processors::PutSFTP>();

  auto plan = testController.createPlan();
  auto repo = std::make_shared<TestRepository>();

  // Build MiNiFi processing graph
  auto generate = plan->addProcessor(
      "GenerateFlowFile",
      "Generate");
  auto put = plan->addProcessor(
      "PutSFTP",
      "PutSFTP",
      core::Relationship("success", "description"),
      true);
  plan->setProperty(put, "Hostname", "localhost");
  plan->setProperty(put, "Port", "22");
  plan->setProperty(put, "Username", "test");
  plan->setProperty(put, "Password", "<redacted>");
  plan->setProperty(put, "Remote Path", "/Users/test/processor-test");
  plan->setProperty(put, "Create Directory", "true");
  plan->setProperty(put, "Batch Size", "500");
  plan->setProperty(put, "Connection Timeout", "30 sec");
  plan->setProperty(put, "Data Timeout", "30 sec");
  plan->setProperty(put, "Conflict Resolution", "NONE");
  plan->setProperty(put, "Strict Host Key Checking", "false");
  plan->setProperty(put, "Send Keep Alive On Timeout", "false");
  plan->setProperty(put, "Use Compression", "false");
  plan->setProperty(put, "Permissions", "rwxrwx---");
  plan->setProperty(put, "Last Modified Time", "1990-05-21T12:32:12Z");

  plan->runNextProcessor();  // Generate
  plan->runNextProcessor();  // PutSFTP

  // Verify output state
}
