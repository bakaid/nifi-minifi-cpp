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
#ifndef __LIST_SFTP_H__
#define __LIST_SFTP_H__

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#ifndef WIN32
#include <regex.h>
#else
#include <regex>
#endif

#include "SFTPProcessorBase.h"
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

class ListSFTP : public SFTPProcessorBase {
 public:

  static constexpr char const *LISTING_STRATEGY_TRACKING_TIMESTAMPS = "Tracking Timestamps";
  static constexpr char const *LISTING_STRATEGY_TRACKING_ENTITIES = "Tracking Entities";

  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT = "Auto Detect";
  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS = "Milliseconds";
  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS = "Seconds";
  static constexpr char const *TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES = "Minutes";

  static constexpr char const *ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW = "Tracking Time Window";
  static constexpr char const *ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE = "All Available";

  static constexpr char const* ProcessorName = "ListSFTP";


  /*!
   * Create a new processor
   */
  ListSFTP(std::string name, utils::Identifier uuid = utils::Identifier());
  virtual ~ListSFTP();

  // Supported Properties
  static core::Property ListingStrategy;
  static core::Property Hostname;
  static core::Property Port;
  static core::Property Username;
  static core::Property Password;
  static core::Property PrivateKeyPath;
  static core::Property PrivateKeyPassphrase;
  static core::Property RemotePath;
  static core::Property SearchRecursively;
  static core::Property FollowSymlink;
  static core::Property FileFilterRegex;
  static core::Property PathFilterRegex;
  static core::Property IgnoreDottedFiles;
  static core::Property StrictHostKeyChecking;
  static core::Property HostKeyFile;
  static core::Property ConnectionTimeout;
  static core::Property DataTimeout;
  static core::Property SendKeepaliveOnTimeout;
  static core::Property TargetSystemTimestampPrecision;
  static core::Property ProxyType;
  static core::Property ProxyHost;
  static core::Property ProxyPort;
  static core::Property HttpProxyUsername;
  static core::Property HttpProxyPassword;
  static core::Property EntityTrackingTimeWindow;
  static core::Property EntityTrackingInitialListingTarget;
  static core::Property MinimumFileAge;
  static core::Property MaximumFileAge;
  static core::Property MinimumFileSize;
  static core::Property MaximumFileSize;

  // Supported Relationships
  static core::Relationship Success;

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_HOST = "sftp.remote.host";
  static constexpr char const* ATTRIBUTE_SFTP_REMOTE_PORT = "sftp.remote.port";
  static constexpr char const* ATTRIBUTE_SFTP_LISTING_USER = "sftp.listing.user";
  static constexpr char const* ATTRIBUTE_FILE_OWNER = "file.owner";
  static constexpr char const* ATTRIBUTE_FILE_GROUP = "file.group";
  static constexpr char const* ATTRIBUTE_FILE_PERMISSIONS = "file.permissions";
  static constexpr char const* ATTRIBUTE_FILE_SIZE = "file.size";
  static constexpr char const* ATTRIBUTE_FILE_LASTMODIFIEDTIME = "file.lastModifiedTime";

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  virtual void notifyStop() override;

 private:

  std::string listing_strategy_;
  bool search_recursively_;
  bool follow_symlink_;
  std::string file_filter_regex_;
  std::string path_filter_regex_;
  bool file_filter_regex_set_;
  bool path_filter_regex_set_;
#ifndef WIN32
  regex_t compiled_file_filter_regex_;
  regex_t compiled_path_filter_regex_;
#else
  std::regex compiled_file_filter_regex_;
  std::regex compiled_path_filter_regex_;
#endif
  bool ignore_dotted_files_;
  std::string target_system_timestamp_precision_;
  std::string entity_tracking_initial_listing_target_;
  uint64_t minimum_file_age_;
  uint64_t maximum_file_age_;
  uint64_t minimum_file_size_;
  uint64_t maximum_file_size_;

  uint64_t last_listed_latest_entry_timestamp_;
  uint64_t last_processed_latest_entry_timestamp_;

  struct Child {
    Child();
    Child(const std::string& parent_path_, std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>&& sftp_child);

    bool directory;
    std::string parent_path;
    std::string filename;
    LIBSSH2_SFTP_ATTRIBUTES attrs;
  };

  bool filter(const std::string& parent_path, const std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>& sftp_child);
  bool filterFile(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs);
  bool filterDirectory(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs);

  bool createAndTransferFlowFileFromChild(
      const std::shared_ptr<core::ProcessSession>& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      const Child& child);

  void listByTrackingTimestamps(
      const std::shared_ptr<core::ProcessContext>& context,
      const std::shared_ptr<core::ProcessSession>& session,
      const std::string& hostname,
      uint16_t port,
      const std::string& username,
      std::vector<Child>&& files);
};

REGISTER_RESOURCE(ListSFTP, "Performs a listing of the files residing on an SFTP server. "
                            "For each file that is found on the remote server, a new FlowFile will be created with "
                            "the filename attribute set to the name of the file on the remote server. "
                            "This can then be used in conjunction with FetchSFTP in order to fetch those files.")

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
