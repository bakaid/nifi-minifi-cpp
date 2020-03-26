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

function(use_bundled_libarchive SOURCE_DIR BINARY_DIR)
    # Define patch step
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/libarchive/libarchive.patch")

    # Define byproducts
    if (WIN32)
        set(BYPRODUCT "lib/libarchive.lib")
    else()
        set(BYPRODUCT "lib/libarchive.a")
    endif()

    # Set build options
    set(LIBARCHIVE_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
            "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/libarchive-install"
            -DENABLE_MBEDTLS=OFF
            -DENABLE_NETTLE=OFF
            -DENABLE_OPENSSL=ON
            -DENABLE_LIBB2=OFF
            -DENABLE_LZ4=OFF
            -DENABLE_LZO=OFF
            -DENABLE_LZMA=ON
            -DENABLE_ZSTD=OFF
            -DENABLE_ZLIB=ON
            -DENABLE_BZip2=ON
            -DENABLE_LIBXML2=OFF
            -DENABLE_EXPAT=OFF
            -DENABLE_PCREPOSIX=OFF
            -DENABLE_TAR=ON
            -DENABLE_CPIO=OFF
            -DENABLE_CAT=ON
            -DENABLE_XATTR=ON
            -DENABLE_ACL=ON
            -DENABLE_ICONV=OFF
            -DENABLE_TEST=OFF)

    append_third_party_passthrough_args(LIBARCHIVE_CMAKE_ARGS "${LIBARCHIVE_CMAKE_ARGS}")

    message("LIBARCHIVE_CMAKE_ARGS: ${LIBARCHIVE_CMAKE_ARGS}")

    # Build project
    ExternalProject_Add(
            libarchive-external
            URL "https://github.com/libarchive/libarchive/releases/download/v3.4.2/libarchive-3.4.2.tar.gz"
            URL_HASH "SHA256=b60d58d12632ecf1e8fad7316dc82c6b9738a35625746b47ecdcaf4aed176176"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/libarchive-src"
            LIST_SEPARATOR % # This is needed for passing semicolon-separated lists
            CMAKE_ARGS ${LIBARCHIVE_CMAKE_ARGS}
            PATCH_COMMAND ${PC}
            BUILD_BYPRODUCTS "${BINARY_DIR}/thirdparty/libarchive-install/${BYPRODUCT}"
            EXCLUDE_FROM_ALL TRUE
    )

    # Set dependencies
    add_dependencies(libarchive-external OpenSSL::Crypto ZLIB::ZLIB LibLZMA::LibLZMA BZip2::BZip2)

    # Set variables
    set(LIBARCHIVE_FOUND "YES" CACHE STRING "" FORCE)
    set(LIBARCHIVE_INCLUDE_DIRS "${BINARY_DIR}/thirdparty/libarchive-install/include" CACHE STRING "" FORCE)
    set(LIBARCHIVE_LIBRARY "${BINARY_DIR}/thirdparty/libarchive-install/${BYPRODUCT}" CACHE STRING "" FORCE)
    set(LIBARCHIVE_LIBRARIES ${LIBARCHIVE_LIBRARY} CACHE STRING "" FORCE)

    # Create imported targets
    add_library(LibArchive::LibArchive STATIC IMPORTED)
    set_target_properties(LibArchive::LibArchive PROPERTIES IMPORTED_LOCATION "${LIBARCHIVE_LIBRARY}")
    add_dependencies(LibArchive::LibArchive libarchive-external)
    set_property(TARGET LibArchive::LibArchive APPEND PROPERTY INTERFACE_LINK_LIBRARIES OpenSSL::Crypto ZLIB::ZLIB LibLZMA::LibLZMA BZip2::BZip2)
    file(MAKE_DIRECTORY ${LIBARCHIVE_INCLUDE_DIRS})
    set_property(TARGET LibArchive::LibArchive APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LIBARCHIVE_INCLUDE_DIRS})
endfunction(use_bundled_libarchive)
