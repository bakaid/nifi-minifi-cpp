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

set( CMAKE_VERBOSE_MAKEFILE on )

include(${CMAKE_SOURCE_DIR}/extensions/ExtensionHeader.txt)

ExternalProject_Add(
        open62541-header
        PREFIX ${CMAKE_SOURCE_DIR}/open62541
        URL https://github.com/open62541/open62541/releases/download/v0.3.0/open62541.h
        DOWNLOAD_NO_EXTRACT 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

ExternalProject_Add(
        open62541-cpp
        PREFIX ${CMAKE_SOURCE_DIR}/open62541
        URL https://github.com/open62541/open62541/releases/download/v0.3.0/open62541.c
        DOWNLOAD_NO_EXTRACT 1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
)

include_directories("${CMAKE_SOURCE_DIR}/open62541/src")

include_directories(include)

file(GLOB SOURCES "src/*.cpp" "${CMAKE_SOURCE_DIR}/open62541/src/open62541.c")

add_library(minifi-opc-extensions STATIC ${SOURCES})
set_property(TARGET minifi-opc-extensions PROPERTY POSITION_INDEPENDENT_CODE ON)

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(PUBLIC minifi-opc-extensions "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries(minifi-opc-extensions "${CMAKE_THREAD_LIBS_INIT}")
endif()

target_link_libraries(minifi-opc-extensions ${CMAKE_DL_LIBS})

if (WIN32)
    set_target_properties(minifi-opc-extensions PROPERTIES
        LINK_FLAGS "/WHOLEARCHIVE"
    )
elseif (APPLE)
    set_target_properties(minifi-opc-extensions PROPERTIES
        LINK_FLAGS "-Wl,-all_load"
    )
else ()
    set_target_properties(minifi-opc-extensions PROPERTIES
        LINK_FLAGS "-Wl,--whole-archive"
    )
endif ()


SET (OPC-EXTENSIONS minifi-opc-extensions PARENT_SCOPE)

add_dependencies(minifi-opc-extensions open62541-header)
add_dependencies(minifi-opc-extensions open62541-cpp)

register_extension(minifi-opc-extensions)
