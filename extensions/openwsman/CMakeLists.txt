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

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

file(GLOB SOURCES "processors/*.cpp")

set(BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/openwsman")

# openwsman 2.6.9
set(OPENWSMAN_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/openwsman-install"
        -DBUILD_PYTHON=NO
        -DBUILD_PYTHON3=NO
        -DBUILD_LIBCIM=NO
        -DBUILD_EXAMPLES=NO
        -DBUILD_BINDINGS=NO
        -DBUILD_RUBY=NO
        -DBUILD_PERL=NO
        -DBUILD_JAVA=NO
        -DBUILD_CSHARP=NO
        -DBUILD_CUNIT_TESTS=NO
        -DDISABLE_PLUGINS=YES
        -DUSE_PAM=NO
        -DBUILD_TESTS=NO
        -DDISABLE_SERVER=YES
        -DBUILD_SHARED_LIBS=NO
    )

set(BYPRODUCTS
        "lib64/libwsman.a"
#         "lib64/libwsman_server.a"
#         "lib64/libwsman_client.a"
#         "lib64/libwsman_curl_client_transport.a"
        )

FOREACH(BYPRODUCT ${BYPRODUCTS})
    LIST(APPEND OPENWSMAN_LIBRARIES_LIST "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/openwsman-install/${BYPRODUCT}")
ENDFOREACH(b)
    
ExternalProject_Add(
            openwsman-external
            URL "https://github.com/Openwsman/openwsman/archive/v2.6.9.tar.gz"
            SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/openwsman-src"
            CMAKE_ARGS ${OPENWSMAN_CMAKE_ARGS}
            BUILD_BYPRODUCTS "${OPENWSMAN_LIBRARIES_LIST}"
    )

set(OPENWSMAN_FOUND "YES" CACHE STRING "" FORCE)
set(OPENWSMAN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/openwsman-src/include" CACHE STRING "" FORCE)
set(OPENWSMAN_LIBRARIES "${OPENWSMAN_LIBRARIES_LIST}" CACHE STRING "" FORCE)
message("OPENWSMAN_LIBRARIES: ${OPENWSMAN_LIBRARIES}")

add_library(minifi-openwsman STATIC ${SOURCES})
set_property(TARGET minifi-openwsman PROPERTY POSITION_INDEPENDENT_CODE ON)
if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(PUBLIC minifi-openwsman "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(minifi-openwsman "${CMAKE_THREAD_LIBS_INIT}")
endif()

# Include UUID
find_package(UUID REQUIRED)
target_link_libraries(minifi-openwsman ${LIBMINIFI} ${UUID_LIBRARIES})

# Include OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Include CivetWeb
target_link_libraries(minifi-openwsman civetweb-cpp c-library)
include_directories(${CIVET_THIRDPARTY_ROOT}/include)

# Include LibXML2
find_package(LibXml2 REQUIRED)
target_link_libraries(minifi-openwsman ${LIBXML2_LIBRARIES})
include_directories(${LIBXML2_LIBRARIES})

# Include Openwsman
add_dependencies(minifi-openwsman openwsman-external)
target_link_libraries(minifi-openwsman ${OPENWSMAN_LIBRARIES})
include_directories(${OPENWSMAN_INCLUDE_DIR})

if (WIN32)
message("${OPENSSL_LIBRARIES}")
    set (WIN32_ARCHIVES "")
        foreach(OPENSSL_LIB ${OPENSSL_LIBRARIES})
        if (WIN32)
        if (OPENSSL_LIB MATCHES "\\.lib$" OR OPENSSL_LIB MATCHES "\\.dll$" )
                message( FATAL "Including ${OPENSSL_LIB}")
        set_target_properties(minifi-openwsman PROPERTIES LINK_FLAGS "${LINK_FLAGS} /WHOLEARCHIVE:${OPENSSL_LIB}")
        endif()
    endif()
    endforeach()

elseif (APPLE)
    target_link_libraries(minifi-openwsman ${OPENSSL_LIBRARIES})
    set_target_properties(minifi-openwsman PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    message("${OPENSSL_LIBRARIES}")
    foreach(OPENSSL_LIB ${OPENSSL_LIBRARIES})
        target_link_libraries (minifi-openwsman ${OPENSSL_LIB})
    endforeach()
    set_target_properties(minifi-openwsman PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()

# add_executable(openwsman-server "poc/OpenwsmanServer.cpp")
# target_link_libraries(openwsman-server ${OPENWSMAN_LIBRARIES} civetweb-cpp c-library xml2)

SET (OPENWSMAN-EXTENSION minifi-openwsman PARENT_SCOPE)
register_extension(minifi-openwsman)
