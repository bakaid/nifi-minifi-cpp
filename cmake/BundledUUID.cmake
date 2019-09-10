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

function(use_bundled_uuid SOURCE_DIR BINARY_DIR)
    if (WIN32)
        set(BYPRODUCT "libuuid.lib")
    else()
        set(BYPRODUCT "libuuid.a")
    endif()

    ExternalProject_Add(
            uuid-external
            SOURCE_DIR "${SOURCE_DIR}/thirdparty/uuid"
            BINARY_DIR "${BINARY_DIR}/thirdparty/uuid"
            CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            INSTALL_COMMAND ""
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/uuid/${BYPRODUCT}"
    )

    set(UUID_FOUND "YES" CACHE STRING "" FORCE)
    if (WIN32)
        set(UUID_INCLUDE_DIR "${SOURCE_DIR}/thirdparty/uuid/include/win32" CACHE STRING "" FORCE)
    else()
        set(UUID_INCLUDE_DIR "${SOURCE_DIR}/thirdparty/uuid/include/posix" CACHE STRING "" FORCE)
    endif()
    set(UUID_LIBRARY "${BINARY_DIR}/thirdparty/uuid/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(UUID_LIBRARIES ${UUID_LIBRARY} CACHE STRING "" FORCE)

    add_library(uuid STATIC IMPORTED)
    set_target_properties(uuid PROPERTIES IMPORTED_LOCATION "${UUID_LIBRARY}")
    add_dependencies(uuid uuid-external)
    set_property(TARGET uuid APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${UUID_INCLUDE_DIR})
endfunction(use_bundled_uuid)
