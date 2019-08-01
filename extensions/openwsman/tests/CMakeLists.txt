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


file(GLOB OPENWSMAN_INTEGRATION_TESTS "*.cpp")
SET(OPENWSMAN-EXTENSIONS_TEST_COUNT 0)
FOREACH(testfile ${OPENWSMAN_INTEGRATION_TESTS})
    get_filename_component(testfilename "${testfile}" NAME_WE)
    add_executable("${testfilename}" "${testfile}")
    target_include_directories(${testfilename} BEFORE PRIVATE ${CURL_INCLUDE_DIRS})
    target_include_directories(${testfilename} BEFORE PRIVATE ${LIBSSH2_INCLUDE_DIR})
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/standard-processors/processors")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/libminifi/test/")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/nanofi/include")
    target_include_directories(${testfilename} BEFORE PRIVATE "${CMAKE_SOURCE_DIR}/extensions/openwsman")
    target_include_directories(${testfilename} BEFORE PRIVATE "${OPENWSMAN_INCLUDE_DIR}")
    target_include_directories(${testfilename} BEFORE PRIVATE "../")
    target_include_directories(${testfilename} BEFORE PRIVATE ./include)
    target_include_directories(${testfilename} BEFORE PRIVATE ./tools)

    if (APPLE)
        target_link_libraries (${testfilename} -Wl,-all_load ${OPENSSL_LIBRARIES} ${LIBXML2_LIBRARIES} minifi-openwsman minifi-standard-processors)
    else ()
        target_link_libraries (${testfilename} -Wl,--whole-archive ${OPENSSL_LIBRARIES} ${LIBXML2_LIBRARIES} minifi-openwsman minifi-standard-processors -Wl,--no-whole-archive)
    endif ()

    createTests("${testfilename}")
    target_link_libraries(${testfilename} ${CATCH_MAIN_LIB})
    MATH(EXPR OPENWSMAN-EXTENSIONS_TEST_COUNT "${OPENWSMAN-EXTENSIONS_TEST_COUNT}+1")
    add_test(NAME "${testfilename}" COMMAND "${testfilename}" WORKING_DIRECTORY ${TEST_DIR})
ENDFOREACH()
message("-- Finished building ${OPENWSMAN-EXTENSIONS_TEST_COUNT} OPENWSMAN related test file(s)...")
