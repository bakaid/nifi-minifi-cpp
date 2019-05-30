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

#include "ListSFTP.h"

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
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtils.h"
#include "utils/ScopeGuard.h"
#include "utils/file/FileUtils.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property ListSFTP::ListingStrategy(
    core::PropertyBuilder::createProperty("Listing Strategy")->withDescription("Specify how to determine new/updated entities. See each strategy descriptions for detail.")
        ->isRequired(true)
        ->withAllowableValues<std::string>({LISTING_STRATEGY_TRACKING_TIMESTAMPS,
                                            LISTING_STRATEGY_TRACKING_ENTITIES})
        ->withDefaultValue(LISTING_STRATEGY_TRACKING_TIMESTAMPS)->build());
core::Property ListSFTP::Hostname(
    core::PropertyBuilder::createProperty("Hostname")->withDescription("The fully qualified hostname or IP address of the remote system")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::Port(
    core::PropertyBuilder::createProperty("Port")->withDescription("The port that the remote system is listening on for file transfers")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::Username(
    core::PropertyBuilder::createProperty("Username")->withDescription("Username")
        ->isRequired(true)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::Password(
    core::PropertyBuilder::createProperty("Password")->withDescription("Password for the user account")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::PrivateKeyPath(
    core::PropertyBuilder::createProperty("Private Key Path")->withDescription("The fully qualified path to the Private Key file")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::PrivateKeyPassphrase(
    core::PropertyBuilder::createProperty("Private Key Passphrase")->withDescription("Password for the private key")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::RemotePath(
    core::PropertyBuilder::createProperty("Remote Path")->withDescription("The fully qualified filename on the remote system")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::SearchRecursively(
    core::PropertyBuilder::createProperty("Search Recursively")->withDescription("If true, will pull files from arbitrarily nested subdirectories; "
                                                                                 "otherwise, will not traverse subdirectories")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property ListSFTP::FollowSymlink(
    core::PropertyBuilder::createProperty("Follow symlink")->withDescription("If true, will pull even symbolic files and also nested symbolic subdirectories; "
                                                                             "otherwise, will not read symbolic files and will not traverse symbolic link subdirectories")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property ListSFTP::FileFilterRegex(
    core::PropertyBuilder::createProperty("File Filter Regex")->withDescription("Provides a Java Regular Expression for filtering Filenames; "
                                                                                "if a filter is supplied, only files whose names match that Regular Expression will be fetched")
        ->isRequired(false)->build());
core::Property ListSFTP::PathFilterRegex(
    core::PropertyBuilder::createProperty("Path Filter Regex")->withDescription("When Search Recursively is true, then only subdirectories whose path matches the given Regular Expression will be scanned")
        ->isRequired(false)->build());
core::Property ListSFTP::IgnoreDottedFiles(
    core::PropertyBuilder::createProperty("Ignore Dotted Files")->withDescription("If true, files whose names begin with a dot (\".\") will be ignored")
        ->isRequired(true)->withDefaultValue<bool>(true)->build());
core::Property ListSFTP::StrictHostKeyChecking(
    core::PropertyBuilder::createProperty("Strict Host Key Checking")->withDescription("Indicates whether or not strict enforcement of hosts keys should be applied")
        ->isRequired(true)->withDefaultValue<bool>(false)->build());
core::Property ListSFTP::HostKeyFile(
    core::PropertyBuilder::createProperty("Host Key File")->withDescription("If supplied, the given file will be used as the Host Key; otherwise, no use host key file will be used")
        ->isRequired(false)->build());
core::Property ListSFTP::ConnectionTimeout(
    core::PropertyBuilder::createProperty("Connection Timeout")->withDescription("Amount of time to wait before timing out while creating a connection")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property ListSFTP::DataTimeout(
    core::PropertyBuilder::createProperty("Data Timeout")->withDescription("When transferring a file between the local and remote system, this value specifies how long is allowed to elapse without any data being transferred between systems")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("30 sec")->build());
core::Property ListSFTP::SendKeepaliveOnTimeout(
    core::PropertyBuilder::createProperty("Send Keep Alive On Timeout")->withDescription("Indicates whether or not to send a single Keep Alive message when SSH socket times out")
        ->isRequired(true)->withDefaultValue<bool>(true)->build());
core::Property ListSFTP::TargetSystemTimestampPrecision(
    core::PropertyBuilder::createProperty("Target System Timestamp Precision")->withDescription("Specify timestamp precision at the target system. "
                                                                                                "Since this processor uses timestamp of entities to decide which should be listed, "
                                                                                                "it is crucial to use the right timestamp precision.")
        ->isRequired(true)
        ->withAllowableValues<std::string>({TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS,
                                            TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES})
        ->withDefaultValue(TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT)->build());
core::Property ListSFTP::ProxyType(
    core::PropertyBuilder::createProperty("Proxy Type")->withDescription("Specifies the Proxy Configuration Controller Service to proxy network requests. If set, it supersedes proxy settings configured per component. "
                                                                         "Supported proxies: HTTP + AuthN, SOCKS + AuthN")
        ->isRequired(false)
        ->withAllowableValues<std::string>({PROXY_TYPE_DIRECT,
                                            PROXY_TYPE_HTTP,
                                            PROXY_TYPE_SOCKS})
        ->withDefaultValue(PROXY_TYPE_DIRECT)->build());
core::Property ListSFTP::ProxyHost(
    core::PropertyBuilder::createProperty("Proxy Host")->withDescription("The fully qualified hostname or IP address of the proxy server")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::ProxyPort(
    core::PropertyBuilder::createProperty("Proxy Port")->withDescription("The port of the proxy server")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::HttpProxyUsername(
    core::PropertyBuilder::createProperty("Http Proxy Username")->withDescription("Http Proxy Username")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::HttpProxyPassword(
    core::PropertyBuilder::createProperty("Http Proxy Password")->withDescription("Http Proxy Password")
        ->isRequired(false)->supportsExpressionLanguage(true)->build());
core::Property ListSFTP::EntityTrackingTimeWindow(
    core::PropertyBuilder::createProperty("Entity Tracking Time Window")->withDescription("Specify how long this processor should track already-listed entities. "
                                                                                          "'Tracking Entities' strategy can pick any entity whose timestamp is inside the specified time window. "
                                                                                          "For example, if set to '30 minutes', any entity having timestamp in recent 30 minutes will be the listing target when this processor runs. "
                                                                                          "A listed entity is considered 'new/updated' and a FlowFile is emitted if one of following condition meets: "
                                                                                          "1. does not exist in the already-listed entities, "
                                                                                          "2. has newer timestamp than the cached entity, "
                                                                                          "3. has different size than the cached entity. "
                                                                                          "If a cached entity's timestamp becomes older than specified time window, that entity will be removed from the cached already-listed entities. "
                                                                                          "Used by 'Tracking Entities' strategy.")
        ->isRequired(false)->build());
core::Property ListSFTP::EntityTrackingInitialListingTarget(
    core::PropertyBuilder::createProperty("Entity Tracking Initial Listing Target")->withDescription("Specify how initial listing should be handled. Used by 'Tracking Entities' strategy.")
        ->withAllowableValues<std::string>({ENTITY_TRACKING_INITIAL_LISTING_TARGET_TRACKING_TIME_WINDOW,
                                            ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE})
        ->isRequired(false)->withDefaultValue(ENTITY_TRACKING_INITIAL_LISTING_TARGET_ALL_AVAILABLE)->build());
core::Property ListSFTP::MinimumFileAge(
    core::PropertyBuilder::createProperty("Minimum File Age")->withDescription("The minimum age that a file must be in order to be pulled; "
                                                                               "any file younger than this amount of time (according to last modification date) will be ignored")
        ->isRequired(true)->withDefaultValue<core::TimePeriodValue>("0 sec")->build());
core::Property ListSFTP::MaximumFileAge(
    core::PropertyBuilder::createProperty("Maximum File Age")->withDescription("The maximum age that a file must be in order to be pulled; "
                                                                               "any file older than this amount of time (according to last modification date) will be ignored")
        ->isRequired(false)->build()); // TODO: validator
core::Property ListSFTP::MinimumFileSize(
    core::PropertyBuilder::createProperty("Minimum File Size")->withDescription("The minimum size that a file must be in order to be pulled")
        ->isRequired(true)->withDefaultValue<core::DataSizeValue>("0 B")->build());
core::Property ListSFTP::MaximumFileSize(
    core::PropertyBuilder::createProperty("Maximum File Size")->withDescription("The maximum size that a file must be in order to be pulled")
        ->isRequired(false)->build()); // TODO: validator

core::Relationship ListSFTP::Success("success", "All FlowFiles that are received are routed to success");

const std::map<std::string, uint64_t> ListSFTP::LISTING_LAG_MAP = {
  {ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS, 1000},
  {ListSFTP::TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES, 60000},
};

void ListSFTP::initialize() {
  logger_->log_trace("Initializing FetchSFTP");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(ListingStrategy);
  properties.insert(Hostname);
  properties.insert(Port);
  properties.insert(Username);
  properties.insert(Password);
  properties.insert(PrivateKeyPath);
  properties.insert(PrivateKeyPassphrase);
  properties.insert(RemotePath);
  properties.insert(SearchRecursively);
  properties.insert(FollowSymlink);
  properties.insert(FileFilterRegex);
  properties.insert(PathFilterRegex);
  properties.insert(IgnoreDottedFiles);
  properties.insert(StrictHostKeyChecking);
  properties.insert(HostKeyFile);
  properties.insert(ConnectionTimeout);
  properties.insert(DataTimeout);
  properties.insert(SendKeepaliveOnTimeout);
  properties.insert(TargetSystemTimestampPrecision);
  properties.insert(ProxyType);
  properties.insert(ProxyHost);
  properties.insert(ProxyPort);
  properties.insert(HttpProxyUsername);
  properties.insert(HttpProxyPassword);
  properties.insert(EntityTrackingInitialListingTarget);
  properties.insert(MinimumFileAge);
  properties.insert(MaximumFileAge);
  properties.insert(MinimumFileSize);
  properties.insert(MaximumFileSize);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

ListSFTP::ListSFTP(std::string name, utils::Identifier uuid /*= utils::Identifier()*/)
    : SFTPProcessorBase(name, uuid)
    , search_recursively_(false)
    , follow_symlink_(false)
    , file_filter_regex_set_(false)
    , path_filter_regex_set_(false)
    , ignore_dotted_files_(false)
    , minimum_file_age_(0U)
    , maximum_file_age_(0U)
    , minimum_file_size_(0U)
    , maximum_file_size_(0U)
    , already_loaded_from_cache_(false)
    , last_listed_latest_entry_timestamp_(0U)
    , last_processed_latest_entry_timestamp_(0U) {
  logger_ = logging::LoggerFactory<ListSFTP>::getLogger();
}

ListSFTP::~ListSFTP() {
#ifndef WIN32
  regfree(&compiled_file_filter_regex_);
  regfree(&compiled_path_filter_regex_);
#endif
}

void ListSFTP::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string value;
  context->getProperty(ListingStrategy.getName(), listing_strategy_);
  if (!context->getProperty(SearchRecursively.getName(), value)) {
    logger_->log_error("Search Recursively attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, search_recursively_);
  }
  if (!context->getProperty(FollowSymlink.getName(), value)) {
    logger_->log_error("Follow symlink attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, follow_symlink_);
  }
  if (context->getProperty(FileFilterRegex.getName(), file_filter_regex_)) {
#ifndef WIN32
    int ret = regcomp(&compiled_file_filter_regex_, file_filter_regex_.c_str(), 0);
    if (ret != 0) {
      logger_->log_error("Failed to compile File Filter Regex \"%s\"", file_filter_regex_.c_str());
    } else {
      file_filter_regex_set_ = true;
    }
#else
    compiled_file_filter_regex_ = std::regex(file_filter_regex_);
    file_filter_regex_set_ = true;
#endif
  } else {
    file_filter_regex_set_ = false;
  }
  if (context->getProperty(PathFilterRegex.getName(), path_filter_regex_)) {
#ifndef WIN32
    int ret = regcomp(&compiled_path_filter_regex_, path_filter_regex_.c_str(), 0);
    if (ret != 0) {
      logger_->log_error("Failed to compile File Filter Regex \"%s\"", path_filter_regex_.c_str());
    } else {
      path_filter_regex_set_ = true;
    }
#else
    compiled_path_filter_regex_ = std::regex(path_filter_regex_);
    path_filter_regex_set_ = true;
#endif
  } else {
    path_filter_regex_set_ = false;
  }
  if (!context->getProperty(IgnoreDottedFiles.getName(), value)) {
    logger_->log_error("Ignore Dotted Files attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, ignore_dotted_files_);
  }
  if (!context->getProperty(StrictHostKeyChecking.getName(), value)) {
    logger_->log_error("Strict Host Key Checking attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, strict_host_checking_);
  }
  context->getProperty(HostKeyFile.getName(), host_key_file_);
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
  if (!context->getProperty(SendKeepaliveOnTimeout.getName(), value)) {
    logger_->log_error("Send Keep Alive On Timeout attribute is missing or invalid");
  } else {
    utils::StringUtils::StringToBool(value, use_keepalive_on_timeout_);
  }
  context->getProperty(TargetSystemTimestampPrecision.getName(), target_system_timestamp_precision_);
  context->getProperty(ProxyType.getName(), proxy_type_);
  context->getProperty(EntityTrackingInitialListingTarget.getName(), entity_tracking_initial_listing_target_);
  if (!context->getProperty(MinimumFileAge.getName(), value)) {
    logger_->log_error("Minimum File Age attribute is missing or invalid");
  } else {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, minimum_file_age_, unit) || !core::Property::ConvertTimeUnitToMS(minimum_file_age_, unit, minimum_file_age_)) {
      logger_->log_error("Minimum File Age attribute is invalid");
    }
  }
  if (context->getProperty(MaximumFileAge.getName(), value)) {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, maximum_file_age_, unit) || !core::Property::ConvertTimeUnitToMS(maximum_file_age_, unit, maximum_file_age_)) {
      logger_->log_error("Maximum File Age attribute is invalid");
    }
  }
  if (!context->getProperty(MinimumFileSize.getName(), minimum_file_size_)) {
    logger_->log_error("Minimum File Size attribute is invalid");
  }
  if (context->getProperty(MaximumFileSize.getName(), value)) {
    if (!core::DataSizeValue::StringToInt(value, maximum_file_size_)) {
      logger_->log_error("Maximum File Size attribute is invalid");
    }
  }

  startKeepaliveThreadIfNeeded();
}

void ListSFTP::notifyStop() {
  logger_->log_debug("Got notifyStop, stopping keepalive thread and clearing connections");
  cleanupConnectionCache();
}

ListSFTP::Child::Child()
    :directory(false) {
  memset(&attrs, 0x00, sizeof(attrs));
}

ListSFTP::Child::Child(const std::string& parent_path_, std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>&& sftp_child) {
  parent_path = parent_path_;
  std::tie(filename, std::ignore, attrs) = std::move(sftp_child);
  directory = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
}

std::string ListSFTP::Child::getPath() const {
  std::stringstream ss;
  ss << parent_path << "/" << filename;
  return ss.str();
}

bool ListSFTP::filter(const std::string& parent_path, const std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>& sftp_child) {
  const std::string& filename = std::get<0>(sftp_child);
  const LIBSSH2_SFTP_ATTRIBUTES& attrs = std::get<2>(sftp_child);
  /* This should not happen */
  if (filename.empty()) {
    logger_->log_error("Listing directory \"%s\" returned an empty child", parent_path.c_str());
    return false;
  }
  /* Ignore current dir and parent dir */
  if (filename == "." || filename == "..") {
    return false;
  }
  /* Dotted files */
  if (ignore_dotted_files_ && filename[0] == '.') {
    logger_->log_debug("Ignoring \"%s/%s\" because Ignore Dotted Files is true", parent_path.c_str(), filename.c_str());
    return false;
  }
  if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
    // TODO: maybe do a fallback stat here
    logger_->log_error("Failed to get permissions in stat for \"%s/%s\"", parent_path.c_str(), filename.c_str());
    return false;
  }
  if (LIBSSH2_SFTP_S_ISREG(attrs.permissions)) {
    return filterFile(parent_path, filename, attrs);
  } else if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
    return filterDirectory(parent_path, filename, attrs);
  } else {
    logger_->log_debug("Skipping non-regular, non-directory file \"%s/%s\"", parent_path.c_str(), filename.c_str());
    return false;
  }
}

bool ListSFTP::filterFile(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs) {
  if (!(attrs.flags & LIBSSH2_SFTP_ATTR_UIDGID) ||
      !(attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) ||
      !(attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME)) {
    // TODO: maybe do a fallback stat here
    logger_->log_error("Failed to get all attributes in stat for \"%s/%s\"", parent_path.c_str(), filename.c_str());
    return false;
  }

  /* Age */
  time_t now = time(nullptr); // TODO
  int64_t file_age = (now - attrs.mtime) * 1000;
  if (file_age < minimum_file_age_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is younger than the Minimum File Age: %ld ms < %lu ms",
        parent_path.c_str(),
        filename.c_str(),
        file_age,
        minimum_file_age_);
    return false;
  }
  if (maximum_file_age_ != 0U && file_age > maximum_file_age_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is older than the Maximum File Age: %ld ms > %lu ms",
                       parent_path.c_str(),
                       filename.c_str(),
                       file_age,
                       maximum_file_age_);
    return false;
  }

  /* Size */
  if (attrs.filesize < minimum_file_size_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is smaller than the Minimum File Size: %lu B < %lu B",
                       parent_path.c_str(),
                       filename.c_str(),
                       attrs.filesize,
                       minimum_file_size_);
    return false;
  }
  if (maximum_file_size_ != 0U && attrs.filesize > maximum_file_size_) {
    logger_->log_debug("Ignoring \"%s/%s\" because it is larger than the Maximum File Size: %lu B > %lu B",
                       parent_path.c_str(),
                       filename.c_str(),
                       attrs.filesize,
                       maximum_file_size_);
    return false;
  }

  /* File Filter Regex */
  if (file_filter_regex_set_) {
    bool match = false;
#ifndef WIN32
    int ret = regexec(&compiled_file_filter_regex_, filename.c_str(), static_cast<size_t>(0), nullptr, 0);
    match = ret == 0;
#else
    match = std::regex_match(filename, compiled_file_filter_regex_);
#endif
    if (!match) {
      logger_->log_debug("Ignoring \"%s/%s\" because it did not match the File Filter Regex \"%s\"",
                         parent_path.c_str(),
                         filename.c_str(),
                         file_filter_regex_);
      return false;
    }
  }

  return true;
}

bool ListSFTP::filterDirectory(const std::string& parent_path, const std::string& filename, const LIBSSH2_SFTP_ATTRIBUTES& attrs) {
  if (!search_recursively_) {
    return false;
  }

  /* Path Filter Regex */
  if (path_filter_regex_set_) {
    std::stringstream ss;
    ss << parent_path << "/" << filename;
    auto dir_path = ss.str();
    bool match = false;
#ifndef WIN32
    int ret = regexec(&compiled_path_filter_regex_, dir_path.c_str(), static_cast<size_t>(0), nullptr, 0);
    match = ret == 0;
#else
    match = std::regex_match(dir_path, compiled_path_filter_regex_);
#endif
    if (!match) {
      logger_->log_debug("Not recursing into \"%s\" because it did not match the Path Filter Regex \"%s\"",
                         dir_path.c_str(),
                         path_filter_regex_);
      return false;
    }
  }

  return true;
}

bool ListSFTP::createAndTransferFlowFileFromChild(
    const std::shared_ptr<core::ProcessSession>& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    const ListSFTP::Child& child) {
  /* Convert mtime to string */
  if (child.attrs.mtime > std::numeric_limits<int64_t>::max()) {
    logger_->log_error("Modification date %lu of \"%s/%s\" larger than int64_t max", child.attrs.mtime, child.parent_path.c_str(), child.filename.c_str());
    return true;
  }
  std::string mtime_str;
  if (!getDateTimeStr(static_cast<int64_t>(child.attrs.mtime), mtime_str)) {
    logger_->log_error("Failed to convert modification date %lu of \"%s/%s\" to string", child.attrs.mtime, child.parent_path.c_str(), child.filename.c_str());
    return true;
  }

  /* Create FlowFile */
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->create());
  if (flow_file == nullptr) {
    logger_->log_error("Failed to create FlowFileRecord");
    return false;
  }

  /* Set attributes */
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_HOST, hostname);
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_REMOTE_PORT, std::to_string(port));
  session->putAttribute(flow_file, ATTRIBUTE_SFTP_LISTING_USER, username);

  /* uid and gid */
  session->putAttribute(flow_file, ATTRIBUTE_FILE_OWNER, std::to_string(child.attrs.uid));
  session->putAttribute(flow_file, ATTRIBUTE_FILE_GROUP, std::to_string(child.attrs.gid));

  /* permissions */
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(4) << std::oct << (child.attrs.permissions & 0777);
  session->putAttribute(flow_file, ATTRIBUTE_FILE_PERMISSIONS, ss.str());

  /* filesize */
  session->putAttribute(flow_file, ATTRIBUTE_FILE_SIZE, std::to_string(child.attrs.filesize));

  /* mtime */
  session->putAttribute(flow_file, ATTRIBUTE_FILE_LASTMODIFIEDTIME, mtime_str);

  flow_file->updateKeyedAttribute(FILENAME, child.filename);
  flow_file->updateKeyedAttribute(PATH, child.parent_path);

  session->transfer(flow_file, Success);

  return true;
}

void ListSFTP::listByTrackingTimestamps(
    const std::shared_ptr<core::ProcessContext>& context,
    const std::shared_ptr<core::ProcessSession>& session,
    const std::string& hostname,
    uint16_t port,
    const std::string& username,
    std::vector<Child>&& files) {
  uint64_t min_timestamp_to_list = last_listed_latest_entry_timestamp_;

  if (!already_loaded_from_cache_) {
    // TODO: load from cache
    already_loaded_from_cache_ = true;
  }

  std::chrono::time_point<std::chrono::steady_clock> current_run_time = std::chrono::steady_clock::now();
  time_t now = time(nullptr);

  std::map<uint64_t /*timestamp*/, std::list<Child>> ordered_files;
  bool target_system_has_seconds = false;
  for (auto&& file : files) {
    uint64_t timestamp = file.attrs.mtime * 1000;
    target_system_has_seconds |= timestamp % 60000 != 0;

    bool new_file = min_timestamp_to_list == 0U || (timestamp >= min_timestamp_to_list && timestamp >= last_processed_latest_entry_timestamp_);
    if (new_file) {
      auto& files_for_timestamp = ordered_files[timestamp];
      files_for_timestamp.emplace_back(std::move(file));
    }
  }

  uint64_t latest_listed_entry_timestamp_this_cycle = 0U;
  size_t flow_files_created = 0U;
  if (ordered_files.size() > 0) {
    latest_listed_entry_timestamp_this_cycle = std::prev(ordered_files.end())->first;

    std::string remote_system_timestamp_precision;
    if (target_system_timestamp_precision_ == TARGET_SYSTEM_TIMESTAMP_PRECISION_AUTO_DETECT) {
      if (target_system_has_seconds) {
        logger_->log_debug("Precision auto detection detected second precision");
        remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS;
      } else {
        logger_->log_debug("Precision auto detection detected minute precision");
        remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES;
      }
    } else if (target_system_timestamp_precision_ == TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES) {
        remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_MINUTES;
    } else {
      /*
       * We only have seconds-precision timestamps, TARGET_SYSTEM_TIMESTAMP_PRECISION_MILLISECONDS makes no real sense here,
       * so we will treat it as TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS.
       */
      remote_system_timestamp_precision = TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS;
    }
    uint64_t listing_lag = LISTING_LAG_MAP.at(remote_system_timestamp_precision);
    logger_->log_debug("The listing lag is %lu ms", listing_lag);

    if (latest_listed_entry_timestamp_this_cycle == last_listed_latest_entry_timestamp_) {
      const auto& latest_files = ordered_files.at(latest_listed_entry_timestamp_this_cycle);
      if (std::chrono::duration_cast<std::chrono::milliseconds>(current_run_time - last_run_time_).count() < listing_lag ||
          (latest_listed_entry_timestamp_this_cycle == last_processed_latest_entry_timestamp_ &&
          std::all_of(latest_files.begin(), latest_files.end(), [this](const Child& child) {
            return latest_identifiers_processed_.count(child.getPath()) == 1U;
          }))) {
        context->yield();
        return;
      }
    } else {
      uint64_t minimum_reliable_timestamp = now * 1000 - listing_lag;
      if (remote_system_timestamp_precision == TARGET_SYSTEM_TIMESTAMP_PRECISION_SECONDS) {
        minimum_reliable_timestamp -= minimum_reliable_timestamp % 1000;
      } else {
        minimum_reliable_timestamp -= minimum_reliable_timestamp % 60000;
      }
      if (minimum_reliable_timestamp < latest_listed_entry_timestamp_this_cycle) {
        logger_->log_debug("Skipping files with latest timestamp because their modification date is not smaller than the minimum reliable timestamp: %lu ms >= %lu ms",
            latest_listed_entry_timestamp_this_cycle,
            minimum_reliable_timestamp);
        ordered_files.erase(latest_listed_entry_timestamp_this_cycle);
      }
    }

    for (auto& files_for_timestamp : ordered_files) {
      if (files_for_timestamp.first == last_processed_latest_entry_timestamp_) {
        /* Filter out previously processed entities. */
        for (auto it = files_for_timestamp.second.begin(); it != files_for_timestamp.second.end();) {
          if (latest_identifiers_processed_.count(it->getPath()) != 0U) {
            it = files_for_timestamp.second.erase(it);
          } else {
            ++it;
          }
        }
      }
      for (const auto& file : files_for_timestamp.second) {
        /* Create the FlowFile for this path */
        if (createAndTransferFlowFileFromChild(session, hostname, port, username, file)) {
          flow_files_created++;
        } else {
          // TODO
        }
      }
    }
  }

  if (latest_listed_entry_timestamp_this_cycle != 0U) {
    bool processed_new_files = flow_files_created > 0U;
    if (processed_new_files) {
      auto last_files_it = std::prev(ordered_files.end());
      if (last_files_it->first != last_processed_latest_entry_timestamp_) {
        latest_identifiers_processed_.clear();
      }

      for (const auto& last_file : last_files_it->second) {
        latest_identifiers_processed_.insert(last_file.getPath());
      }

      last_processed_latest_entry_timestamp_ = last_files_it->first;
    }

    last_run_time_ = current_run_time;

    if (latest_listed_entry_timestamp_this_cycle != last_listed_latest_entry_timestamp_ || processed_new_files) {
      last_listed_latest_entry_timestamp_ = latest_listed_entry_timestamp_this_cycle;
      // TODO: persist
    }
  } else {
    logger_->log_debug("There are no files to list. Yielding.");
    context->yield();
    return;
  }
}

void ListSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  /* Parse EL-supporting properties */
  std::string hostname;
  uint16_t port = 0U;
  std::string username;
  std::string password;
  std::string private_key_path;
  std::string private_key_passphrase;
  std::string remote_path;
  std::string proxy_host;
  uint16_t proxy_port = 0U;
  std::string proxy_username;
  std::string proxy_password;
  uint64_t entity_tracking_time_window = 0U;

  std::string value;
  if (!context->getProperty(Hostname.getName(), hostname)) {
    logger_->log_error("Hostname attribute is missing");
    context->yield();
    return;
  }
  if (!context->getProperty(Port.getName(), value)) {
    logger_->log_error("Port attribute is missing or invalid");
    context->yield();
    return;
  } else {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Port attribute \"%s\" is invalid", value);
      context->yield();
      return;
    } else {
      port = static_cast<uint16_t>(port_tmp);
    }
  }
  if (!context->getProperty(Username.getName(), username)) {
    logger_->log_error("Username attribute is missing");
    context->yield();
    return;
  }
  context->getProperty(Password.getName(), password);
  context->getProperty(PrivateKeyPath.getName(), private_key_path);
  context->getProperty(PrivateKeyPassphrase.getName(), private_key_passphrase);
  context->getProperty(Password.getName(), password);
  context->getProperty(RemotePath.getName(), remote_path);
  /* Remove trailing slashes */
  while (remote_path.size() > 1U && remote_path.back() == '/') {
    remote_path.resize(remote_path.size() - 1);
  }
  context->getProperty(ProxyHost.getName(), proxy_host);
  if (context->getProperty(ProxyPort.getName(), value) && !value.empty()) {
    int port_tmp;
    if (!core::Property::StringToInt(value, port_tmp) ||
        port_tmp < std::numeric_limits<uint16_t>::min() ||
        port_tmp > std::numeric_limits<uint16_t>::max()) {
      logger_->log_error("Proxy Port attribute \"%s\" is invalid", value);
      context->yield();
      return;
    } else {
      proxy_port = static_cast<uint16_t>(port_tmp);
    }
  }
  context->getProperty(HttpProxyUsername.getName(), proxy_username);
  context->getProperty(HttpProxyPassword.getName(), proxy_password);
  if (context->getProperty(EntityTrackingTimeWindow.getName(), value)) {
    core::TimeUnit unit;
    if (!core::Property::StringToTime(value, entity_tracking_time_window, unit) ||
        !core::Property::ConvertTimeUnitToMS(entity_tracking_time_window, unit, entity_tracking_time_window)) {
      /* The default is 3 hours	*/
      entity_tracking_time_window = 3 * 3600 * 1000;
      logger_->log_error("Entity Tracking Time Window attribute is invalid");
    }
  } else {
    /* The default is 3 hours	*/
    entity_tracking_time_window = 3 * 3600 * 1000;
  }

  /* Get SFTPClient from cache or create it */
  const SFTPProcessorBase::ConnectionCacheKey connection_cache_key = {hostname, port, username, proxy_type_, proxy_host, proxy_port, proxy_username};
  auto client = getOrCreateConnection(connection_cache_key,
                                      password,
                                      private_key_path,
                                      private_key_passphrase,
                                      proxy_password);
  if (client == nullptr) {
    context->yield();
    return;
  }

  /*
   * Unless we're sure that the connection is good, we don't want to put it back to the cache.
   * So we will only call this when we're sure that the connection is OK.
   */
  auto put_connection_back_to_cache = [this, &connection_cache_key, &client]() {
    addConnectionToCache(connection_cache_key, std::move(client));
  };

  std::deque<Child> directories;
  std::vector<Child> files;

  /* Add initial directory */
  Child root;
  std::tie(root.parent_path, root.filename) = utils::file::FileUtils::split_path(remote_path, true /*force_posix*/);
  root.directory = true;
  directories.emplace_back(std::move(root));

  /* Process directories */
  while (!directories.empty()) {
    auto directory = std::move(directories.front());
    directories.pop_front();

    std::string new_parent_path;
    if (directory.parent_path.empty()) {
      new_parent_path = directory.filename;
    } else {
      std::stringstream ss;
      ss << directory.parent_path << "/" << directory.filename;
      new_parent_path = ss.str();
    }
    std::vector<std::tuple<std::string /* filename */, std::string /* longentry */, LIBSSH2_SFTP_ATTRIBUTES /* attrs */>> dir_children;
    if (!client->listDirectory(new_parent_path, follow_symlink_, dir_children)) {
      continue;
    }
    for (auto&& dir_child : dir_children) {
      if (filter(new_parent_path, dir_child)) {
        Child child(new_parent_path, std::move(dir_child));
        if (child.directory) {
          directories.emplace_back(std::move(child));
        } else {
          files.emplace_back(std::move(child));
        }
      }
    }
  }

//  for (const auto& file : files) {
//    if (!createAndTransferFlowFileFromChild(session, hostname, port, username, file)) {
//      context->yield();
//      return;
//    }
//  }
  if (listing_strategy_ == LISTING_STRATEGY_TRACKING_TIMESTAMPS) {
    listByTrackingTimestamps(context, session, hostname, port, username, std::move(files));
  } else {
    // TODO: entity tracking
  }

  put_connection_back_to_cache();
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
