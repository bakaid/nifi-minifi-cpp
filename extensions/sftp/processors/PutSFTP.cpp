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
}


void PutSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flowFile = std::static_pointer_cast<FlowFileRecord>(session->get());
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
