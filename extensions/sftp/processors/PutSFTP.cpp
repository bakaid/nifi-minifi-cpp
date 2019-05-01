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

#include "PutSFTP.h"
#ifdef WIN32
#include <regex>
#else
#include <regex.h>
#endif
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property PutSFTP::Hostname(
    core::PropertyBuilder::createProperty("Hostname")->withDescription("The fully qualified hostname or IP address of the remote system")->build());
core::Property PutSFTP::Port(
    core::PropertyBuilder::createProperty("Port")->withDescription("The port that the remote system is listening on for file transfers")
        ->withDefaultValue<int>(22)->build());
core::Property PutSFTP::Username(
    core::PropertyBuilder::createProperty("Username")->withDescription("Username")->build());
core::Property PutSFTP::Password(
    core::PropertyBuilder::createProperty("Password")->withDescription("Password for the user account")
        ->isRequired(false)->build());
core::Property PutSFTP::PrivateKeyPath(
    core::PropertyBuilder::createProperty("Private Key Path")->withDescription("The fully qualified path to the Private Key file")
        ->isRequired(false)->build());
core::Property PutSFTP::PrivateKeyPassphrase(
    core::PropertyBuilder::createProperty("Private Key Passphrase")->withDescription("Password for the private key")
        ->isRequired(false)->build());
core::Property PutSFTP::RemotePath(
    core::PropertyBuilder::createProperty("Remote Path")->withDescription("The path on the remote system from which to pull or push files")
        ->isRequired(false)->build());
core::Property PutSFTP::CreateDirectory(
    core::PropertyBuilder::createProperty("Create Directory")->withDescription("Specifies whether or not the remote directory should be created if it does not exist.")
        ->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::DisableDirectoryListing(
    core::PropertyBuilder::createProperty("Disable Directory Listing")->withDescription("If set to 'true', directory listing is not performed prior to create missing directories. "
                                                                                        "By default, this processor executes a directory listing command to see target directory existence before creating missing directories. "
                                                                                        "However, there are situations that you might need to disable the directory listing such as the following. "
                                                                                        "Directory listing might fail with some permission setups (e.g. chmod 100) on a directory. "
                                                                                        "Also, if any other SFTP client created the directory after this processor performed a listing and before a directory creation request by this processor is finished, "
                                                                                        "then an error is returned because the directory already exists.")
                                                                                        ->isRequired(false)->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::BatchSize(
    core::PropertyBuilder::createProperty("Batch Size")->withDescription("The maximum number of FlowFiles to send in a single connection")
        ->withDefaultValue<int>(500)->build());
core::Property PutSFTP::ConnectionTimeout(
    core::PropertyBuilder::createProperty("Connection Timeout")->withDescription("Amount of time to wait before timing out while creating a connection")
        ->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property PutSFTP::DataTimeout(
    core::PropertyBuilder::createProperty("Data Timeout")->withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
        ->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property PutSFTP::ConflictResolution(
    core::PropertyBuilder::createProperty("Conflict Resolution")->withDescription("Determines how to handle the problem of filename collisions")
        ->withAllowableValues<std::string>({CONFLICT_RESOLUTION_REPLACE,
                                            CONFLICT_RESOLUTION_IGNORE,
                                            CONFLICT_RESOLUTION_RENAME,
                                            CONFLICT_RESOLUTION_REJECT,
                                            CONFLICT_RESOLUTION_FAIL,
                                            CONFLICT_RESOLUTION_NONE})
        ->withDefaultValue(CONFLICT_RESOLUTION_NONE)->build());
core::Property PutSFTP::RejectZeroByte(
    core::PropertyBuilder::createProperty("Reject Zero-Byte Files")->withDescription("Determines whether or not Zero-byte files should be rejected without attempting to transfer")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property PutSFTP::DotRename(
    core::PropertyBuilder::createProperty("Dot Rename")->withDescription("If true, then the filename of the sent file is prepended with a \".\" and then renamed back to the original once the file is completely sent. "
                                                                         "Otherwise, there is no rename. This property is ignored if the Temporary Filename property is set.")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property PutSFTP::TempFilename(
    core::PropertyBuilder::createProperty("Temporary Filename")->withDescription("If set, the filename of the sent file will be equal to the value specified during the transfer and after successful completion will be renamed to the original filename. "
                                                                                 "If this value is set, the Dot Rename property is ignored.")
        ->isRequired(false)->build());
core::Property PutSFTP::HostKeyFile(
    core::PropertyBuilder::createProperty("Host Key File")->withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
        ->isRequired(false)->build());
core::Property PutSFTP::LastModifiedTime(
    core::PropertyBuilder::createProperty("Last Modified Time")->withDescription("The lastModifiedTime to assign to the file after transferring it. "
                                                                                  "If not set, the lastModifiedTime will not be changed. "
                                                                                  "Format must be yyyy-MM-dd'T'HH:mm:ssZ. "
                                                                                  "You may also use expression language such as ${file.lastModifiedTime}. "
                                                                                  "If the value is invalid, the processor will not be invalid but will fail to change lastModifiedTime of the file.")
        ->isRequired(false)->build()); // TODO: timestamp validation
core::Property PutSFTP::Permissions(
    core::PropertyBuilder::createProperty("Permissions")->withDescription("The permissions to assign to the file after transferring it. "
                                                                          "Format must be either UNIX rwxrwxrwx with a - in place of denied permissions (e.g. rw-r--r--) or an octal number (e.g. 644). "
                                                                          "If not set, the permissions will not be changed. "
                                                                          "You may also use expression language such as ${file.permissions}. "
                                                                          "If the value is invalid, the processor will not be invalid but will fail to change permissions of the file.")
        ->isRequired(false)->build());
core::Property PutSFTP::RemoteOwner(
    core::PropertyBuilder::createProperty("Remote Owner")->withDescription("Integer value representing the User ID to set on the file after transferring it. "
                                                                           "If not set, the owner will not be set. You may also use expression language such as ${file.owner}. "
                                                                           "If the value is invalid, the processor will not be invalid but will fail to change the owner of the file.")
        ->isRequired(false)->build());
core::Property PutSFTP::RemoteGroup(
    core::PropertyBuilder::createProperty("Remote Group")->withDescription("Integer value representing the Group ID to set on the file after transferring it. "
                                                                           "If not set, the group will not be set. You may also use expression language such as ${file.group}. "
                                                                           "If the value is invalid, the processor will not be invalid but will fail to change the group of the file.")
        ->isRequired(false)->build());
core::Property PutSFTP::StrictHostKeyChecking(
    core::PropertyBuilder::createProperty("Strict Host Key Checking")->withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
        ->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::UseKeepaliveOnTimeout(
    core::PropertyBuilder::createProperty("Send Keep Alive On Timeout")->withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
        ->withDefaultValue<bool>(true)->build());
core::Property PutSFTP::UseCompression(
    core::PropertyBuilder::createProperty("Use Compression")->withDescription("Indicates whether or not ZLIB compression should be used when transferring files")
        ->withDefaultValue<bool>(false)->build());
core::Property PutSFTP::ProxyType(
    core::PropertyBuilder::createProperty("Proxy Type")->withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
                                                                         "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
        ->isRequired(false)
        ->withAllowableValues<std::string>({PROXY_TYPE_DIRECT,
                                            PROXY_TYPE_HTTP,
                                            PROXY_TYPE_SOCKS})
        ->withDefaultValue(PROXY_TYPE_DIRECT)->build());
core::Property PutSFTP::ProxyHost(
    core::PropertyBuilder::createProperty("Proxy Host")->withDescription("The fully qualified hostname or IP address of the proxy server")
        ->isRequired(false)->build());
core::Property PutSFTP::ProxyPort(
    core::PropertyBuilder::createProperty("Proxy Port")->withDescription("The port of the proxy server")
        ->isRequired(false)->build());
core::Property PutSFTP::HttpProxyUsername(
    core::PropertyBuilder::createProperty("Http Proxy Username")->withDescription("Http Proxy Username")
        ->isRequired(false)->build());
core::Property PutSFTP::HttpProxyPassword(
    core::PropertyBuilder::createProperty("Http Proxy Password")->withDescription("Http Proxy Password")
        ->isRequired(false)->build());

core::Relationship PutSFTP::Success("success", "FlowFiles that are successfully sent will be routed to success");
core::Relationship PutSFTP::Reject("reject", "FlowFiles that were rejected by the destination system");
core::Relationship PutSFTP::Failure("failure", "FlowFiles that failed to send to the remote system; failure is usually looped back to this processor");

void PutSFTP::initialize() {
  logger_->log_trace("Initializing PutSFTP");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(Hostname);
  properties.insert(Port);
  properties.insert(Username);
  properties.insert(Password);
  properties.insert(PrivateKeyPath);
  properties.insert(PrivateKeyPassphrase);
  properties.insert(RemotePath);
  properties.insert(CreateDirectory);
  properties.insert(DisableDirectoryListing);
  properties.insert(BatchSize);
  properties.insert(ConnectionTimeout);
  properties.insert(DataTimeout);
  properties.insert(ConflictResolution);
  properties.insert(RejectZeroByte);
  properties.insert(DotRename);
  properties.insert(TempFilename);
  properties.insert(HostKeyFile);
  properties.insert(LastModifiedTime);
  properties.insert(Permissions);
  properties.insert(RemoteOwner);
  properties.insert(RemoteGroup);
  properties.insert(StrictHostKeyChecking);
  properties.insert(UseKeepaliveOnTimeout);
  properties.insert(UseCompression);
  properties.insert(ProxyType);
  properties.insert(ProxyHost);
  properties.insert(ProxyPort);
  properties.insert(HttpProxyUsername);
  properties.insert(HttpProxyPassword);
  setSupportedProperties(properties);
  
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  relationships.insert(Reject);
  relationships.insert(Failure);
  setSupportedRelationships(relationships);
}

PutSFTP::~PutSFTP() {
}

void PutSFTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  if (!context->getProperty(CreateDirectory.getName(), value)) {
    logger_->log_error("Create Directory attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, create_directory_);
  }
  if (context->getProperty(DisableDirectoryListing.getName(), value)) {
    utils::StringUtils::StringToBool(value, disable_directory_listing_);
  }
  if (!context->getProperty(BatchSize.getName(), value)) {
    logger_->log_error("Batch Size attribute is missing or invalid");
  } else {
    core::Property::StringToInt(value, batch_size_);
  }
  if (!context->getProperty(ConnectionTimeout.getName(), value)) {
    logger_->log_error("Connection Timeout attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, connection_timeout_, unit) || !core::Property::ConvertTimeUnitToMS(connection_timeout_, unit, connection_timeout_)) {
      logger_->log_error("Connection Timeout attribute is invalid");
    }
  }
  if (!context->getProperty(DataTimeout.getName(), value)) {
    logger_->log_error("Data Timeout attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, data_timeout_, unit) || !core::Property::ConvertTimeUnitToMS(data_timeout_, unit, data_timeout_)) {
      logger_->log_error("Data Timeout attribute is invalid");
    }
  }
  context->getProperty(ConflictResolution.getName(), conflict_resolution_);
  if (context->getProperty(RejectZeroByte.getName(), value)) {
    utils::StringUtils::StringToBool(value, reject_zero_byte_);
  }
  if (context->getProperty(DotRename.getName(), value)) {
    utils::StringUtils::StringToBool(value, dot_rename_);
  }
  context->getProperty(HostKeyFile.getName(), host_key_file_);
  if (!context->getProperty(StrictHostKeyChecking.getName(), value)) {
    logger_->log_error("Strict Host Key Checking attribute is missing or invalid");
  } else {
    core::Property::StringToInt(value, strict_host_checking_);
  }
  if (!context->getProperty(UseCompression.getName(), value)) {
    logger_->log_error("Use Compression attribute is missing or invalid");
  } else {
    core::Property::StringToInt(value, use_compression_);
  }
  context->getProperty(ProxyType.getName(), proxy_type_);
}

PutSFTP::ReadCallback::ReadCallback(const std::string& target_path,
                                    utils::SFTPClient& client,
                                    const std::string& conflict_resolution)
    : logger_(logging::LoggerFactory<PutSFTP::ReadCallback>::getLogger())
    , write_succeeded_(false)
    , target_path_(target_path)
    , client_(client)
    , conflict_resolution_(conflict_resolution) {
}

PutSFTP::ReadCallback::~ReadCallback() {
}

int64_t PutSFTP::ReadCallback::process(std::shared_ptr<io::BaseStream> stream) {
  if (!client_.putFile(target_path_, *stream, conflict_resolution_ == "REPLACE" /*overwrite*/)) {
    return -1;
  }
  write_succeeded_ = true;
  return stream->getSize();
}

bool PutSFTP::ReadCallback::commit() {
  return true;
}

void PutSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->get());
  if (flow_file == nullptr) {
    return;
  }
  std::string filename;
  flow_file->getKeyedAttribute(FILENAME, filename);

  /* Parse possibly flowfile-dependent properties */
  std::string hostname;
  uint16_t port = 0U;
  std::string username;
  std::string password;
  std::string private_key_path;
  std::string private_key_passphrase;
  std::string remote_path;
  std::string temp_file_name;
  bool last_modified_time_set = false;
  int64_t last_modified_time = 0U;
  bool permissions_set = false;
  uint32_t permissions = 0U;
  bool remote_owner_set = false;
  uint64_t remote_owner = 0U;
  bool remote_group_set = false;
  uint64_t remote_group = 0U;
  std::string proxy_host;
  uint16_t proxy_port = 0U;
  std::string proxy_username;
  std::string proxy_password;

  std::string value;
  if (!context->getProperty(Hostname, hostname, flow_file)) {
    logger_->log_error("Hostname attribute is missing");
    context->yield();
    return;
  }
  if (!context->getProperty(Port, value, flow_file)) {
    logger_->log_error("Port attribute is missing or invalid");
    context->yield();
    return;
  } else {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Port attribute is invalid");
      context->yield();
      return;
    } else {
      port = static_cast<uint16_t>(port_tmp);
    }
  }
  if (!context->getProperty(Username, username, flow_file)) {
    logger_->log_error("Username attribute is missing");
    context->yield();
    return;
  }
  context->getProperty(Password, password, flow_file);
  context->getProperty(PrivateKeyPath, private_key_path, flow_file);
  context->getProperty(PrivateKeyPassphrase, private_key_passphrase, flow_file);
  context->getProperty(Password, password, flow_file);
  context->getProperty(RemotePath, remote_path, flow_file);
  while (remote_path.size() > 1U && remote_path.back() == '/') {
    remote_path.resize(remote_path.size() - 1);
  }
  context->getProperty(TempFilename, temp_file_name, flow_file);
  if (context->getProperty(LastModifiedTime, value, flow_file)) {
    if (core::Property::StringToDateTime(value, last_modified_time)) {
      last_modified_time_set = true;
    }
  }
  if (context->getProperty(Permissions, value, flow_file)) {
    if (core::Property::StringToPermissions(value, permissions)) {
      permissions_set = true;
    }
  }
  if (context->getProperty(RemoteOwner, value, flow_file)) {
    if (core::Property::StringToInt(value, remote_owner)) {
      remote_owner_set = true;
    }
  }
  if (context->getProperty(RemoteGroup, value, flow_file)) {
    if (core::Property::StringToInt(value, remote_group)) {
      remote_group_set = true;
    }
  }
  context->getProperty(ProxyHost, proxy_host, flow_file);
  if (context->getProperty(ProxyPort, value, flow_file)) {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Proxy Port attribute is invalid");
      context->yield();
      return;
    } else {
      proxy_port = static_cast<uint16_t>(port_tmp);
    }
  }
  context->getProperty(HttpProxyUsername, proxy_username, flow_file);
  context->getProperty(HttpProxyPassword, proxy_password, flow_file);

  /* Create and setup SFTPClient */
  utils::SFTPClient client(hostname, port, username);
  if (!IsNullOrEmpty(host_key_file_)) {
    if (!client.setHostKeyFile(host_key_file_, strict_host_checking_)) {
      logger_->log_error("Cannot set host key file");
      context->yield();
      return;
    }
  }
  if (!IsNullOrEmpty(password)) {
    client.setPasswordAuthenticationCredentials(password);
  }
  if (!IsNullOrEmpty(private_key_path)) {
    client.setPublicKeyAuthenticationCredentials(private_key_path, private_key_passphrase);
  }
  if (proxy_type_ != PROXY_TYPE_DIRECT) {
    utils::HTTPProxy proxy;
    proxy.host = proxy_host;
    proxy.port = proxy_port;
    proxy.username = proxy_username;
    proxy.password = proxy_password;
    if (!client.setProxy(proxy_type_ == PROXY_TYPE_HTTP ? utils::SFTPClient::ProxyType::Http : utils::SFTPClient::ProxyType::Socks, proxy)) {
      logger_->log_error("Cannot set proxy");
      context->yield();
      return;
    }
  }
  if (!client.setConnectionTimeout(connection_timeout_)) {
    logger_->log_error("Cannot set connection timeout");
    context->yield();
    return;
  }
  client.setDataTimeout(data_timeout_);
  if (!client.setSendKeepAlive(use_keepalive_on_timeout_)) {
    logger_->log_error("Cannot set keepalive on timeout");
    context->yield();
    return;
  }
  if (!client.setUseCompression(use_compression_)) {
    logger_->log_error("Cannot set compression");
    context->yield();
    return;
  }

  /* Connect to SFTP server */
  if (!client.connect()) {
    logger_->log_error("Cannot connect to SFTP server");
    context->yield();
    return;
  }

  /* Create remote directory if needed */
  bool should_create_directory = disable_directory_listing_;
  if (!disable_directory_listing_) {
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    if (!client.stat(remote_path, true /*follow_symlinks*/, attrs)) {
      logger_->log_error("Cannot stat %s", remote_path.c_str());
      should_create_directory = true;
    } else {
      if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS && !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
        logger_->log_error("Remote path %s is not a directory", remote_path.c_str());
        context->yield();
        return;
      }
    }
  }
  if (should_create_directory) {
    client.createDirectoryHierarchy(remote_path);
    if (!disable_directory_listing_) {
      LIBSSH2_SFTP_ATTRIBUTES attrs;
      if (!client.stat(remote_path, true /*follow_symlinks*/, attrs)) {
        logger_->log_error("Could not create remote directory %s", remote_path.c_str());
        context->yield();
        return;
      } else {
        if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS && !LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
          logger_->log_error("Remote path %s is not a directory", remote_path.c_str());
          context->yield();
          return;
        }
      }
    }
  }

  /* Upload file */
  std::stringstream target_path;
  target_path << remote_path << "/";
  if (!IsNullOrEmpty(temp_file_name)) {
    target_path << temp_file_name;
  } else if (dot_rename_) {
    target_path << "." << filename;
  } else {
    target_path << filename;
  }
  logger_->log_debug("The target path is %s", target_path.str().c_str());

  ReadCallback read_callback(target_path.str(), client, conflict_resolution_);
  session->read(flow_file, &read_callback);

  if (read_callback.commit()) {
    session->transfer(flow_file, Success);
  } else {
    session->transfer(flow_file, Failure);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
