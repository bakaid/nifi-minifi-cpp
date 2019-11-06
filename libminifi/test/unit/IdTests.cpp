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
#include <utility>
#include <string>
#include <memory>
#include <ctime>
#include <algorithm>
#include "../TestBase.h"
#include "utils/Id.h"

TEST_CASE("Test default is time", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(std::make_shared<minifi::Properties>());

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate_time implementation for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test time", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "TiMe");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate_time implementation for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Generate Move", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "TiMe");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  auto generated = generator->generate();
  auto str = generated.to_string();
  utils::Identifier moved = std::move(generated);
  auto str2 = moved.to_string();
  REQUIRE(str == str2);
}

TEST_CASE("Test random", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "RaNDoM");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate_random for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test uuid_default", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "UUID_default");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Using uuid_generate for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test invalid", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "InVaLiD");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  REQUIRE(true == LogTestController::getInstance().contains("Invalid value for uid.implementation (invalid). Using uuid_generate_time implementation for uids."));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Hex Device Segment 16 bits correct digits", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "minifi_uid");
  id_props->set("uid.minifi.device.segment", "09aF");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier uuid;
  generator->generate(uuid);
  auto uid = uuid.toArray();
  REQUIRE(0x09 == uid[0]);
  REQUIRE(0xaf == uid[1]);
  REQUIRE(0 == uid[15]);

  generator->generate(uuid);
  uid = uuid.toArray();
  REQUIRE(0x09 == uid[0]);
  REQUIRE(0xaf == uid[1]);
  REQUIRE(1 == uid[15]);

  REQUIRE(true == LogTestController::getInstance().contains("Using user defined device segment: 9af"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Hex Device Segment 16 bits too many digits", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "minifi_uid");
  id_props->set("uid.minifi.device.segment", "09aFee");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier uuid;
  generator->generate(uuid);
  auto uid = uuid.toArray();
  REQUIRE(0x09 == uid[0]);
  REQUIRE(0xaf == uid[1]);
  REQUIRE(0 == (uid[2] & 128));
  REQUIRE(0 == uid[15]);

  generator->generate(uuid);
  uid = uuid.toArray();
  REQUIRE(0x09 == uid[0]);
  REQUIRE(0xaf == uid[1]);
  REQUIRE(0 == (uid[2] & 128));
  REQUIRE(1 == uid[15]);

  REQUIRE(true == LogTestController::getInstance().contains("Using user defined device segment: 9af"));
  LogTestController::getInstance().reset();
}

TEST_CASE("Test Hex Device Segment 18 bits", "[id]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  id_props->set("uid.implementation", "minifi_uid");
  id_props->set("uid.minifi.device.segment.bits", "18");
  id_props->set("uid.minifi.device.segment", "09aF8");

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  utils::Identifier uuid;
  generator->generate(uuid);
  auto uid = uuid.toArray();
  REQUIRE(0x09 == uid[0]);
  REQUIRE(0xaf == uid[1]);
  REQUIRE(128 == (uid[2] & 192));
  REQUIRE(0 == uid[15]);

  generator->generate(uuid);
  uid = uuid.toArray();
  REQUIRE(0x09 == uid[0]);
  REQUIRE(0xaf == uid[1]);
  REQUIRE(128 == (uid[2] & 192));
  REQUIRE(1 == uid[15]);

  utils::Identifier uuid2;
  generator->generate(uuid2);
  REQUIRE(uuid.to_string() != uuid2.to_string());
  REQUIRE(uuid != uuid2);

  REQUIRE(true == LogTestController::getInstance().contains("Using minifi uid prefix: 9af8"));
  LogTestController::getInstance().reset();
}


TEST_CASE("Collision", "[collision]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  SECTION("random") {
    id_props->set("uid.implementation", "random");
  }
  SECTION("time") {
    id_props->set("uid.implementation", "time");
  }
  SECTION("uuid_default") {
    id_props->set("uid.implementation", "uuid_default");
  }

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  std::vector<utils::Identifier> uuids(16 * 1024U);
  std::vector<std::thread> threads;
  for (size_t i = 0U; i < 16U; i++) {
    threads.emplace_back([&, i](){
      for (size_t j = 0U; j < 1024U; j++) {
        generator->generate(uuids[i * 1024U + j]);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }

  std::sort(uuids.begin(), uuids.end(), [](const utils::Identifier& a, const utils::Identifier& b) {
    return memcmp(a.toArray(), b.toArray(), 16U) < 0;
  });
  REQUIRE(uuids.end() == std::adjacent_find(uuids.begin(), uuids.end()));

  LogTestController::getInstance().reset();
}

TEST_CASE("Speed", "[speed]") {
  TestController test_controller;

  LogTestController::getInstance().setDebug<utils::IdGenerator>();
  std::shared_ptr<minifi::Properties> id_props = std::make_shared<minifi::Properties>();
  std::string implementation;
  SECTION("random") {
    implementation = "random";
  }
  SECTION("time") {
    implementation = "time";
  }
  SECTION("uuid_default") {
    implementation = "uuid_default";
  }
  id_props->set("uid.implementation", implementation);

  std::shared_ptr<utils::IdGenerator> generator = utils::IdGenerator::getIdGenerator();
  generator->initialize(id_props);

  std::vector<utils::Identifier> uuids(128U * 1024U);
  // Prime the generator
  generator->generate(uuids[0]);

  auto before = std::chrono::high_resolution_clock::now();
  for (size_t i = 0U; i < uuids.size(); i++) {
    generator->generate(uuids[i]);
  }
  auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - before).count();
  std::cerr << "Generating one " << implementation << " UUID took " << (duration / uuids.size()) << "ns" << std::endl;

  LogTestController::getInstance().reset();
}
