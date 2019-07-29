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
#include "FlowController.h"
#include "properties/Configure.h"
#include "unit/ProvenanceTestHelper.h"
#include "io/StreamFactory.h"
#include "processors/SourceInitiatedSubscription.h"

TEST_CASE("SourceInitiatedSubscriptionTest", "[basic]") {
  TestController testController;
  auto plan = testController.createPlan();

  LogTestController::getInstance().setTrace<processors::SourceInitiatedSubscription>();
  
  auto source_initiated_subscription = plan->addProcessor("SourceInitiatedSubscription",
                                                          "SourceInitiatedSubscription");

  plan->setProperty(source_initiated_subscription, "Listen Hostname", "23.96.27.78");
  plan->setProperty(source_initiated_subscription, "Listen Port", "5986");
  plan->setProperty(source_initiated_subscription, "SSL Certificate", "/home/bakaid/certs/server.pem");
  plan->setProperty(source_initiated_subscription, "SSL Certificate Authority", "/home/bakaid/certs/ca.crt");
  plan->setProperty(source_initiated_subscription, "Initial Existing Events Strategy", processors::SourceInitiatedSubscription::INITIAL_EXISTING_EVENTS_STRATEGY_ALL);
  plan->setProperty(source_initiated_subscription, "State File", "/tmp/wef.state");

  testController.runSession(plan, true);

  std::this_thread::sleep_for(std::chrono::seconds(3600));
}

