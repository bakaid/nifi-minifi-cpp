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
#ifndef __SFTP_CLIENT_H__
#define __SFTP_CLIENT_H__

#include <curl/curl.h>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <vector>
#include <iostream>
#include <string>
#include <uuid/uuid.h>
#ifdef WIN32
#include <regex>
#else
#include <regex.h>
#endif
#include <vector>

#include "utils/HTTPClient.h"
#include "core/logging/Logger.h"
#include "core/logging/LoggerConfiguration.h"
#include "properties/Configure.h"
#include "io/validation.h"
#include "io/BaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Initializes and cleans up libssh2 once. Cleanup will only occur at the end of our execution since we are relying on a static variable.
 */
class LibSSH2Initializer {
 public:
  static LibSSH2Initializer *getInstance() {
    static LibSSH2Initializer initializer;
    return &initializer;
  }
  void initialize() {

  }
 private:
  ~LibSSH2Initializer() {
    libssh2_exit();
  }
  LibSSH2Initializer() {
    libssh2_init(0);
  }
};

class SFTPClient {
 public:

  SFTPClient(const std::string &hostname, uint16_t port, const std::string& username);

  ~SFTPClient();

  bool setVerbose();

  bool setHostKeyFile(const std::string& host_key_file_path, bool strict_host_checking);

  void setPasswordAuthenticationCredentials(const std::string& password);

  void setPublicKeyAuthenticationCredentials(const std::string& private_key_file_path, const std::string& private_key_passphrase);

  enum class ProxyType : uint8_t {
    Http,
    Socks
  };

  bool setProxy(ProxyType type, const utils::HTTPProxy& proxy);

  bool setConnectionTimeout(int64_t timeout);

  void setDataTimeout(int64_t timeout);

  bool setSendKeepAlive(bool send_keepalive);

  bool setUseCompression(bool use_compression);

  bool connect();

  bool getFile(const std::string& path, io::BaseStream& output);

  bool putFile(const std::string& path, io::BaseStream& input, bool overwrite);

  bool rename(const std::string& source_path, const std::string& target_path, bool overwrite);

  bool createDirectoryHierarchy(const std::string& path);

  bool removeFile(const std::string& path);

  bool removeDirectory(const std::string& path);

  bool listDirectory(const std::string& path, bool follow_symlinks,
      std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>>& children_result);

 protected:

  std::shared_ptr<logging::Logger> logger_;

  const std::string hostname_;
  const uint16_t port_;
  const std::string username_;

  LIBSSH2_KNOWNHOSTS *ssh_known_hosts_;
  bool strict_host_checking_;

  bool password_authentication_enabled_;
  std::string password_;

  bool public_key_authentication_enabled_;
  std::string private_key_file_path_;
  std::string private_key_passphrase_;

  int64_t data_timeout_;

  CURL *easy_;
  LIBSSH2_SESSION *ssh_session_;
  LIBSSH2_SFTP *sftp_session_;

  bool connected_;

};

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
