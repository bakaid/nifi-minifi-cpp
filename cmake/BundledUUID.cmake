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
    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/uuid.lib")
    else()
        set(BYPRODUCT "lib/libuuid.a")
    endif()

    # Set build options
    set(UUID_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/uuid-install")

    # Build project
    ExternalProject_Add(
            uuid-external
            SOURCE_DIR "${SOURCE_DIR}/thirdparty/uuid"
            BINARY_DIR "${BINARY_DIR}/thirdparty/uuid"
            CMAKE_ARGS ${UUID_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/uuid-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set variables
    set(UUID_FOUND "YES" CACHE STRING "" FORCE)
    set(UUID_INCLUDE_DIR "${BINARY_DIR}/thirdparty/uuid-install/include" CACHE STRING "" FORCE)
    set(UUID_LIBRARY "${BINARY_DIR}/thirdparty/uuid-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(UUID_LIBRARIES ${UUID_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    file(MAKE_DIRECTORY ${UUID_INCLUDE_DIR})

    add_library(uuid STATIC IMPORTED)
    set_target_properties(uuid PROPERTIES IMPORTED_LOCATION "${UUID_LIBRARY}")
    add_dependencies(uuid uuid-external)
    set_property(TARGET uuid APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${UUID_INCLUDE_DIR})
endfunction(use_bundled_uuid)
