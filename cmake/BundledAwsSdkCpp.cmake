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

function(use_bundled_libaws SOURCE_DIR BINARY_DIR)
    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
    else()
        set(SUFFIX "a")
    endif()
    set(BYPRODUCTS
            "lib/libaws-c-common.${SUFFIX}"
            "lib/libaws-checksums.${SUFFIX}"
            "lib/libaws-c-event-stream.${SUFFIX}"
            "lib/libaws-cpp-sdk-core.${SUFFIX}"
            "lib/libaws-cpp-sdk-s3.${SUFFIX}")

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND LIBAWS_LIBRARIES_LIST "${BINARY_DIR}/thirdparty/libaws-install/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(LIBAWS_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            -DCMAKE_PREFIX_PATH=${BINARY_DIR}/thirdparty/libaws-install
            -DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libaws-install
            -DBUILD_ONLY=s3
            -DENABLE_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DENABLE_UNITY_BUILD=ON)

    string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH}")
    list(APPEND LIBAWS_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    list(APPEND LIBAWS_CMAKE_ARGS ${PASSTHROUGH_VARIABLES})

    # Build project
    ExternalProject_Add(
            awssdk-external
            URL "https://github.com/aws/aws-sdk-cpp/archive/1.7.109.tar.gz"
            URL_HASH "SHA256=72dc9ee89787db3dd12ba944e8d2576c06290366124048a9a72a369114487cd1"
            EXCLUDE_FROM_ALL TRUE
            SOURCE_DIR "${BINARY_DIR}/thirdparty/libaws-src"
            INSTALL_DIR "${BINARY_DIR}/thirdparty/libaws-install"
            CMAKE_ARGS ${LIBAWS_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${LIBAWS_LIBRARIES_LIST}"
    )

    # Set dependencies
    add_dependencies(awssdk-external CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)

    # Set variables
    set(LIBAWS_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBAWS_INCLUDE_DIR "${BINARY_DIR}/thirdparty/libaws-install/include" CACHE STRING "" FORCE)
    set(LIBAWS_LIBRARIES ${LIBAWS_LIBRARIES_LIST} CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${LIBAWS_INCLUDE_DIR})

    add_library(AWS::libaws-c-common STATIC IMPORTED)
    set_target_properties(AWS::libaws-c-common PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/lib/libaws-c-common.${SUFFIX}")
    add_dependencies(AWS::libaws-c-common awssdk-external)
    set_property(TARGET AWS::libaws-c-common APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::libaws-c-common APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    if (APPLE)
        set_property(TARGET AWS::libaws-c-common APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::libaws-checksums STATIC IMPORTED)
    set_target_properties(AWS::libaws-checksums PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/lib/libaws-checksums.${SUFFIX}")
    add_dependencies(AWS::libaws-checksums awssdk-external)
    set_property(TARGET AWS::libaws-checksums APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::libaws-checksums APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    if (APPLE)
        set_property(TARGET AWS::libaws-checksums APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::libaws-c-event-stream STATIC IMPORTED)
    set_target_properties(AWS::libaws-c-event-stream PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/lib/libaws-c-event-stream.${SUFFIX}")
    add_dependencies(AWS::libaws-c-event-stream awssdk-external)
    set_property(TARGET AWS::libaws-c-event-stream APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::libaws-c-event-stream APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    if (APPLE)
        set_property(TARGET AWS::libaws-c-event-stream APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::libaws-cpp-sdk-core STATIC IMPORTED)
    set_target_properties(AWS::libaws-cpp-sdk-core PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/lib/libaws-cpp-sdk-core.${SUFFIX}")
    add_dependencies(AWS::libaws-cpp-sdk-core awssdk-external)
    set_property(TARGET AWS::libaws-cpp-sdk-core APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::libaws-cpp-sdk-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    if (APPLE)
        set_property(TARGET AWS::libaws-cpp-sdk-core APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()

    add_library(AWS::libaws-cpp-sdk-s3 STATIC IMPORTED)
    set_target_properties(AWS::libaws-cpp-sdk-s3 PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/libaws-install/lib/libaws-cpp-sdk-s3.${SUFFIX}")
    add_dependencies(AWS::libaws-cpp-sdk-s3 awssdk-external)
    set_property(TARGET AWS::libaws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBAWS_INCLUDE_DIR})
    set_property(TARGET AWS::libaws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES CURL::libcurl OpenSSL::Crypto OpenSSL::SSL ZLIB::ZLIB)
    if (APPLE)
        set_property(TARGET AWS::libaws-cpp-sdk-s3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES "-framework CoreFoundation")
    endif()
endfunction(use_bundled_libaws)
