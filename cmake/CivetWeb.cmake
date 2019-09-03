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

    if (WIN32)
        set(BYPRODUCTS
                "lib/libcivetweb.lib"
                "lib/libcivetweb-cpp.lib"
                )
    else()
        set(BYPRODUCTS
                "lib/libcivetweb.a"
                "lib/libcivetweb-cpp.a"
                )
    endif()

    FOREACH(BYPRODUCT ${BYPRODUCTS})
        LIST(APPEND CIVETWEB_LIBRARIES_LIST "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/civetweb-install/${BYPRODUCT}")
    ENDFOREACH(BYPRODUCT)

    if (WIN32)
    else()
        set(PC patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/civetweb/civetweb.patch)
    endif()

    set(CIVETWEB_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/civetweb-install/" CACHE STRING "" FORCE)

    set(CIVETWEB_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${CIVETWEB_BIN_DIR}"
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCIVETWEB_ENABLE_SSL_DYNAMIC_LOADING=OFF
            -DCIVETWEB_ENABLE_CXX=ON
            -DBUILD_TESTING=OFF
            -DCIVETWEB_ENABLE_ASAN=OFF # TODO
            )

    list(APPEND CMAKE_MODULE_PATH_PASSTHROUGH_LIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ssl)
    list(APPEND CIVETWEB_CMAKE_ARGS "-DLIBRESSL_BIN_DIR=${LIBRESSL_BIN_DIR}"
            "-DLIBRESSL_SRC_DIR=${LIBRESSL_SRC_DIR}"
            "-DBYPRODUCT_PREFIX=${BYPRODUCT_PREFIX}"
            "-DBYPRODUCT_SUFFIX=${BYPRODUCT_SUFFIX}")
    if(CMAKE_MODULE_PATH_PASSTHROUGH_LIST)
        string(REPLACE ";" "%" CMAKE_MODULE_PATH_PASSTHROUGH "${CMAKE_MODULE_PATH_PASSTHROUGH_LIST}")
        list(APPEND CIVETWEB_CMAKE_ARGS "-DCMAKE_MODULE_PATH=${CMAKE_MODULE_PATH_PASSTHROUGH}")
    endif()

    ExternalProject_Add(
            civetweb-external
            URL "https://github.com/civetweb/civetweb/archive/v1.10.tar.gz"
            SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/civetweb-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${CIVETWEB_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${CIVETWEB_LIBRARIES_LIST}"
    )

    add_dependencies(civetweb-external libressl-portable)

    add_library(c-library STATIC IMPORTED)
    if (WIN32)
        set_target_properties(c-library PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/lib/libcivetweb.lib")
    else()
        set_target_properties(c-library PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/lib/libcivetweb.a")
    endif()

    add_library(civetweb-cpp STATIC IMPORTED)
    if (WIN32)
        set_target_properties(civetweb-cpp PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/lib/libcivetweb-cpp.lib")
    else()
        set_target_properties(civetweb-cpp PROPERTIES IMPORTED_LOCATION "${CIVETWEB_BIN_DIR}/lib/libcivetweb-cpp.a")
    endif()

    set(CIVETWEB_FOUND "YES" CACHE STRING "" FORCE)
    set(CIVETWEB_INCLUDE_DIR "${CIVETWEB_BIN_DIR}/include" CACHE STRING "" FORCE)
    set(CIVETWEB_LIBRARIES ${OPENWSMAN_LIBRARIES_LIST} CACHE STRING "" FORCE)

    file(MAKE_DIRECTORY ${CIVETWEB_INCLUDE_DIR})

    if (OPENSSL_FOUND)
        target_link_libraries(civetweb-cpp INTERFACE "${OPENSSL_LIBRARIES}")
    endif(OPENSSL_FOUND)
    target_include_directories(c-library INTERFACE ${CIVETWEB_INCLUDE_DIR})
    target_link_libraries(civetweb-cpp INTERFACE c-library)
    add_dependencies(c-library civetweb-external)
    add_dependencies(civetweb-cpp civetweb-external)
endfunction(use_bundled_civetweb)
