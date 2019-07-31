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

#include <openssl/x509.h>
extern "C" {
#include "wsman-api.h"
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
#define XML_NS_CUSTOM_AUTHENTICATION "http://schemas.microsoft.com/wbem/wsman/1/authentication"
#define XML_NS_CUSTOM_POLICY "http://schemas.xmlsoap.org/ws/2002/12/policy"
#define XML_NS_CUSTOM_MACHINEID "http://schemas.microsoft.com/wbem/wsman/1/machineid"
#define WSMAN_CUSTOM_ACTION_ACK "http://schemas.dmtf.org/wbem/wsman/1/wsman/Ack"
#define WSMAN_CUSTOM_ACTION_HEARTBEAT "http://schemas.dmtf.org/wbem/wsman/1/wsman/Heartbeat"
#define WSMAN_CUSTOM_ACTION_EVENTS "http://schemas.dmtf.org/wbem/wsman/1/wsman/Events"

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
        ->isRequired(true)->withDefaultValue<int64_t>(5986, core::StandardValidators::LISTEN_PORT_VALIDATOR())->build());
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
        ->withDefaultValue("<QueryList>\n"
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
    , id_generator_(utils::IdGenerator::getIdGenerator())
    , session_factory_(nullptr)
    , listen_port_(0U) {
}

SourceInitiatedSubscription::~SourceInitiatedSubscription() {
}

SourceInitiatedSubscription::Handler::Handler(SourceInitiatedSubscription& processor)
    : processor_(processor) {
}

SourceInitiatedSubscription::SubscriberData::SubscriberData()
    : bookmark(nullptr)
    , subscription(nullptr) {
}

SourceInitiatedSubscription::SubscriberData::~SubscriberData() {
  if (bookmark != nullptr) {
    ws_xml_destroy_doc(bookmark);
  }
  if (subscription != nullptr) {
    ws_xml_destroy_doc(subscription);
  }
}

bool SourceInitiatedSubscription::Handler::handlePost(CivetServer* server, struct mg_connection* conn) {
  const struct mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    processor_.logger_->log_error("Failed to get request info");
    return false;
  }

  const char* endpoint = req_info->local_uri;
  if (endpoint == nullptr) {
    processor_.logger_->log_error("Failed to get called endpoint (local_uri)");
    return false;
  }
  processor_.logger_->log_trace("Endpoint \"%s\" has been called", endpoint);

  for (int i = 0; i < req_info->num_headers; i++) {
    processor_.logger_->log_trace("Received header \"%s: %s\"", req_info->http_headers[i].name, req_info->http_headers[i].value);
  }

  const char* content_type = mg_get_header(conn, "Content-Type");
  if (content_type == nullptr) {
    processor_.logger_->log_error("Content-Type header missing");
    return false;
  }
  
  std::string charset;
  const char* charset_begin = strstr(content_type, "charset=");
  if (charset_begin == nullptr) {
    processor_.logger_->log_warn("charset missing from Content-Type header, assuming UTF-8");
    charset = "UTF-8";
  } else {
    charset_begin += strlen("charset=");
    const char* charset_end = strchr(charset_begin, ';');
    if (charset_end == nullptr) {
        charset = std::string(charset_begin);
    } else {
        charset = std::string(charset_begin, charset_end - charset_begin);
    }
  }
  processor_.logger_->log_trace("charset is \"%s\"", charset.c_str());

  std::vector<uint8_t> raw_data;
  {
    std::array<uint8_t, 16384U> buf;
    int read_bytes;
    while ((read_bytes = mg_read(conn, buf.data(), buf.size())) > 0) {
      size_t orig_size = raw_data.size();
      raw_data.resize(orig_size + read_bytes);
      memcpy(raw_data.data() + orig_size, buf.data(), read_bytes);
    }
  }

  if (raw_data.empty()) {
    processor_.logger_->log_error("POST body is empty");
    return false;
  }

  WsXmlDocH doc = ws_xml_read_memory(reinterpret_cast<char*>(raw_data.data()), raw_data.size(), charset.c_str(), 0);

  if (doc == nullptr) {
    processor_.logger_->log_error("Failed to parse POST body as XML");
    return false;
  }

  {
    WsXmlNodeH node = ws_xml_get_doc_root(doc);
    char* xml_buf = nullptr;
    int xml_buf_size = 0;
    ws_xml_dump_memory_node_tree_enc(node, &xml_buf, &xml_buf_size, "UTF-8");
    if (xml_buf != nullptr) {
        logging::LOG_TRACE(processor_.logger_) << "Received request: \"" << std::string(xml_buf, xml_buf_size) << "\"";
        ws_xml_free_memory(xml_buf);
    }
  }

  if (endpoint == processor_.subscription_manager_path_) {
    return this->handleSubscriptionManager(conn, endpoint, doc);
  } else if (strncmp(endpoint, processor_.subscriptions_base_path_.c_str(), processor_.subscriptions_base_path_.length()) == 0) {
    return this->handleSubscriptions(conn, endpoint, doc);
  } else {
    ws_xml_destroy_doc(doc);
    return false;
  }
}

std::string SourceInitiatedSubscription::Handler::getSoapAction(WsXmlDocH doc) {
  WsXmlNodeH header = ws_xml_get_soap_header(doc);
  if (header == nullptr) {
    return "";
  }
  WsXmlNodeH action_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_ADDRESSING, WSA_ACTION);
  if (action_node == nullptr) {
    return "";
  }
  char* text = ws_xml_get_node_text(action_node);
  if (text == nullptr) {
    return "";
  }

  return text;
}

std::string SourceInitiatedSubscription::Handler::getMachineId(WsXmlDocH doc) {
  WsXmlNodeH header = ws_xml_get_soap_header(doc);
  if (header == nullptr) {
    return "";
  }
  WsXmlNodeH machineid_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_CUSTOM_MACHINEID, "MachineID");
  if (machineid_node == nullptr) {
    return "";
  }
  char* text = ws_xml_get_node_text(machineid_node);
  if (text == nullptr) {
    return "";
  }
  
  return text;
}

void SourceInitiatedSubscription::Handler::sendResponse(struct mg_connection* conn, const std::string& machineId, const std::string& remoteIp, char* xml_buf, size_t xml_buf_size) {
  logging::LOG_TRACE(processor_.logger_) << "Sending response to " << machineId << " (" << remoteIp << "): \"" << std::string(xml_buf, xml_buf_size) << "\"";

  mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  mg_printf(conn, "Content-Type: application/soap+xml;charset=UTF-8\r\n");
  mg_printf(conn, "Authorization: %s\r\n", WSMAN_SECURITY_PROFILE_HTTPS_MUTUAL);
  mg_printf(conn, "Content-Length: %d\r\n", xml_buf_size);
  mg_printf(conn, "\r\n");
  mg_printf(conn, "%.*s", xml_buf_size, xml_buf);
}

bool SourceInitiatedSubscription::Handler::handleSubscriptionManager(struct mg_connection* conn, const std::string& /*endpoint*/, WsXmlDocH request) {
  utils::ScopeGuard guard([&]() {
      ws_xml_destroy_doc(request);
  });

  auto machine_id = getMachineId(request);

  WsXmlDocH response = wsman_create_response_envelope(request, nullptr);

  WsXmlNodeH response_header = ws_xml_get_soap_header(response);
  utils::Identifier msg_id = processor_.id_generator_->generate();
  ws_xml_add_child_format(response_header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:%s", msg_id.to_string().c_str());

  WsXmlNodeH response_body = ws_xml_get_soap_body(response);
  WsXmlNodeH enumeration_response = ws_xml_add_child(response_body, XML_NS_ENUMERATION, WSENUM_ENUMERATE_RESP, nullptr);
  ws_xml_add_child(enumeration_response, XML_NS_ENUMERATION, WSENUM_ENUMERATION_CONTEXT, nullptr);
  WsXmlNodeH enumeration_items = ws_xml_add_child(enumeration_response, XML_NS_WS_MAN, WSENUM_ITEMS, nullptr);
  ws_xml_add_child(enumeration_response, XML_NS_WS_MAN, WSENUM_END_OF_SEQUENCE, nullptr);

  WsXmlNodeH subscription = ws_xml_add_child(enumeration_items, nullptr, "Subscription", nullptr);
  ws_xml_set_ns(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "m");

  // Subscription version
  std::lock_guard<std::mutex> lock(processor_.mutex_);
  auto it = processor_.subscribers_.find(machine_id);

  std::string subscription_version;
  if (it != processor_.subscribers_.end() && it->second.subscription != nullptr) {
    subscription_version = it->second.subscription_version;
  } else {
    utils::Identifier id = processor_.id_generator_->generate();
    subscription_version = id.to_string();
  }
  ws_xml_add_child_format(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "Version", "uuid:%s", subscription_version.c_str());

  // Subscription
  if (it != processor_.subscribers_.end() && it->second.subscription != nullptr) {
    WsXmlNodeH subscription_node = ws_xml_get_doc_root(it->second.subscription);
    ws_xml_copy_node(subscription_node, subscription);
  } else {
    WsXmlDocH subscription_doc = ws_xml_create_envelope();

    WsXmlNodeH header = ws_xml_get_soap_header(subscription_doc);
    WsXmlNodeH node;

    node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_ACTION, EVT_ACTION_SUBSCRIBE);
    ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");

    utils::Identifier msg_id = processor_.id_generator_->generate();
    ws_xml_add_child_format(header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:%s", msg_id.to_string().c_str());

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

    // node = ws_xml_add_child(option_set, XML_NS_WS_MAN, WSM_OPTION, "true");
    // ws_xml_add_node_attr(node, nullptr, WSM_NAME, "ReadExistingEvents");

    WsXmlNodeH body = ws_xml_get_soap_body(subscription_doc);
    WsXmlNodeH subscribe_node = ws_xml_add_child(body, XML_NS_EVENTING, WSEVENT_SUBSCRIBE, nullptr);
    
    // EndTo
    utils::Identifier subscription_id = processor_.id_generator_->generate(); // TODO
    utils::Identifier event_id = processor_.id_generator_->generate(); // TODO
    WsXmlNodeH endto_node = ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_ENDTO, nullptr);
    {
      ws_xml_add_child_format(endto_node, XML_NS_ADDRESSING, WSA_ADDRESS, "https://%s:%hu%s/%s",
                              processor_.listen_hostname_.c_str(),
                              processor_.listen_port_,
                              processor_.subscriptions_base_path_.c_str(),
                              subscription_id.to_string().c_str()
                            );
      node = ws_xml_add_child(endto_node, XML_NS_ADDRESSING, WSA_REFERENCE_PROPERTIES, nullptr);
      ws_xml_add_child_format(node, XML_NS_EVENTING, WSEVENT_IDENTIFIER, "%s", event_id.to_string().c_str());
    }
    
    // Delivery
    WsXmlNodeH delivery_node = ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_DELIVERY, nullptr);
    ws_xml_add_node_attr(delivery_node, nullptr, WSEVENT_DELIVERY_MODE, WSEVENT_DELIVERY_MODE_EVENTS);

    ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_HEARTBEATS, "PT10.000S");
    
    WsXmlNodeH notify_node = ws_xml_add_child(delivery_node, XML_NS_EVENTING, WSEVENT_NOTIFY_TO, nullptr);
    {
      ws_xml_add_child_format(notify_node, XML_NS_ADDRESSING, WSA_ADDRESS, "https://%s:%hu%s/%s",
                              processor_.listen_hostname_.c_str(),
                              processor_.listen_port_,
                              processor_.subscriptions_base_path_.c_str(),
                              subscription_id.to_string().c_str()
                            );
      node = ws_xml_add_child(notify_node, XML_NS_ADDRESSING, WSA_REFERENCE_PROPERTIES, nullptr);
      ws_xml_add_child_format(node, XML_NS_EVENTING, WSEVENT_IDENTIFIER, "%s", event_id.to_string().c_str());
      // Policy
      WsXmlNodeH policy = ws_xml_add_child(notify_node, nullptr, "Policy", nullptr);
      ws_xml_set_ns(policy, XML_NS_CUSTOM_POLICY, "c");
      ws_xml_ns_add(policy, XML_NS_CUSTOM_AUTHENTICATION, "auth");
      WsXmlNodeH exactly_one = ws_xml_add_child(policy, XML_NS_CUSTOM_POLICY, "ExactlyOne", nullptr);
      WsXmlNodeH all = ws_xml_add_child(exactly_one, XML_NS_CUSTOM_POLICY, "All", nullptr);
      WsXmlNodeH authentication = ws_xml_add_child(all, XML_NS_CUSTOM_AUTHENTICATION, "Authentication", nullptr);
      ws_xml_add_node_attr(authentication, nullptr, "Profile", WSMAN_SECURITY_PROFILE_HTTPS_MUTUAL);
      WsXmlNodeH client_certificate = ws_xml_add_child(authentication, XML_NS_CUSTOM_AUTHENTICATION, "ClientCertificate", nullptr);
      WsXmlNodeH thumbprint = ws_xml_add_child_format(client_certificate, XML_NS_CUSTOM_AUTHENTICATION, "Thumbprint", "%s", processor_.ssl_ca_cert_thumbprint_.c_str());
      ws_xml_add_node_attr(thumbprint, nullptr, "Role", "issuer");
    }
    
    ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSM_MAX_ELEMENTS, "20");
    ws_xml_add_child(delivery_node, XML_NS_WS_MAN, WSENUM_MAX_TIME, "PT5.000S");

    // Expires
    ws_xml_add_child(subscribe_node, XML_NS_EVENTING, WSEVENT_EXPIRES, "PT600.000S");

    // Filter
    WsXmlNodeH filter_node = ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_FILTER, processor_.xpath_xml_query_.c_str());
    // ws_xml_add_node_attr(filter_node, nullptr, "Dialect", "http://schemas.microsoft.com/win/2004/08/events/eventquery");
    
    // WsXmlNodeH query_list = ws_xml_add_child(filter_node, nullptr, "QueryList", nullptr);
    // WsXmlNodeH query = ws_xml_add_child(query_list, nullptr, "Query", nullptr);
    // ws_xml_add_node_attr(query, nullptr, "Id", "0");
    // WsXmlNodeH select = ws_xml_add_child(query, nullptr, "Select", "*");
    // ws_xml_add_node_attr(select, nullptr, "Path", "Application");

    // Bookmark
    if (it != processor_.subscribers_.end() && it->second.bookmark != nullptr) {
      WsXmlNodeH bookmark_node = ws_xml_get_doc_root(it->second.bookmark);
      ws_xml_copy_node(bookmark_node, subscribe_node);
    } else if (processor_.initial_existing_events_strategy_ == INITIAL_EXISTING_EVENTS_STRATEGY_ALL) {
      ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_BOOKMARK, "http://schemas.dmtf.org/wbem/wsman/1/wsman/bookmark/earliest");
    }

    // Send Bookmarks
    ws_xml_add_child(subscribe_node, XML_NS_WS_MAN, WSM_SENDBOOKMARKS, nullptr);


    // Copy whole Subscription
    WsXmlNodeH subscription_node = ws_xml_get_doc_root(subscription_doc);
    ws_xml_copy_node(subscription_node, subscription);
    
    // Save subscription
    if (it != processor_.subscribers_.end()) {
      if (it->second.subscription != nullptr) {
        ws_xml_destroy_doc(it->second.subscription);
      }
    } else {
      it = processor_.subscribers_.emplace(machine_id, SubscriberData()).first;
    }
    it->second.subscription_version = std::move(subscription_version);
    it->second.subscription = subscription_doc;
  }

  // Send response
  char* xml_buf = nullptr;
  int xml_buf_size = 0;
  ws_xml_dump_memory_enc(response, &xml_buf, &xml_buf_size, "UTF-8");

  const struct mg_request_info* req_info = mg_get_request_info(conn);
  sendResponse(conn, machine_id, req_info->remote_addr, xml_buf, xml_buf_size);

  ws_xml_free_memory(xml_buf);

  return true;
}

SourceInitiatedSubscription::Handler::WriteCallback::WriteCallback(char* text)
    : text_(text) {
}

int64_t SourceInitiatedSubscription::Handler::WriteCallback::process(std::shared_ptr<io::BaseStream> stream) {
  return stream->write(reinterpret_cast<uint8_t*>(text_), strlen(text_));
}

int SourceInitiatedSubscription::Handler::enumerateEventCallback(WsXmlNodeH node, void* data) {
  if (data == nullptr) {
    return 1; // TODO
  }

  SourceInitiatedSubscription::Handler* self = nullptr;
  std::string machine_id;
  std::string remote_ip;
  std::tie(self, machine_id, remote_ip) = *static_cast<std::tuple<SourceInitiatedSubscription::Handler*, std::string, std::string>*>(data);

  char* text = ws_xml_get_node_text(node);
  if (text == nullptr) {
      return 1; // TODO
  }

  self->processor_.logger_->log_trace("Found Event");
  auto session = self->processor_.session_factory_->createSession();
  auto flow_file = std::static_pointer_cast<FlowFileRecord>(session->create());
  if (flow_file == nullptr) {
    return 1; // TODO
  }

  WriteCallback callback(text);
  session->write(flow_file, &callback);

  session->putAttribute(flow_file, FlowAttributeKey(MIME_TYPE), "application/xml");
  flow_file->addAttribute(ATTRIBUTE_WEF_REMOTE_MACHINEID, machine_id);
  flow_file->addAttribute(ATTRIBUTE_WEF_REMOTE_IP, remote_ip);

  session->transfer(flow_file, SourceInitiatedSubscription::Success);
  session->commit();

  return 0;
}

bool SourceInitiatedSubscription::Handler::handleSubscriptions(struct mg_connection* conn, const std::string& endpoint, WsXmlDocH request) {
  utils::ScopeGuard guard([&]() {
      ws_xml_destroy_doc(request);
  });
  auto action = getSoapAction(request);
  auto machine_id = getMachineId(request);
  const struct mg_request_info* req_info = mg_get_request_info(conn);
  std::string remote_ip = req_info->remote_addr;
  if (action == WSMAN_CUSTOM_ACTION_HEARTBEAT) {
    processor_.logger_->log_debug("Received Heartbeat on %s from %s (%s)", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
  } else if (action == WSMAN_CUSTOM_ACTION_EVENTS) {
    processor_.logger_->log_debug("Received Events on %s from %s (%s)", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
    WsXmlNodeH body = ws_xml_get_soap_body(request);
    if (body == nullptr) {
      processor_.logger_->log_error("Received malformed Events request on %s from %s (%s), SOAP Body missing", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
      return false;
    }
    WsXmlNodeH events_node = ws_xml_get_child(body, 0 /*index*/, XML_NS_WS_MAN, WSM_EVENTS);
    if (events_node == nullptr) {
      processor_.logger_->log_error("Received malformed Events request on %s from %s (%s), Events missing", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
      return false;
    }
    const struct mg_request_info* req_info = mg_get_request_info(conn);
    std::tuple<SourceInitiatedSubscription::Handler*, std::string, std::string> callback_args = std::forward_as_tuple(this, machine_id, remote_ip);
    int ret = ws_xml_enum_children(events_node, &SourceInitiatedSubscription::Handler::enumerateEventCallback, &callback_args, 0 /*bRecursive*/);
    if (ret != 0) {
      processor_.logger_->log_error("Failed to parse events on %s from %s (%s)", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str());
      // TODO
    }
    // Bookmark
    WsXmlNodeH header = ws_xml_get_soap_header(request);
    WsXmlNodeH bookmark_node = ws_xml_get_child(header, 0 /*index*/, XML_NS_WS_MAN, WSM_BOOKMARK);
    if (bookmark_node != nullptr) {
      WsXmlDocH bookmark_doc = ws_xml_create_doc(XML_NS_WS_MAN, WSM_BOOKMARK);
      WsXmlNodeH temp = ws_xml_get_doc_root(bookmark_doc);
      ws_xml_duplicate_children(temp, bookmark_node);

      std::lock_guard<std::mutex> lock(processor_.mutex_);
      auto it = processor_.subscribers_.find(machine_id);
      if (it != processor_.subscribers_.end()) {
          if (it->second.bookmark != nullptr) {
            ws_xml_destroy_doc(it->second.bookmark);
          }
      } else {
        it = processor_.subscribers_.emplace(machine_id, SubscriberData()).first;
      }
      it->second.bookmark = bookmark_doc;
      // Bookmark changed, invalidate subscription
      if (it->second.subscription != nullptr) {
        it->second.subscription_version.clear();
        ws_xml_destroy_doc(it->second.subscription);
        it->second.subscription = nullptr;
      }

      char* xml_buf = nullptr;
      int xml_buf_size = 0;
      ws_xml_dump_memory_enc(bookmark_doc, &xml_buf, &xml_buf_size, "UTF-8");
      processor_.logger_->log_debug("Saved new bookmark for %s: \"%.*s\"", machine_id.c_str(), xml_buf_size, xml_buf);
      ws_xml_free_memory(xml_buf);
    }
  } else {
    processor_.logger_->log_error("%s called by %s (%s) with unknown Action \"%s\"", endpoint.c_str(), machine_id.c_str(), remote_ip.c_str(), action.c_str());
    return false; // TODO
  }

  WsXmlDocH ack = wsman_create_response_envelope(request, WSMAN_CUSTOM_ACTION_ACK);
  WsXmlNodeH ack_header = ws_xml_get_soap_header(ack);

  utils::Identifier msg_id = processor_.id_generator_->generate();
  ws_xml_add_child_format(ack_header, XML_NS_ADDRESSING, WSA_MESSAGE_ID, "uuid:%s", msg_id.to_string().c_str());

  // Send ACK
  char* xml_buf = nullptr;
  int xml_buf_size = 0;
  ws_xml_dump_memory_enc(ack, &xml_buf, &xml_buf_size, "UTF-8");

  sendResponse(conn, machine_id, remote_ip, xml_buf, xml_buf_size);

  ws_xml_free_memory(xml_buf);

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

  FILE* fp = fopen(ssl_ca_file.c_str(), "rb");
  if (fp == nullptr) {
    logger_->log_error("Failed to open file specified by SSL Certificate Authority attribute");
    return;
  }
  X509* ca = nullptr;
  PEM_read_X509(fp, &ca, nullptr, nullptr);
  fclose(fp);
  if (ca == nullptr) {
    logger_->log_error("Failed to parse file specified by SSL Certificate Authority attribute");
    return;
  }
  std::array<uint8_t, 20U> hash_buf;
  int ret = X509_digest(ca, EVP_sha1(), hash_buf.data(), nullptr);
  X509_free(ca);
  if (ret != 1) {
    logger_->log_error("Failed to get fingerprint for CA specified by SSL Certificate Authority attribute");
    return;
  }
  ssl_ca_cert_thumbprint_ = utils::StringUtils::to_hex(hash_buf.data(), hash_buf.size(), true /*uppercase*/);
  logger_->log_debug("%s SHA-1 thumbprint is %s", ssl_ca_file.c_str(), ssl_ca_cert_thumbprint_.c_str());

  session_factory_ = sessionFactory;

  std::vector<std::string> options;
  options.emplace_back("enable_keep_alive");
  options.emplace_back("yes");
  options.emplace_back("keep_alive_timeout_ms");
  options.emplace_back("15000");
  options.emplace_back("num_threads");
  options.emplace_back("1");
  options.emplace_back("listening_ports");
  options.emplace_back(std::to_string(listen_port_) + "s");
  options.emplace_back("ssl_certificate");
  options.emplace_back(ssl_certificate_file);
  options.emplace_back("ssl_ca_file");
  options.emplace_back(ssl_ca_file);
  options.emplace_back("ssl_verify_peer");
  options.emplace_back(verify_peer ? "yes" : "no");

  server_ = std::unique_ptr<CivetServer>(new CivetServer(options));
  handler_ = std::unique_ptr<Handler>(new Handler(*this));
  server_->addHandler("**", *handler_);
}

void SourceInitiatedSubscription::notifyStop() {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
