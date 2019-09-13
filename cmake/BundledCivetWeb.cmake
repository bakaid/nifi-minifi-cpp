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

function(use_bundled_civetweb SOURCE_DIR BINARY_DIR)
    message("Using bundled civetweb")

    # Patch source
    if (WIN32)
    else()
        set(PC patch -p1 < ${SOURCE_DIR}/thirdparty/civetweb/civetweb.patch)
    endif()

    # Define byproducts
    if (WIN32)
        set(SUFFIX "lib")
    else()
        set(SUFFIX "a")
    endif()

    set(BYPRODUCTS
            "lib/libcivetweb.${SUFFIX}"
            "lib/libcivetweb-cpp.${SUFFIX}"
            )

    set(CIVETWEB_BIN_DIR "${BINARY_DIR}/thirdparty/civetweb-install/" CACHE STRING "" FORCE)

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND CIVETWEB_LIBRARIES_LIST "${CIVETWEB_BIN_DIR}/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    # Set build options
    set(CIVETWEB_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${CIVETWEB_BIN_DIR}"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCIVETWEB_ENABLE_SSL_DYNAMIC_LOADING=OFF
            -DCIVETWEB_ENABLE_CXX=ON
            -DBUILD_TESTING=OFF
            -DCIVETWEB_ENABLE_ASAN=OFF # TODO
            )
    if (OPENSSL_OFF)
        list(APPEND CIVETWEB_CMAKE_ARGS -DCIVETWEB_ENABLE_SSL=OFF)
    endif()

    list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST ${SOURCE_DIR}/cmake/ssl)
    list(APPEND CIVETWEB_CMAKE_ARGS "-DLIBRESSL_BIN_DIR=${LIBRESSL_BIN_DIR}"
            "-DLIBRESSL_SRC_DIR=${LIBRESSL_SRC_DIR}"
            "-DBYPRODUCT_PREFIX=${BYPRODUCT_PREFIX}"
            "-DBYPRODUCT_SUFFIX=${BYPRODUCT_SUFFIX}")
    if(CMAKE_MODULE_PATH_PASSTHROUGH_LIST)
        string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH_PASSTHROUGH_LIST}")
        list(APPEND CIVETWEB_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    endif()

    # Build project
    ExternalProject_Add(
            civetweb-external
            URL "https://github.com/civetweb/civetweb/archive/v1.10.tar.gz"
            URL_HASH "SHA256=e6958f005aa01b02645bd3ff9760dd085e83d30530cdd97b584632419195bea5"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/civetweb-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${CIVETWEB_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${CIVETWEB_LIBRARIES_LIST}"
    )

    # Set variables
    set(CIVETWEB_FOUND "YES" CACHE STRING "" FORCE)
    set(CIVETWEB_INCLUDE_DIR "${CIVETWEB_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(CIVETWEB_LIBRARIES "${CIVETWEB_BIN_DIR}/lib/libcivetweb.${SUFFIX}" "${CIVETWEB_BIN_DIR}/lib/libcivetweb-cpp.${SUFFIX}" CACHE STRING "" FORCE)

    # Set exported variables for FindPackage.cmake
    set(EXPORT_CIVETWEB_INCLUDE_DIR "${CIVETWEB_INCLUDE_DIR}" CACHE STRING "" FORCE)
    set(EXPORT_CIVETWEB_LIBRARIES "${CIVETWEB_LIBRARIES}" CACHE STRING "" FORCE)

    # Create imported targets
    add_library(CIVETWEB::c-library STATIC IMPORTED)
    set_target_properties(CIVETWEB::c-library PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/lib/libcivetweb.${SUFFIX}")
    file(MAKE_DIRECTORY ${CIVETWEB_INCLUDE_DIR})
    set_property(TARGET CIVETWEB::c-library APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${CIVETWEB_INCLUDE_DIR})
    add_dependencies(CIVETWEB::c-library civetweb-external)
    if (NOT OPENSSL_OFF)
        add_dependencies(civetweb-external OpenSSL::SSL OpenSSL::Crypto)
        set_property(TARGET CIVETWEB::c-library APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    endif()

    add_library(CIVETWEB::civetweb-cpp STATIC IMPORTED)
    set_target_properties(CIVETWEB::civetweb-cpp PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/lib/libcivetweb-cpp.${SUFFIX}")
    set_property(TARGET CIVETWEB::civetweb-cpp APPEND PROPERTY INTERFACE_LINK_LIBRARIES CIVETWEB::c-library)
    add_dependencies(CIVETWEB::civetweb-cpp civetweb-external)
endfunction(use_bundled_civetweb)
