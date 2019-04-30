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
#include "SFTPClient.h"
#include <memory>
#include <set>
#include <vector>
#include <string>
#include <exception>
#include <sstream>
#include <iomanip>
#include "utils/StringUtils.h"
#include "utils/ScopeGuard.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

#define SFTP_ERROR(CODE) case CODE: \
                          return #CODE
static const char* sftp_strerror(unsigned long err) {
  switch (err) {
    SFTP_ERROR(LIBSSH2_FX_OK);
    SFTP_ERROR(LIBSSH2_FX_EOF);
    SFTP_ERROR(LIBSSH2_FX_NO_SUCH_FILE);
    SFTP_ERROR(LIBSSH2_FX_PERMISSION_DENIED);
    SFTP_ERROR(LIBSSH2_FX_FAILURE);
    SFTP_ERROR(LIBSSH2_FX_BAD_MESSAGE);
    SFTP_ERROR(LIBSSH2_FX_NO_CONNECTION);
    SFTP_ERROR(LIBSSH2_FX_CONNECTION_LOST);
    SFTP_ERROR(LIBSSH2_FX_OP_UNSUPPORTED);
    SFTP_ERROR(LIBSSH2_FX_INVALID_HANDLE);
    SFTP_ERROR(LIBSSH2_FX_NO_SUCH_PATH);
    SFTP_ERROR(LIBSSH2_FX_FILE_ALREADY_EXISTS);
    SFTP_ERROR(LIBSSH2_FX_WRITE_PROTECT);
    SFTP_ERROR(LIBSSH2_FX_NO_MEDIA);
    SFTP_ERROR(LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM);
    SFTP_ERROR(LIBSSH2_FX_QUOTA_EXCEEDED);
    SFTP_ERROR(LIBSSH2_FX_UNKNOWN_PRINCIPAL);
    SFTP_ERROR(LIBSSH2_FX_LOCK_CONFLICT);
    SFTP_ERROR(LIBSSH2_FX_DIR_NOT_EMPTY);
    SFTP_ERROR(LIBSSH2_FX_NOT_A_DIRECTORY);
    SFTP_ERROR(LIBSSH2_FX_INVALID_FILENAME);
    SFTP_ERROR(LIBSSH2_FX_LINK_LOOP);
    default:
      return "Unknown error";
  }
}

SFTPClient::SFTPClient(const std::string &hostname, uint16_t port, const std::string& username)
    : logger_(logging::LoggerFactory<SFTPClient>::getLogger()),
      hostname_(hostname),
      port_(port),
      username_(username),
      ssh_known_hosts_(nullptr),
      strict_host_checking_(false),
      password_authentication_enabled_(false),
      public_key_authentication_enabled_(false),
      data_timeout_(0),
      easy_(nullptr),
      ssh_session_(nullptr),
      sftp_session_(nullptr),
      connected_(false) {
  curl_global_init(CURL_GLOBAL_DEFAULT); // TODO
  LibSSH2Initializer::getInstance()->initialize();
  easy_ = curl_easy_init();
  if (easy_ == nullptr) {
    throw std::runtime_error("Cannot create curl easy handle");
  }
  ssh_session_ = libssh2_session_init();
  if (ssh_session_ == nullptr) {
    curl_easy_cleanup(easy_);
    throw std::runtime_error("Cannot create ssh session handler");
  }
}

SFTPClient::~SFTPClient() {
    if (sftp_session_ != nullptr) {
      libssh2_sftp_shutdown(sftp_session_);
    }
    if (ssh_known_hosts_ != nullptr) {
      libssh2_knownhost_free(ssh_known_hosts_);
    }
    if (ssh_session_ != nullptr) {
      libssh2_session_disconnect(ssh_session_, "Normal Shutdown");
      libssh2_session_free(ssh_session_);
    }
    if (easy_ != nullptr) {
      curl_easy_cleanup(easy_);
    }
  logger_->log_trace("Closing SFTPClient for %s:%hu", hostname_, port_);
}

bool SFTPClient::setVerbose() {
  if (curl_easy_setopt(easy_, CURLOPT_VERBOSE, 1L) != CURLE_OK) {
    return false;
  }
  return true;
}

bool SFTPClient::setHostKeyFile(const std::string& host_key_file_path, bool strict_host_checking) {
  if (ssh_known_hosts_ != nullptr) {
    return false;
  }
  ssh_known_hosts_ = libssh2_knownhost_init(ssh_session_);
  if (ssh_known_hosts_ == nullptr) {
    return false;
  }
  if (libssh2_knownhost_readfile(ssh_known_hosts_, host_key_file_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH) <= 0) {
    return false;
  }
  strict_host_checking_ = strict_host_checking;
  return true;
}

void SFTPClient::setPasswordAuthenticationCredentials(const std::string& password) {
  password_authentication_enabled_ = true;
  password_ = password;
}

void SFTPClient::setPublicKeyAuthenticationCredentials(const std::string& private_key_file_path, const std::string& private_key_passphrase) {
  public_key_authentication_enabled_ = true;
  private_key_file_path_ = private_key_file_path;
  private_key_passphrase_ = private_key_passphrase;
}

bool SFTPClient::setProxy(ProxyType type, const utils::HTTPProxy& proxy) {
  switch (type) {
    case ProxyType::Http:
      if (curl_easy_setopt(easy_, CURLOPT_PROXYTYPE, CURLPROXY_HTTP) != CURLE_OK) {
        return false;
      }
      if (curl_easy_setopt(easy_, CURLOPT_HTTPPROXYTUNNEL, 1L) != CURLE_OK) {
        return false;
      }
      break;
    case ProxyType::Socks:
      if (curl_easy_setopt(easy_, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5) != CURLE_OK) {
        return false;
      }
      break;
  }
  std::stringstream proxy_string;
  proxy_string << proxy.host << ":" << proxy.port;
  if (curl_easy_setopt(easy_, CURLOPT_PROXY, proxy_string.str().c_str()) != CURLE_OK) {
    return false;
  }
  return true;
}

bool SFTPClient::setConnectionTimeout(int64_t timeout) {
  if (curl_easy_setopt(easy_, CURLOPT_CONNECTTIMEOUT, timeout) != CURLE_OK) {
    return false;
  }
  return true;
}

void SFTPClient::setDataTimeout(int64_t timeout) {
  data_timeout_ = timeout;
  libssh2_session_set_timeout(ssh_session_, timeout);
}

bool SFTPClient::setSendKeepAlive(bool send_keepalive) {
  return true; // TODO
}

bool SFTPClient::setUseCompression(bool use_compression) {
  if (libssh2_session_flag(ssh_session_, LIBSSH2_FLAG_COMPRESS, 1) != 0) {
    return false;
  }
  return true;
}

bool SFTPClient::connect() {
  if (connected_) {
    return true;
  }

  /* Setting up curl request */
  std::stringstream uri;
  uri << hostname_ << ":" << port_;
  if (curl_easy_setopt(easy_, CURLOPT_URL, uri.str().c_str()) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_NOSIGNAL, 1L) != CURLE_OK) {
    return false;
  }
  if (curl_easy_setopt(easy_, CURLOPT_CONNECT_ONLY, 1L) != CURLE_OK) {
    return false;
  }

  /* Connecting to proxy, if needed, then to the host */
  if (curl_easy_perform(easy_) != CURLE_OK) {
    return false;
  }

  /* Getting socket from curl */
  curl_socket_t sockfd;
  if (curl_easy_getinfo(easy_, CURLINFO_ACTIVESOCKET, &sockfd) != CURLE_OK) {
    // TODO
    return false;
  }

  /* Establishing SSH connection */
  if (libssh2_session_handshake(ssh_session_, sockfd) != 0) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_info("Failed to establish SSH connection, error: %s", err_msg);
    return false;
  }

  /* Checking remote host */
  if (ssh_known_hosts_ != nullptr) {
    size_t hostkey_len = 0U;
    int type = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
    const char *hostkey = libssh2_session_hostkey(ssh_session_, &hostkey_len, &type);
    if (hostkey == nullptr) {
      // TODO
    }
    int keybit = 0;
    switch (type) {
      case LIBSSH2_HOSTKEY_TYPE_RSA:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHRSA;
        break;
      case LIBSSH2_HOSTKEY_TYPE_DSS:
        keybit = LIBSSH2_KNOWNHOST_KEY_SSHDSS;
        break;
      default:
        keybit = LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
        break;
    }
    int keycheck_result = libssh2_knownhost_checkp(ssh_known_hosts_,
                            hostname_.c_str(),
                            -1 /*port*/,
                            hostkey, hostkey_len,
                            LIBSSH2_KNOWNHOST_TYPE_PLAIN |
                            LIBSSH2_KNOWNHOST_KEYENC_RAW |
                            keybit,
                            nullptr /*host*/);
    switch (keycheck_result) {
      case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
      case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
      case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
        logger_->log_warn("Host key verification failed for %s: %d", hostname_.c_str(), keycheck_result); // TODO
        if (strict_host_checking_) {
          return false;
        } else {
        }
        break;
      case LIBSSH2_KNOWNHOST_CHECK_MATCH:
        logger_->log_debug("Host key verification succeeded for %s", hostname_.c_str());
        break;
    }
  } else {
    const char* fingerprint = libssh2_hostkey_hash(ssh_session_, LIBSSH2_HOSTKEY_HASH_SHA1);
    if (fingerprint == nullptr) {
      logger_->log_warn("Cannot get remote server fingerprint");
    } else {
      std::stringstream fingerprint_hex;
      for (size_t i = 0; i < 20; i++) {
        fingerprint_hex << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(static_cast<uint8_t>(fingerprint[i]));
        if (i != 19) {
          fingerprint_hex << ":";
        }
      }
      logger_->log_info("SHA1 host key fingerprint for %s is %s", uri.str().c_str(), fingerprint_hex.str().c_str());
    }
  }

  /* Getting possible authentication methods */
  bool authenticated = false;
  std::set<std::string> auth_methods;
  char* userauthlist = libssh2_userauth_list(ssh_session_, username_.c_str(), strlen(username_.c_str()));
  if (userauthlist == nullptr) {
    if (libssh2_userauth_authenticated(ssh_session_) == 1) {
      authenticated = true;
      logger_->log_warn("SSH server authenticated with SSH_USERAUTH_NONE - this is unusual");
    } else {
      logger_->log_error("Failed to get supported SSH authentication methods");
      return false;
    }
  } else {
    auto methods_split = utils::StringUtils::split(userauthlist, ",");
    auth_methods.insert(std::make_move_iterator(methods_split.begin()), std::make_move_iterator(methods_split.end()));
  }

  /* Authenticating */
  if (!authenticated && public_key_authentication_enabled_ && auth_methods.count("publickey") == 1) {
    if (libssh2_userauth_publickey_fromfile_ex(ssh_session_,
                                               username_.c_str(),
                                               username_.length(),
                                               nullptr /*publickey*/,
                                               private_key_file_path_.c_str(),
                                               private_key_passphrase_.c_str()) == 0) {
      authenticated = true;
      logger_->log_debug("Successfully authenticated with publickey");
    } else {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to authenticate with publickey, error: %s", err_msg);
    }
  }
  if (!authenticated && password_authentication_enabled_ && auth_methods.count("password") == 1) {
    if (libssh2_userauth_password(ssh_session_, username_.c_str(), password_.c_str()) == 0) {
      authenticated = true;
      logger_->log_debug("Successfully authenticated with password");
    } else {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_info("Failed to authenticate with password, error: %s", err_msg);
    }
  }
  if (!authenticated) {
    logger_->log_error("Could not authenticate with any available method");
    return false;
  }

  /* Initializing SFTP session */
  sftp_session_ = libssh2_sftp_init(ssh_session_);
  if (sftp_session_ == nullptr) {
    char *err_msg = nullptr;
    libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
    logger_->log_error("Failed to initialize SFTP session, error: %s", err_msg);
    return false;
  }

  connected_ = true;

  return true;
}

bool SFTPClient::getFile(const std::string& path, io::BaseStream& output) {
  LIBSSH2_SFTP_HANDLE *file_handle = libssh2_sftp_open(sftp_session_, path.c_str(), LIBSSH2_FXF_READ, 0);
  if (file_handle == nullptr) {
    logger_->log_error("Failed to open remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  ScopeGuard guard([&file_handle]() {
    libssh2_sftp_close(file_handle);
  });

  std::vector<uint8_t> buf(32 * 1024U); // TODO
  do {
    ssize_t read_ret = libssh2_sftp_read(file_handle, reinterpret_cast<char*>(buf.data()), buf.size());
    if (read_ret < 0) {
      logger_->log_error("Failed to read remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
      return false;
    } else if (read_ret == 0) {
      break; // TODO
    }
    int remaining = read_ret;
    while (remaining > 0) {
      int write_ret = output.writeData(buf.data() + (buf.size() - remaining), remaining);
      if (write_ret < 0) {
        logger_->log_error("Failed to write output");
        return false;
      }
      remaining -= write_ret;
    }
  } while (true);

  return true;
}

bool SFTPClient::putFile(const std::string& path, io::BaseStream& input, bool overwrite) {
  int flags = 0;
  if (overwrite) {
    flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC;
  } else {
    flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL;
  }
  LIBSSH2_SFTP_HANDLE *file_handle = libssh2_sftp_open(sftp_session_, path.c_str(), flags, 0644);
  if (file_handle == nullptr) {
    logger_->log_error("Failed to open remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  ScopeGuard guard([&file_handle]() {
    libssh2_sftp_close(file_handle);
  });

  std::vector<uint8_t> buf(32 * 1024U); // TODO
  do {
    int read_ret = input.readData(buf.data(), buf.size());
    if (read_ret < 0) {
      char *err_msg = nullptr;
      libssh2_session_last_error(ssh_session_, &err_msg, nullptr, 0);
      logger_->log_error("Error while reading input");
      return false;
    } else if (read_ret == 0) {
      break; // TODO
    }
    ssize_t remaining = read_ret;
    while (remaining > 0) {
      int write_ret = libssh2_sftp_write(file_handle, reinterpret_cast<char*>(buf.data() + (buf.size() - remaining)), remaining);
      if (write_ret < 0) {
        logger_->log_error("Failed to write remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
        return false;
      }
      remaining -= write_ret;
    }
  } while (true);

  return true;
}

bool SFTPClient::rename(const std::string& source_path, const std::string& target_path, bool overwrite) {
  int flags = 0;
  if (overwrite) {
    flags = LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE | LIBSSH2_SFTP_RENAME_OVERWRITE;
  } else {
    flags = LIBSSH2_SFTP_RENAME_ATOMIC | LIBSSH2_SFTP_RENAME_NATIVE;
  }
  if (libssh2_sftp_rename_ex(sftp_session_,
                              source_path.c_str(),
                              source_path.length(),
                              target_path.c_str(),
                              target_path.length(),
                              flags) != 0) {
    logger_->log_error("Failed to rename remote file \"%s\", error: %s", source_path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  return true;
}

// TODO: what if there is no ending /
bool SFTPClient::createDirectoryHierarchy(const std::string& path) {
  if (path.empty() || path[0] != '/') {
    return false;
  }
  size_t pos = 1U;
  while ((pos = path.find('/', pos)) != std::string::npos) {
    /* Skip //-s in path */
    if (path[pos] == '/') {
      pos++;
      continue;
    }
    int res = libssh2_sftp_mkdir_ex(sftp_session_, path.c_str(), pos, 0755);
    if (res < 0) {
      auto err = libssh2_sftp_last_error(sftp_session_);
      if (err != LIBSSH2_FX_FILE_ALREADY_EXISTS &&
          err != LIBSSH2_FX_FAILURE &&
          err != LIBSSH2_FX_PERMISSION_DENIED) {
        logger_->log_error("Failed to create remote directory \"%s\", error: %s", path.substr(0, pos).c_str(), sftp_strerror(err));
        return false;
      } else {
        logger_->log_debug("Non-fatal failure to create remote directory \"%s\", error: %s", path.substr(0, pos).c_str(), sftp_strerror(err));
      }
    }
    pos++;
  }
  return true;
}

bool SFTPClient::removeFile(const std::string& path) {
  if (libssh2_sftp_unlink(sftp_session_, path.c_str()) != 0) {
    logger_->log_error("Failed to remove remote file \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  return true;
}

bool SFTPClient::removeDirectory(const std::string& path) {
  if (libssh2_sftp_rmdir(sftp_session_, path.c_str()) != 0) {
    logger_->log_error("Failed to remove remote directory \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  return true;
}

bool SFTPClient::listDirectory(const std::string& path, bool follow_symlinks,
    std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>>& children_result) {
  LIBSSH2_SFTP_HANDLE *dir_handle = libssh2_sftp_open_ex(sftp_session_,
                                                          path.c_str(),
                                                          path.length(),
                                                          0 /* flags */,
                                                          0 /* mode */,
                                                          LIBSSH2_SFTP_OPENDIR);
  if (dir_handle == nullptr) {
    logger_->log_error("Failed to open remote directory \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
    return false;
  }
  ScopeGuard guard([&dir_handle]() {
    libssh2_sftp_close(dir_handle);
  });

  LIBSSH2_SFTP_ATTRIBUTES attrs;
  std::vector<char> filename(PATH_MAX);
  std::vector<char> longentry(PATH_MAX);
  do {
    int ret = libssh2_sftp_readdir_ex(dir_handle,
                                      filename.data(),
                                      filename.size(),
                                      longentry.data(),
                                      longentry.size(),
                                      &attrs);
    if (ret < 0) {
      logger_->log_error("Failed to read remote directory \"%s\", error: %s", path.c_str(), sftp_strerror(libssh2_sftp_last_error(sftp_session_)));
      return false;
    } else if (ret == 0) {
      break;
    }
    children_result.emplace_back(std::string(filename.data()), std::string(longentry.data()), std::move(attrs));
  } while (true);
  return true;
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
