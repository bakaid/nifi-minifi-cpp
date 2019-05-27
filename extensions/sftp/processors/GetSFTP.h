///**
// * PutSFTP class declaration
// *
// * Licensed to the Apache Software Foundation (ASF) under one or more
// * contributor license agreements.  See the NOTICE file distributed with
// * this work for additional information regarding copyright ownership.
// * The ASF licenses this file to You under the Apache License, Version 2.0
// * (the "License"); you may not use this file except in compliance with
// * the License.  You may obtain a copy of the License at
// *
// *     http://www.apache.org/licenses/LICENSE-2.0
// *
// * Unless required by applicable law or agreed to in writing, software
// * distributed under the License is distributed on an "AS IS" BASIS,
// * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// * See the License for the specific language governing permissions and
// * limitations under the License.
// */
//#ifndef __GET_SFTP_H__
//#define __GET_SFTP_H__
//
//#include <memory>
//#include <string>
//#include <list>
//#include <map>
//#include <mutex>
//#include <thread>
//
//#include "utils/ByteArrayCallback.h"
//#include "FlowFileRecord.h"
//#include "core/Processor.h"
//#include "core/ProcessSession.h"
//#include "core/Core.h"
//#include "core/Property.h"
//#include "core/Resource.h"
//#include "controllers/SSLContextService.h"
//#include "core/logging/LoggerConfiguration.h"
//#include "utils/Id.h"
//#include "../client/SFTPClient.h"
//
//namespace org {
//namespace apache {
//namespace nifi {
//namespace minifi {
//namespace processors {
//
//class GetSFTP : public core::Processor {
// public:
//
//  static constexpr char const *PROXY_TYPE_DIRECT = "DIRECT";
//  static constexpr char const *PROXY_TYPE_HTTP = "HTTP";
//  static constexpr char const *PROXY_TYPE_SOCKS = "SOCKS";
//
//  static constexpr char const* ProcessorName = "GetSFTP";
//
//
//  /*!
//   * Create a new processor
//   */
//  GetSFTP(std::string name, utils::Identifier uuid = utils::Identifier());
//  virtual ~GetSFTP();
//
//  // Supported Properties
//  static core::Property Hostname;
//  static core::Property Port;
//  static core::Property Username;
//  static core::Property Password;
//  static core::Property PrivateKeyPath;
//  static core::Property PrivateKeyPassphrase;
//  static core::Property RemotePath;
//  static core::Property FileFilterRegex;
//  static core::Property PathFilterRegex;
//  static core::Property PollingInterval;
//  static core::Property SearchRecursively;
//  static core::Property FollowSymlink;
//  static core::Property IgnoreDottedFiles;
//  static core::Property DeleteOriginal;
//  static core::Property ConnectionTimeout;
//  static core::Property DataTimeout;
//  static core::Property HostKeyFile;
//  static core::Property MaxSelects;
//  static core::Property RemotePollBatchSize;
//  static core::Property StrictHostKeyChecking;
//  static core::Property SendKeepaliveOnTimeout;
//  static core::Property UseCompression;
//  static core::Property UseNaturalOrdering;
//  static core::Property ProxyType;
//  static core::Property ProxyHost;
//  static core::Property ProxyPort;
//  static core::Property HttpProxyUsername;
//  static core::Property HttpProxyPassword;
//
//  // Supported Relationships
//  static core::Relationship Success;
//
//  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
//  virtual void initialize() override;
//  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
//  virtual void notifyStop() override;
//
//  class WriteCallback : public OutputStreamCallback {
//   public:
//    WriteCallback(utils::SFTPClient& client);
//    ~WriteCallback();
//    virtual int64_t process(std::shared_ptr<io::BaseStream> stream) override;
//
//   private:
//    std::shared_ptr<logging::Logger> logger_;
//    utils::SFTPClient& client_;
//  };
//
// private:
//
//  std::shared_ptr<logging::Logger> logger_;
//
//  std::string file_filter_regex;
//  std::string path_filter_regex;
//  uint32_t polling_interval;
//  bool search_recursively;
//  bool follow_symlink;
//  bool ignore_dotted_files;
//  bool delete_original;
//  uint32_t max_selects;
//  uint32_t remote_poll_batch_size;
//  bool use_natural_ordering;
//
//  bool processOne(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);
//};
//
//REGISTER_RESOURCE(PutSFTP, "Sends FlowFiles to an SFTP Server")
//
//} /* namespace processors */
//} /* namespace minifi */
//} /* namespace nifi */
//} /* namespace apache */
//} /* namespace org */
//
//#endif
