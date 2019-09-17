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

function(use_bundled_librdkafka SOURCE_DIR BINARY_DIR)
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/rdkafka.lib")
    else()
        set(BYPRODUCT "lib/librdkafka.a")
    endif()

    # Set build options
    set(LIBRDKAFKA_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/librdkafka-install"
            "-DWITH_SASL=OFF"
            "-DOPENSSL_VERSION=1.0.2"
            "-DRDKAFKA_BUILD_STATIC=ON"
            "-DRDKAFKA_BUILD_EXAMPLES=OFF"
            "-DRDKAFKA_BUILD_TESTS=OFF"
            "-DENABLE_LZ4_EXT=OFF"
            "-DWITH_ZSTD=OFF"
            "-DCMAKE_INSTALL_LIBDIR=lib")

    string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH}")
    list(APPEND LIBRDKAFKA_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    list(APPEND LIBRDKAFKA_CMAKE_ARGS ${PASSTHROUGH_VARIABLES})

    # Build project
    ExternalProject_Add(
            kafka-external
            GIT_REPOSITORY "https://github.com/edenhill/librdkafka.git"
            GIT_TAG "v1.0.1"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${LIBRDKAFKA_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/librdkafka-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(kafka-external OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)

    # Set variables
    set(LIBRDKAFKA_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBRDKAFKA_INCLUDE_DIR "${BINARY_DIR}/thirdparty/librdkafka-install/include/librdkafka" CACHE STRING "" FORCE)
    set(LIBRDKAFKA_LIBRARY "${BINARY_DIR}/thirdparty/librdkafka-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBRDKAFKA_LIBRARIES ${LIBRDKAFKA_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(librdkafka STATIC IMPORTED)
    set_target_properties(librdkafka PROPERTIES IMPORTED_LOCATION "${LIBRDKAFKA_LIBRARY}")
    add_dependencies(librdkafka kafka-external)
    set_property(TARGET librdkafka APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
    file(MAKE_DIRECTORY ${LIBRDKAFKA_INCLUDE_DIR})
    set_property(TARGET librdkafka APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBRDKAFKA_INCLUDE_DIR})
endfunction(use_bundled_librdkafka)