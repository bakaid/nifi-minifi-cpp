#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

file(GLOB KEYVALUE_TESTS  "*.cpp")

SET(KEYVALUE_INT_TEST_COUNT 0)

FOREACH(testfile ${KEYVALUE_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}" )
    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/libminifi/test/")
#    target_include_directories(${testfilename} PRIVATE BEFORE "${CMAKE_SOURCE_DIR}/extensions/pcap/")
    createTests("${testfilename}")
    if(APPLE)
        target_link_libraries ("${testfilename}" -Wl,-all_load minifi-standard-processors)
    else()
        target_link_libraries ("${testfilename}" -Wl,--whole-archive minifi-standard-processors -Wl,--no-whole-archive)
    endif()
    if (DISABLE_ROCKSDB STREQUAL "OFF" OR NOT DISABLE_ROCKSDB)
        if(APPLE)
            target_link_libraries ("${testfilename}" -Wl,-all_load minifi-rocksdb-repos)
        else()
            target_link_libraries ("${testfilename}" -Wl,--whole-archive minifi-rocksdb-repos -Wl,--no-whole-archive)
        endif()
    endif()
    MATH(EXPR KEYVALUE_INT_TEST_COUNT "${KEYVALUE_INT_TEST_COUNT}+1")
ENDFOREACH()

message("-- Finished building ${KEYVALUE_INT_TEST_COUNT} keyvalue controller test file(s)...")

add_test(NAME UnorderedMapPersistableKeyValueStoreServiceTest COMMAND PersistableKeyValueStoreServiceTest "--config-yaml ${TEST_RESOURCES}/UnorderedMapPersistableKeyValueStoreServiceTest.yml")
if (DISABLE_ROCKSDB STREQUAL "OFF" OR NOT DISABLE_ROCKSDB)
    add_test(NAME RocksdDbPersistableKeyValueStoreServiceTest COMMAND PersistableKeyValueStoreServiceTest "--config-yaml ${TEST_RESOURCES}/RocksdDbPersistableKeyValueStoreServiceTest.yml")
endif()