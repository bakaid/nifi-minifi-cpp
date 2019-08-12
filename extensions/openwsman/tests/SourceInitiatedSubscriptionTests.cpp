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
#ifndef WIN32
#include <unistd.h>
#endif

#include "TestBase.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "core/repository/VolatileContentRepository.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "processors/SourceInitiatedSubscription.h"
#include "processors/LogAttribute.h"
#include "processors/UpdateAttribute.h"
#include "processors/PutFile.h"

TEST_CASE("SourceInitiatedSubscriptionTest", "[basic]") {
  TestController testController;
  auto plan = testController.createPlan();

  LogTestController::getInstance().setDebug<minifi::FlowController>();
  LogTestController::getInstance().setDebug<minifi::SchedulingAgent>();
  LogTestController::getInstance().setDebug<minifi::core::ProcessGroup>();
  LogTestController::getInstance().setDebug<minifi::core::Processor>();
  LogTestController::getInstance().setDebug<minifi::core::ProcessSession>();
  LogTestController::getInstance().setTrace<processors::SourceInitiatedSubscription>();
  LogTestController::getInstance().setDebug<processors::LogAttribute>();
  LogTestController::getInstance().setDebug<processors::PutFile>();
  LogTestController::getInstance().setDebug<minifi::core::repository::VolatileContentRepository>();
  
  auto source_initiated_subscription = plan->addProcessor("SourceInitiatedSubscription",
                                                          "SourceInitiatedSubscription");
  auto put_file = plan->addProcessor("PutFile",
                                     "PutFile",
                                     core::Relationship("success", "d"),
                                     true);
//   auto log_attribute = plan->addProcessor("LogAttribute",
//                                           "LogAttribute",
//                                           core::Relationship("success", "d"),
//                                           true);

  plan->setProperty(source_initiated_subscription, "Listen Hostname", "23.96.27.78");
  plan->setProperty(source_initiated_subscription, "Listen Port", "5986");
  plan->setProperty(source_initiated_subscription, "SSL Certificate", "/Users/danielbakai/certs/server.pem");
  plan->setProperty(source_initiated_subscription, "SSL Certificate Authority", "/Users/danielbakai/certs/ca.crt");
  plan->setProperty(source_initiated_subscription, "Initial Existing Events Strategy", processors::SourceInitiatedSubscription::INITIAL_EXISTING_EVENTS_STRATEGY_ALL);
  plan->setProperty(source_initiated_subscription, "State File", "/tmp/wef.state");
  plan->setProperty(source_initiated_subscription, "XPath XML Query",
    "<QueryList>\n"
    "  <Query Id=\"0\">\n"
    "    <Select Path=\"Application\">*[System[Level=1 or Level=2]]</Select>\n"
    "  </Query>\n"
    "</QueryList>\n");
  
//   plan->setProperty(log_attribute, "FlowFiles To Log", "0");
  
  plan->setProperty(put_file, "Directory", "/tmp/wef");

  plan->runNextProcessor();
  plan->runNextProcessor();
  
  while (true) {
      plan->runCurrentProcessor();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

