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
#ifndef __SOURCE_INITIATED_SUBSCRIPTION_PROCESSOR_H__
#define __SOURCE_INITIATED_SUBSCRIPTION_PROCESSOR_H__

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include <CivetServer.h>
extern "C" {
#include "wsman-xml.h"
}

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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class SourceInitiatedSubscription : public core::Processor {
 public:
  static constexpr char const *INITIAL_EXISTING_EVENTS_STRATEGY_NONE = "None";
  static constexpr char const *INITIAL_EXISTING_EVENTS_STRATEGY_ALL = "All";

  static constexpr char const* ProcessorName = "SourceInitiatedSubscription";

  SourceInitiatedSubscription(std::string name, utils::Identifier uuid = utils::Identifier());
  virtual ~SourceInitiatedSubscription();

  // Supported Properties
  static core::Property ListenHostname;
  static core::Property ListenPort;
  static core::Property SubscriptionManagerPath;
  static core::Property SubscriptionsBasePath;
  static core::Property SSLCertificate;
  static core::Property SSLCertificateAuthority;
  static core::Property SSLVerifyPeer;
  static core::Property XPathXmlQuery;
  static core::Property InitialExistingEventsStrategy;
  static core::Property StateFile;

  // Supported Relationships
  static core::Relationship Success;

  // Writes Attributes
  static constexpr char const* ATTRIBUTE_WEF_REMOTE_MACHINEID = "wef.remote.machineid";
  static constexpr char const* ATTRIBUTE_WEF_REMOTE_IP = "wef.remote.ip";

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  virtual void notifyStop() override;
  
  class Handler: public CivetHandler {
   public:
    Handler(SourceInitiatedSubscription& processor);
    bool handlePost(CivetServer* server, struct mg_connection* conn);
    
    class WriteCallback : public OutputStreamCallback {
     public:
      WriteCallback(char* text);
      int64_t process(std::shared_ptr<io::BaseStream> stream);

     private:
      char* text_;
    };

   private:
    SourceInitiatedSubscription& processor_;
    
    bool handleSubscriptionManager(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request);
    bool handleSubscriptions(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request);
    
    static int enumerateEventCallback(WsXmlNodeH node, void* data);
    std::string getSoapAction(WsXmlDocH doc);
    std::string getMachineId(WsXmlDocH doc);
    void sendResponse(struct mg_connection* conn, const std::string& machineId, const std::string& remoteIp, char* xml_buf, size_t xml_buf_size);
  };

 protected:
  std::shared_ptr<logging::Logger> logger_;
  
  std::shared_ptr<utils::IdGenerator> id_generator_;
  
  std::shared_ptr<core::ProcessSessionFactory> session_factory_;

  std::string listen_hostname_;
  uint16_t listen_port_;
  std::string subscription_manager_path_;
  std::string subscriptions_base_path_;
  std::string ssl_ca_cert_thumbprint_;
  std::string xpath_xml_query_;
  std::string initial_existing_events_strategy_;
  std::string state_file_path_;

  std::unique_ptr<CivetServer> server_;
  std::unique_ptr<Handler> handler_;
};

REGISTER_RESOURCE(SourceInitiatedSubscription, "SourceInitiatedSubscription TODO")

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif // __SOURCE_INITIATED_SUBSCRIPTION_PROCESSOR_H__
