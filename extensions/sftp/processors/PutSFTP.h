/**
 * PutSFTP class declaration
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
#ifndef __PUT_SFTP_H__
#define __PUT_SFTP_H__

#include <memory>
#include <string>

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "../client/SFTPClient.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PutSFTP : public core::Processor {
 public:

  static constexpr char const *CONFLICT_RESOLUTION_REPLACE = "REPLACE";
  static constexpr char const *CONFLICT_RESOLUTION_IGNORE = "IGNORE";
  static constexpr char const *CONFLICT_RESOLUTION_RENAME = "RENAME";
  static constexpr char const *CONFLICT_RESOLUTION_REJECT = "REJECT";
  static constexpr char const *CONFLICT_RESOLUTION_FAIL = "FAIL";
  static constexpr char const *CONFLICT_RESOLUTION_NONE = "NONE";

  static constexpr char const *PROXY_TYPE_DIRECT = "DIRECT";
  static constexpr char const *PROXY_TYPE_HTTP = "HTTP";
  static constexpr char const *PROXY_TYPE_SOCKS = "SOCKS";

  static constexpr char const* ProcessorName = "PutSFTP";


  /*!
   * Create a new processor
   */
  PutSFTP(std::string name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<PutSFTP>::getLogger()) {
    static utils::LibSSH2Initializer *initializer = utils::LibSSH2Initializer::getInstance();
    initializer->initialize();
    // TODO
  }
  virtual ~PutSFTP();

  // Supported Properties
  static core::Property Hostname;
  static core::Property Port;
  static core::Property Username;
  static core::Property Password;
  static core::Property PrivateKeyPath;
  static core::Property PrivateKeyPassphrase;
  static core::Property RemotePath;
  static core::Property CreateDirectory;
  static core::Property DisableDirectoryListing;
  static core::Property BatchSize;
  static core::Property ConnectionTimeout;
  static core::Property DataTimeout;
  static core::Property ConflictResolution;
  static core::Property RejectZeroByte;
  static core::Property DotRename;
  static core::Property TempFilename;
  static core::Property HostKeyFile;
  static core::Property LastModifiedTime;
  static core::Property Permissions;
  static core::Property RemoteOwner;
  static core::Property RemoteGroup;
  static core::Property StrictHostKeyChecking;
  static core::Property UseKeepaliveOnTimeout;
  static core::Property UseCompression;
  static core::Property ProxyType;
  static core::Property ProxyHost;
  static core::Property ProxyPort;
  static core::Property HttpProxyUsername;
  static core::Property HttpProxyPassword;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Reject;
  static core::Relationship Failure;

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(PutSFTP, "Sends FlowFiles to an SFTP Server")

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
