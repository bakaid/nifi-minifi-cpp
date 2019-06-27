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

#include <uuid/uuid.h>
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "../TestBase.h"
#include "../../controller/Controller.h"
#include "core/controller/ControllerService.h"
#include "c2/ControllerSocketProtocol.h"
#include "controllers/keyvalue/UnorderedMapPersistableKeyValueStoreService.h"

class UnorderedMapPersistableKeyValueStoreServiceTestsFixture {
 public:
  UnorderedMapPersistableKeyValueStoreServiceTestsFixture()
      : state_dir(strdup("/tmp/state.XXXXXX")) {
    LogTestController::getInstance().setTrace<TestPlan>();
    LogTestController::getInstance().setTrace<minifi::controllers::AbstractAutoPersistingKeyValueStoreService>();
    LogTestController::getInstance().setTrace<minifi::controllers::UnorderedMapKeyValueStoreService>();
    LogTestController::getInstance().setTrace<minifi::controllers::UnorderedMapPersistableKeyValueStoreService>();

    // Create temporary directories
    testController.createTempDirectory(state_dir, false /*cleanup*/);
    REQUIRE(state_dir != nullptr);

    createPlan();
  }

  virtual ~UnorderedMapPersistableKeyValueStoreServiceTestsFixture() {
    free(state_dir);
    LogTestController::getInstance().reset();
  }

  void createPlan() {
    plan = testController.createPlan();

    persistable_key_value_store_service_node = plan->addController(
        "UnorderedMapPersistableKeyValueStoreService",
        "UnorderedMapPersistableKeyValueStoreService");
    plan->setProperty(persistable_key_value_store_service_node, "File", utils::file::FileUtils::concat_path(state_dir, "state.txt"));
    plan->setProperty(persistable_key_value_store_service_node, "Auto Persistence Interval", "0 sec");
  }

  std::shared_ptr<minifi::controllers::PersistableKeyValueStoreService> getController() {
    return std::dynamic_pointer_cast<minifi::controllers::PersistableKeyValueStoreService>(
        persistable_key_value_store_service_node->getControllerServiceImplementation());
  }

 protected:
  char *state_dir;
  TestController testController;
  std::shared_ptr<TestPlan> plan;
  std::shared_ptr<core::controller::ControllerServiceNode> persistable_key_value_store_service_node;
};


TEST_CASE_METHOD(UnorderedMapPersistableKeyValueStoreServiceTestsFixture, "UnorderedMapPersistableKeyValueStoreService set and get", "[basic]") {
  plan->finalize();
  auto controller = getController();

  const char* key = "foobar";
  const char* value = "234";
  REQUIRE(true == controller->set(key, value));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    createPlan();
    plan->finalize();
    auto controller = getController();
  }

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}

TEST_CASE_METHOD(UnorderedMapPersistableKeyValueStoreServiceTestsFixture, "UnorderedMapPersistableKeyValueStoreService special characters", "[basic]") {
  plan->finalize();
  auto controller = getController();

  const char* key = "[]{}()==\\=\n\n";
  const char* value = ":./'\\=!\n=[]{}()";
  REQUIRE(true == controller->set(key, value));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    createPlan();
    plan->finalize();
    auto controller = getController();
  }

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}

TEST_CASE_METHOD(UnorderedMapPersistableKeyValueStoreServiceTestsFixture, "UnorderedMapPersistableKeyValueStoreService set and get all", "[basic]") {
  plan->finalize();
  auto controller = getController();

  const std::unordered_map<std::string, std::string> kvs = {
      {"foobar", "234"},
      {"buzz", "value"},
  };
  for (const auto& kv : kvs) {
    REQUIRE(true == controller->set(kv.first, kv.second));
  }

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    createPlan();
    plan->finalize();
    auto controller = getController();
  }

  std::unordered_map<std::string, std::string> kvs_res;
  REQUIRE(true == controller->get(kvs_res));
  REQUIRE(kvs == kvs_res);
}

TEST_CASE_METHOD(UnorderedMapPersistableKeyValueStoreServiceTestsFixture, "UnorderedMapPersistableKeyValueStoreService set and overwrite", "[basic]") {
  plan->finalize();
  auto controller = getController();

  const char* key = "foobar";
  const char* value = "234";
  const char* new_value = "baz";
  REQUIRE(true == controller->set(key, value));
  REQUIRE(true == controller->set(key, new_value));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    createPlan();
    plan->finalize();
    auto controller = getController();
  }

  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(new_value == res);
}

TEST_CASE_METHOD(UnorderedMapPersistableKeyValueStoreServiceTestsFixture, "UnorderedMapPersistableKeyValueStoreService set and remove", "[basic]") {
  plan->finalize();
  auto controller = getController();

  const char* key = "foobar";
  const char* value = "234";
  REQUIRE(true == controller->set(key, value));
  REQUIRE(true == controller->remove(key));
  REQUIRE(false == controller->remove(key));

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    createPlan();
    plan->finalize();
    auto controller = getController();
  }

  std::string res;
  REQUIRE(false == controller->get(key, res));
}


TEST_CASE_METHOD(UnorderedMapPersistableKeyValueStoreServiceTestsFixture, "UnorderedMapPersistableKeyValueStoreService set and clear", "[basic]") {
  plan->finalize();
  auto controller = getController();

  const std::unordered_map<std::string, std::string> kvs = {
      {"foobar", "234"},
      {"buzz", "value"},
  };
  for (const auto& kv : kvs) {
    REQUIRE(true == controller->set(kv.first, kv.second));
  }
  REQUIRE(true == controller->clear());

  SECTION("without persistence") {
  }
  SECTION("with persistence") {
    controller->persist();
    createPlan();
    plan->finalize();
    auto controller = getController();
  }

  std::unordered_map<std::string, std::string> kvs_res;
  REQUIRE(0U == kvs_res.size());

  /* Make sure we can still insert after we cleared */
  const char* key = "foo";
  const char* value = "bar";
  REQUIRE(true == controller->set(key, value));
  std::string res;
  REQUIRE(true == controller->get(key, res));
  REQUIRE(value == res);
}
