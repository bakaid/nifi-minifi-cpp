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
#include "utils/ScopeGuard.h"
#include "utils/file/FileUtils.h"

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
    , ignore_dotted_files_(false)
    , minimum_file_age_(0U)
    , maximum_file_age_(0U)
    , minimum_file_size_(0U)
    , maximum_file_size_(0U) {
  logger_ = logging::LoggerFactory<ListSFTP>::getLogger();
}

ListSFTP::~ListSFTP() {
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
  context->getProperty(FileFilterRegex.getName(), file_filter_regex_);
  context->getProperty(PathFilterRegex.getName(), path_filter_regex_);
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
  context->getProperty(MaximumFileSize.getName(), maximum_file_size_);

  startKeepaliveThreadIfNeeded();
}

void ListSFTP::notifyStop() {
  logger_->log_debug("Got notifyStop, stopping keepalive thread and clearing connections");
  cleanupConnectionCache();
}

void ListSFTP::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord>(session->get());
  if (flow_file == nullptr) {
    return;
  }

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
      logger_->log_error("Port attribute \"%s\" is invalid", value);
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
  context->getProperty(ProxyHost, proxy_host, flow_file);
  if (context->getProperty(ProxyPort, value, flow_file) && !value.empty()) {
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
  context->getProperty(HttpProxyUsername, proxy_username, flow_file);
  context->getProperty(HttpProxyPassword, proxy_password, flow_file);
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



  session->transfer(flow_file, Success);
  put_connection_back_to_cache();
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
