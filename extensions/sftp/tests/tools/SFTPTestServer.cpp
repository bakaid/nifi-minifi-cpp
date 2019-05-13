/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SFTPTestServer.h"
#include <vector>
#include <thread>
#include <exception>

#ifdef WIN32
#else
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif

#include "utils/file/FileUtils.h"

SFTPTestServer::SFTPTestServer(const std::string& working_directory,
    const std::string& host_key_file /*= "resources/host.pem"*/,
    const std::string& jar_path /*= "tools/sftp-test-server/target/SFTPTestServer-1.0.0.jar"*/)
 : working_directory_(working_directory)
 , started_(false)
 , port_(0U)
#ifdef WIN32
#else
 , server_pid_(-1)
#endif
{
  auto executable_dir = utils::file::FileUtils::get_executable_dir();
  host_key_file_ = utils::file::FileUtils::concat_path(executable_dir, host_key_file);
  jar_path_ = utils::file::FileUtils::concat_path(executable_dir, jar_path);
}

SFTPTestServer::~SFTPTestServer()
{
  try {
    this->stop();
  } catch (...) {
  }
}

bool SFTPTestServer::start() {
  if (started_) {
    return true;
  }
#ifdef WIN32
  throw std::runtime_error("Not implemented");
#else
  /* Delete possible previous port.txt */
  port_file_path_ = utils::file::FileUtils::concat_path(working_directory_, "port.txt");
  if (!port_file_path_.empty()) {
    ::unlink(port_file_path_.c_str());
  }

  /* fork */
  pid_t pid = fork();
  if (pid == 0) {
    /* execv */
    std::vector<char*> args(4U);
    args[0] = strdup("/bin/sh");
    args[1] = strdup("-c");
    args[2] = strdup(("java -jar " + jar_path_ + " -w " + working_directory_ + " -k " + host_key_file_).c_str());
    args[3] = nullptr;
    execv("/bin/sh", args.data());
    std::cerr << "Failed to start server, errno: " << strerror(errno) << std::endl;
    exit(-1);
  } else if (pid < 0) {
    return false;
  } else {
    server_pid_ = pid;

    /* Wait for port.txt to be created */
    for (size_t i = 0; i < 10; i++) {
      std::ifstream port_file(port_file_path_);
      if (port_file.is_open() && port_file.good()) {
        uint16_t port;
        if (port_file >> port) {
          port_ = port;
          started_ = true;
          return true;
        }
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
#endif
  return false;
}

bool SFTPTestServer::stop() {
  if (!started_) {
    return true;
  }
#ifdef WIN32
  throw std::runtime_error("Not implemented");
#else
  if (server_pid_ != -1) {
    if (::kill(server_pid_, SIGTERM) != 0) {
      return false;
    }
  }
  if (!port_file_path_.empty()) {
    ::unlink(port_file_path_.c_str());
  }
#endif
  started_ = false;
  return true;
}

uint16_t SFTPTestServer::getPort() {
  return port_;
}
