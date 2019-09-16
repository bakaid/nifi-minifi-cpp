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

function(use_bundled_rocksdb SOURCE_DIR BINARY_DIR)
    message("Using bundled RocksDB")

    if (WIN32)
        set(BYPRODUCT "lib/librocksdb.lib")
    else()
        set(BYPRODUCT "lib/librocksdb.a")
    endif()

    if (WIN32)
    else()
        set(PC patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rocksdb/rocksdb-BUILD.patch)
    endif()

    set(ROCKSDB_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/rocksdb-install"
            -DWITH_TESTS=OFF
            -DWITH_TOOLS=OFF
            -DFAIL_ON_WARNINGS=OFF)
    if (PORTABLE)
        list(APPEND ROCKSDB_CMAKE_ARGS -DPORTABLE=ON)
    endif()

    ExternalProject_Add(
            rocksdb-external
            GIT_REPOSITORY "https://github.com/facebook/rocksdb.git"
            GIT_TAG "v5.8.6"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/rocksdb-src"
            CMAKE_ARGS ${ROCKSDB_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/rocksdb-install/${BYPRODUCT}"
    )

    set(ROCKSDB_FOUND "YES" CACHE STRING "" FORCE)
    set(ROCKSDB_INCLUDE_DIR "${BINARY_DIR}/thirdparty/rocksdb-install/include" CACHE STRING "" FORCE)
    set(ROCKSDB_LIBRARY "${BINARY_DIR}/thirdparty/rocksdb-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(ROCKSDB_LIBRARIES ${ROCKSDB_LIBRARY} CACHE STRING "" FORCE)

    add_library(RocksDB::RocksDB STATIC IMPORTED)
    set_target_properties(RocksDB::RocksDB PROPERTIES IMPORTED_LOCATION "${ROCKSDB_LIBRARY}")
    add_dependencies(RocksDB::RocksDB rocksdb-external)
    file(MAKE_DIRECTORY ${ROCKSDB_INCLUDE_DIR})
    set_property(TARGET RocksDB::RocksDB APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ROCKSDB_INCLUDE_DIR})
endfunction(use_bundled_rocksdb)
