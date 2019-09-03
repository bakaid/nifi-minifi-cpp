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

function(use_bundled_curl SOURCE_DIR BINARY_DIR)
    get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

    if ("${LIB64}" STREQUAL "TRUE" AND (NOT WIN32 AND NOT APPLE))
        set(LIBSUFFIX 64)
    else()
        set(LIBSUFFIX "")
    endif()

    if (WIN32)
        set(BYPRODUCT "lib/libcurl.lib")
    else()
        set(BYPRODUCT "lib${LIBSUFFIX}/libcurl.a")
    endif()

    if (WIN32)
        set (PC "PATCH_COMMAND ./buildconf.bat")
    endif()

    set(CURL_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DBUILD_CURL_EXE=OFF
            -DBUILD_TESTING=OFF
            -DBUILD_SHARED_LIBS=OFF
            -DHTTP_ONLY=ON
            -DCMAKE_USE_OPENSSL=ON
            -DCURL_DISABLE_CRYPTO_AUTH=ON
            -DCMAKE_USE_LIBSSH2=OFF
            "-DCMAKE_DEBUG_POSTFIX="
            -DHAVE_GLIBC_STRERROR_R=1
            -DHAVE_GLIBC_STRERROR_R__TRYRUN_OUTPUT=""
            -DHAVE_POSIX_STRERROR_R=0
            -DHAVE_POSIX_STRERROR_R__TRYRUN_OUTPUT=""
            -DHAVE_POLL_FINE_EXITCODE=0
            -DHAVE_FSETXATTR_5=0
            -DHAVE_FSETXATTR_5__TRYRUN_OUTPUT=""
            "-DCMAKE_C_FLAGS=${CURL_C_FLAGS}"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DCMAKE_CXX_FLAGS=${CURL_CXX_FLAGS}"
            )

    list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl)
    list(APPEND CURL_CMAKE_ARGS "-DLIBRESSL_BIN_DIR=${LIBRESSL_BIN_DIR}"
            "-DLIBRESSL_SRC_DIR=${LIBRESSL_SRC_DIR}"
            "-DBYPRODUCT_PREFIX=${BYPRODUCT_PREFIX}"
            "-DBYPRODUCT_SUFFIX=${BYPRODUCT_SUFFIX}")
    if(CMAKE_MODULE_PATH_PASSTHROUGH_LIST)
        string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH_PASSTHROUGH_LIST}")
        list(APPEND CURL_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    endif()

    ExternalProject_Add(
            curl-external
            GIT_REPOSITORY "https://github.com/curl/curl.git"
            GIT_TAG "curl-7_64_0"
            SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-src"
            CMAKE_ARGS ${CURL_CMAKE_ARGS}
            ${PC}
            BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install/${BYPRODUCT}"
    )

    add_dependencies(curl-external OpenSSL::SSL OpenSSL::Crypto)

    set(CURL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl/" CACHE STRING "" FORCE)
    set(CURL_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/curl-install/" CACHE STRING "" FORCE)
    set(CURL_BYPRODUCT_DIR "${BYPRODUCT}" CACHE STRING "" FORCE)

    add_library(curl STATIC IMPORTED)
    set_target_properties(curl PROPERTIES IMPORTED_LOCATION "${CURL_BIN_DIR}${BYPRODUCT}")

    set(CURL_FOUND "YES")
    set(CURL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl/include")
    set(CURL_LIBRARY "${CURL_BIN_DIR}${BYPRODUCT}" CACHE STRING "" FORCE)
    set(CURL_LIBRARIES "${CURL_LIBRARY}" CACHE STRING "" FORCE)

    add_library(CURL::libcurl STATIC IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES IMPORTED_LOCATION "${CURL_LIBRARY}")
    add_dependencies(CURL::libcurl curl-external)
    file(MAKE_DIRECTORY ${CURL_INCLUDE_DIRS})
    target_include_directories(CURL::libcurl INTERFACE ${CURL_INCLUDE_DIRS})
endfunction(use_bundled_curl SOURCE_DIR BINARY_DIR)
