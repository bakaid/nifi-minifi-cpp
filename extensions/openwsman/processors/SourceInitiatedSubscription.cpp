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

#include "SourceInitiatedSubscription.h"

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

extern "C" {
#include "wsman-api.h"
#include "wsman-xml.h"
#include "wsman-xml-api.h"
#include "wsman-xml-serialize.h"
#include "wsman-xml-serializer.h"
#include "wsman-soap.h"
#include "wsman-soap-envelope.h"
}

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

#define XML_NS_CUSTOM_SUBSCRIPTION "http://schemas.microsoft.com/wbem/wsman/1/subscription"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {
core::Property SourceInitiatedSubscription::ListenHostname(
    core::PropertyBuilder::createProperty("Listen Hostname")->withDescription("The hostname or IP of this machine that will be advertised to event sources to connect to. It must be contained as a Subject Alternative Name in the server certificate, otherwise source machines will refuse to connect.")
        ->isRequired(true)->build());
core::Property SourceInitiatedSubscription::ListenPort(
    core::PropertyBuilder::createProperty("Listen Port")->withDescription("The port to listen on.")
        ->isRequired(true)->withDefaultValue<uint64_t>(5986U, core::StandardValidators::LISTEN_PORT_VALIDATOR())->build());
core::Property SourceInitiatedSubscription::SubscriptionManagerPath(
    core::PropertyBuilder::createProperty("Subscription Manager Path")->withDescription("The URI path that will be used for the WEC Subscription Manager endpoint.")
        ->isRequired(true)->withDefaultValue("/wsman/SubscriptionManager/WEC")->build());
core::Property SourceInitiatedSubscription::SubscriptionsBasePath(
    core::PropertyBuilder::createProperty("Subscriptions Base Path")->withDescription("The URI path that will be used as the base for endpoints serving individual subscriptions.")
        ->isRequired(true)->withDefaultValue("/wsman/subscriptions")->build());
core::Property SourceInitiatedSubscription::SSLCertificate(
    core::PropertyBuilder::createProperty("SSL Certificate")->withDescription("File containing PEM-formatted file including TLS/SSL certificate and key. The root CA of the certificate must be the CA set in SSL Certificate Authority.")
        ->isRequired(true)->build());
core::Property SourceInitiatedSubscription::SSLCertificateAuthority(
    core::PropertyBuilder::createProperty("SSL Certificate Authority")->withDescription("File containing the PEM-formatted CA that is the root CA for both this server's certificate and the event source clients' certificates.")
        ->isRequired(true)->build());
core::Property SourceInitiatedSubscription::SSLVerifyPeer(
    core::PropertyBuilder::createProperty("SSL Verify Peer")->withDescription("Whether or not to verify the client's certificate")
        ->isRequired(false)->withDefaultValue<bool>(true)->build());
core::Property SourceInitiatedSubscription::XPathXmlQuery(
    core::PropertyBuilder::createProperty("XPath XML Query")->withDescription("An XPath Query in structured XML format conforming to the Query Schema described in https://docs.microsoft.com/en-gb/windows/win32/wes/queryschema-schema, "
    "see an example here: https://docs.microsoft.com/en-gb/windows/win32/wes/consuming-events")
        ->isRequired(true)
        ->withDefaultValue("QueryList>\n"
                           "  <Query Id=\"0\">\n"
                           "    <Select Path=\"Application\">*</Select>\n"
                           "  </Query>\n"
                           "</QueryList>\n")->build());
core::Property SourceInitiatedSubscription::InitialExistingEventsStrategy(
    core::PropertyBuilder::createProperty("Initial Existing Events Strategy")->withDescription("Defines the behaviour of the Processor when a new event source connects.\n"
    "None: will not request existing events\n"
    "All: will request all existing events matching the query")
        ->isRequired(true)->withAllowableValues<std::string>({INITIAL_EXISTING_EVENTS_STRATEGY_NONE, INITIAL_EXISTING_EVENTS_STRATEGY_ALL})->withDefaultValue(INITIAL_EXISTING_EVENTS_STRATEGY_NONE)->build());
core::Property SourceInitiatedSubscription::StateFile(
    core::PropertyBuilder::createProperty("State File")->withDescription("The file the Processor will use to store the current bookmark for each event source. "
    "This will be used after restart to continue event ingestion from the point the Processor left off.")
        ->isRequired(true)->build());

core::Relationship SourceInitiatedSubscription::Success("success", "All Events are routed to success");


SourceInitiatedSubscription::SourceInitiatedSubscription(std::string name, utils::Identifier uuid)
    : Processor(name, uuid)
    , logger_(logging::LoggerFactory<SourceInitiatedSubscription>::getLogger())
    , session_factory_(nullptr)
    , listen_port_(0U) {
}

SourceInitiatedSubscription::~SourceInitiatedSubscription() {
}

SourceInitiatedSubscription::Handler::Handler(SourceInitiatedSubscription& processor)
    : processor_(processor) {
}

bool SourceInitiatedSubscription::Handler::handlePost(CivetServer* server, struct mg_connection* conn) {
  const struct mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    return false;
  }
  
  const char* endpoint = req_info->local_uri;
  if (endpoint == nullptr) {
    return false;
  }
  std::cerr << "Endpoint: " << endpoint << std::endl;
  
  for (int i = 0; i < req_info->num_headers; i++) {
    std::cerr << "Header: " << req_info->http_headers[i].name << ": " << req_info->http_headers[i].value << std::endl;
  }

  const char* content_type = mg_get_header(conn, "Content-Type");
  if (content_type == nullptr) {
    return false;
  }

  std::cerr << "Content-Type: \"" << content_type << "\"" << std::endl;
  
  const char* charset = strstr(content_type, "charset=");
  if (charset == nullptr) {
    return false;
  } else {
    charset += strlen("charset=");
    // TODO: might be other things after a ';'
  }
  
  std::cerr << "charset: \"" << charset << "\"" << std::endl;

  std::vector<uint8_t> raw_data;
  std::array<uint8_t, 16384U> buf;
  int read_bytes;
  while ((read_bytes = mg_read(conn, buf.data(), buf.size())) > 0) {
    raw_data.insert(raw_data.end(), buf.begin(), buf.begin() + read_bytes); // TODO
//       std::cerr << "Body: " << std::string(reinterpret_cast<char*>(buf.data()), read_bytes);
//       fwrite(buf.data(), 1U, read_bytes, stdout);
  }
  
  WsXmlDocH doc = ws_xml_read_memory(reinterpret_cast<char*>(raw_data.data()), raw_data.size(), charset, 0);
  WsXmlNodeH node = ws_xml_get_doc_root(doc);
  char* xml_buf = nullptr;
  int xml_buf_size = 0;
  ws_xml_dump_memory_node_tree_enc(node, &xml_buf, &xml_buf_size, "UTF-8");
  if (xml_buf != nullptr) {
    std::cerr << std::string(xml_buf, xml_buf_size);
    free(xml_buf);
  }
  
  if (strcmp(endpoint, "/wsman/subscriptions/07C41EF8-1EE6-4519-86C5-47A78FB16DED") == 0) {
      WsXmlDocH ack = wsman_create_response_envelope(doc, "http://schemas.dmtf.org/wbem/wsman/1/wsman/Ack");
      WsXmlNodeH ack_header = ws_xml_get_soap_header(ack);
      ws_xml_add_child(ack_header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:06D6A1CD-A99D-441C-8A8C-5571844C4D10");
    
    // Send ACK
    xml_buf = nullptr;
    xml_buf_size = 0;
    ws_xml_dump_memory_enc(ack, &xml_buf, &xml_buf_size, "UTF-8");
    ws_xml_dump_doc(stderr, ack);
    
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
    mg_printf(conn, "Content-Type: application/soap+xml;charset=UTF-8\r\n");
    mg_printf(conn, "Authorization: http://schemas.dmtf.org/wbem/wsman/1/wsman/secprofile/https/mutual\r\n");
    mg_printf(conn, "Content-Length: %d\r\n", xml_buf_size);
    mg_printf(conn, "\r\n");
    mg_printf(conn, "%.*s", xml_buf_size, xml_buf);
    
    return true;
  } else if (strcmp(endpoint, "/wsman/SubscriptionManager/WEC") != 0) {
    return true;
  }

  WsXmlDocH response = wsman_create_response_envelope(doc, nullptr);
  
  WsXmlNodeH response_header = ws_xml_get_soap_header(response);
  ws_xml_add_child(response_header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:06D6A1CD-A99D-441C-8A8C-5571844C4D09");
  
  WsXmlNodeH response_body = ws_xml_get_soap_body(response);
  WsXmlNodeH enumeration_response = ws_xml_add_child(response_body, XML_NS_ENUMERATION, WSENUM_ENUMERATE_RESP, nullptr);
  ws_xml_add_child(enumeration_response, XML_NS_ENUMERATION, WSENUM_ENUMERATION_CONTEXT, nullptr);
  WsXmlNodeH enumeration_items = ws_xml_add_child(enumeration_response, XML_NS_WS_MAN, WSENUM_ITEMS, nullptr);
  ws_xml_add_child(enumeration_response, XML_NS_WS_MAN, WSENUM_END_OF_SEQUENCE, nullptr);
  
  WsXmlNodeH subscription = ws_xml_add_child(enumeration_items, nullptr, "Subscription", nullptr);
  ws_xml_set_ns(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "m");

  ws_xml_add_child(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "Version", "uuid:BB8CD0E7-46F4-40E4-B74C-A0C7B509F692");

  // Subscription
  WsXmlDocH subscription_item = ws_xml_create_envelope();
  {
  WsXmlNodeH header = ws_xml_get_soap_header(subscription_item);
  WsXmlNodeH node;

  node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_ACTION, EVT_ACTION_SUBSCRIBE);
  ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");
  
  ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:346A0039-0C21-465E-8ABD-CF89EE730FA7");

  node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_TO, WSA_TO_ANONYMOUS);
  ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

  node = ws_xml_add_child(header, XML_NS_WS_MAN, WSM_RESOURCE_URI, "http://schemas.microsoft.com/wbem/wsman/1/windows/EventLog");
  ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

  node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_REPLY_TO, nullptr);
  node = ws_xml_add_child(node, XML_NS_ADDRESSING, WSA_ADDRESS, WSA_TO_ANONYMOUS);
  ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");
  
  WsXmlNodeH option_set = ws_xml_add_child(header, XML_NS_WS_MAN, WSM_OPTION_SET, nullptr);
  ws_xml_ns_add(option_set, XML_NS_SCHEMA_INSTANCE, XML_NS_SCHEMA_INSTANCE_PREFIX);

  node = ws_xml_add_child(option_set, XML_NS_WS_MAN, WSM_OPTION, nullptr);
  ws_xml_add_node_attr(node, nullptr, WSM_NAME, "CDATA");
  ws_xml_add_node_attr(node, XML_NS_SCHEMA_INSTANCE, XML_SCHEMA_NIL, "true");

  node = ws_xml_add_child(option_set, XML_NS_WS_MAN, WSM_OPTION, nullptr);
  ws_xml_add_node_attr(node, nullptr, WSM_NAME, "IgnoreChannelError");
  ws_xml_add_node_attr(node, XML_NS_SCHEMA_INSTANCE, XML_SCHEMA_NIL, "true");

  node = ws_xml_add_child(option_set, XML_NS_WS_MAN, WSM_OPTION, "true");
  ws_xml_add_node_attr(node, nullptr, WSM_NAME, "ReadExistingEvents");

  WsXmlNodeH body = ws_xml_get_soap_body(subscription_item);
  WsXmlNodeH subscribe_node = ws_xml_add_child(body, XML_NS_EVENTING, WSEVENT_SUBSCRIBE, nullptr);
  
  // EndTo
  WsXmlNodeH endto_node = ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_ENDTO, nullptr);
  {
    ws_xml_add_child(endto_node, XML_NS_ADDRESSING, WSA_ADDRESS, "https://23.96.27.78:5986/wsman/subscriptions/07C41EF8-1EE6-4519-86C5-47A78FB16DED");
    node = ws_xml_add_child(endto_node, XML_NS_ADDRESSING, WSA_REFERENCE_PROPERTIES, nullptr);
    ws_xml_add_child(node, XML_NS_EVENTING, WSEVENT_IDENTIFIER, "430055A3-8146-49AA-A5C1-D87DC542AB0C");
  }
  
  // Delivery
  WsXmlNodeH delivery_node = ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_DELIVERY, nullptr);
  ws_xml_add_node_attr(delivery_node, nullptr, WSEVENT_DELIVERY_MODE, WSEVENT_DELIVERY_MODE_EVENTS);

  ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_HEARTBEATS, "PT10.000S");
  
  WsXmlNodeH notify_node = ws_xml_add_child(delivery_node, XML_NS_EVENTING, WSEVENT_NOTIFY_TO, nullptr);
  {
    ws_xml_add_child(notify_node, XML_NS_ADDRESSING, WSA_ADDRESS, "https://23.96.27.78:5986/wsman/subscriptions/07C41EF8-1EE6-4519-86C5-47A78FB16DED");
    node = ws_xml_add_child(notify_node, XML_NS_ADDRESSING, WSA_REFERENCE_PROPERTIES, nullptr);
    ws_xml_add_child(node, XML_NS_EVENTING, WSEVENT_IDENTIFIER, "430055A3-8146-49AA-A5C1-D87DC542AB0C");
    // Policy
    const char* legacy_policy_ns = "http://schemas.xmlsoap.org/ws/2002/12/policy";
    const char* authentication_ns = "http://schemas.microsoft.com/wbem/wsman/1/authentication";
    WsXmlNodeH policy = ws_xml_add_child(notify_node, nullptr, "Policy", nullptr);
    ws_xml_set_ns(policy, legacy_policy_ns, "c");
    ws_xml_ns_add(policy, authentication_ns, "auth");
    WsXmlNodeH exactly_one = ws_xml_add_child(policy, legacy_policy_ns, "ExactlyOne", nullptr);
    WsXmlNodeH all = ws_xml_add_child(exactly_one, legacy_policy_ns, "All", nullptr);
    WsXmlNodeH authentication = ws_xml_add_child(all, authentication_ns, "Authentication", nullptr);
    ws_xml_add_node_attr(authentication, nullptr, "Profile", "http://schemas.dmtf.org/wbem/wsman/1/wsman/secprofile/https/mutual");
    WsXmlNodeH client_certificate = ws_xml_add_child(authentication, authentication_ns, "ClientCertificate", nullptr);
    WsXmlNodeH thumbprint = ws_xml_add_child(client_certificate, authentication_ns, "Thumbprint", "EFA9F12309CEA6EAD08699B3B72E49F7F5B7185C");
    ws_xml_add_node_attr(thumbprint, nullptr, "Role", "issuer");
  }
  
  ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_MAX_ELEMENTS, "20");

  // Filter
  WsXmlNodeH filter_node = ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_FILTER, nullptr);
  WsXmlNodeH query_list = ws_xml_add_child(filter_node, nullptr, "QueryList", nullptr);
  WsXmlNodeH query = ws_xml_add_child(query_list, nullptr, "Query", nullptr);
  ws_xml_add_node_attr(query, nullptr, "Id", "0");
  WsXmlNodeH select = ws_xml_add_child(query, nullptr, "Select", "*");
  ws_xml_add_node_attr(select, nullptr, "Path", "Application");
  
  // Send Bookmarks
  ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_SENDBOOKMARKS, nullptr);
  }

  //
  WsXmlNodeH subscription_node = ws_xml_get_doc_root(subscription_item);
  ws_xml_copy_node(subscription_node, subscription);
  
  
  // Send response
  xml_buf = nullptr;
  xml_buf_size = 0;
  ws_xml_dump_memory_enc(response, &xml_buf, &xml_buf_size, "UTF-8");
  
//     std::cerr << "Response: " << std::string(xml_buf, xml_buf_size);
      ws_xml_dump_doc(stderr, response);
  
  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  mg_printf(conn, "Content-Type: application/soap+xml;charset=UTF-8\r\n");
  mg_printf(conn, "Authorization: http://schemas.dmtf.org/wbem/wsman/1/wsman/secprofile/https/mutual\r\n");
  mg_printf(conn, "Content-Length: %d\r\n", xml_buf_size);
  mg_printf(conn, "\r\n");
  mg_printf(conn, "%.*s", xml_buf_size, xml_buf);

  return true;
}

void SourceInitiatedSubscription::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
    logger_->log_trace("SourceInitiatedSubscription onTrigger called");
}

void SourceInitiatedSubscription::initialize() {
  logger_->log_trace("Initializing SourceInitiatedSubscription");

  // Set the supported properties
  std::set<core::Property> properties;
  properties.insert(ListenHostname);
  properties.insert(ListenPort);
  properties.insert(SubscriptionManagerPath);
  properties.insert(SubscriptionsBasePath);
  properties.insert(SSLCertificate);
  properties.insert(SSLCertificateAuthority);
  properties.insert(SSLVerifyPeer);
  properties.insert(XPathXmlQuery);
  properties.insert(InitialExistingEventsStrategy);
  properties.insert(StateFile);
  setSupportedProperties(properties);

  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void SourceInitiatedSubscription::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  std::string ssl_certificate_file;
  std::string ssl_ca_file;
  bool verify_peer = true;
    
  std::string value;
  context->getProperty(ListenHostname.getName(), listen_hostname_);
  if (!context->getProperty(ListenPort.getName(), value)) {
    logger_->log_error("Listen Port attribute is missing or invalid");
    return;
  } else {
    core::Property::StringToInt(value, listen_port_);
  }
  context->getProperty(SubscriptionManagerPath.getName(), subscription_manager_path_);
  context->getProperty(SubscriptionsBasePath.getName(), subscriptions_base_path_);
  if (!context->getProperty(SSLCertificate.getName(), ssl_certificate_file)) {
      logger_->log_error("SSL Certificate attribute is missing");
      return;
  }
  if (!context->getProperty(SSLCertificateAuthority.getName(), ssl_ca_file)) {
      logger_->log_error("SSL Certificate Authority attribute is missing");
      return;
  }
  if (!context->getProperty(SSLVerifyPeer.getName(), value)) {
    logger_->log_error("SSL Verify Peer attribute is missing or invalid");
    return;
  } else {
    utils::StringUtils::StringToBool(value, verify_peer);
  }
  context->getProperty(XPathXmlQuery.getName(), xpath_xml_query_);
  if (!context->getProperty(InitialExistingEventsStrategy.getName(), initial_existing_events_strategy_)) {
    logger_->log_error("Initial Existing Events Strategy attribute is missing or invalid");
    return;
  }
  if (!context->getProperty(StateFile.getName(), state_file_path_)) {
    logger_->log_error("State File attribute is missing or invalid");
    return;
  }

  session_factory_ = sessionFactory;
}

void SourceInitiatedSubscription::notifyStop() {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
