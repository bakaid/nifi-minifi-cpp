#include <vector>
#include <string>
#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

extern "C" {
#include "wsman-api.h"
#include "wsman-xml.h"
#include "wsman-xml-api.h"
#include "wsman-xml-serialize.h"
#include "wsman-xml-serializer.h"
#include "wsman-soap.h"
#include "wsman-soap-envelope.h"
}

#include <CivetServer.h>

int continue_working = 0;

#define XML_NS_CUSTOM_SUBSCRIPTION "http://schemas.microsoft.com/wbem/wsman/1/subscription"

class Responder : public CivetHandler {
public:
    bool handlePost(CivetServer *server, struct mg_connection *conn) {
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
//             std::cerr << "Body: " << std::string(reinterpret_cast<char*>(buf.data()), read_bytes);
//             fwrite(buf.data(), 1U, read_bytes, stdout);
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

        ws_xml_add_child(subscription, XML_NS_CUSTOM_SUBSCRIPTION, "Version", "uuid:BB8CD0E7-46F4-40E4-B74C-A0C7B509F690");

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
        }
        //
        WsXmlNodeH subscription_node = ws_xml_get_doc_root(subscription_item);
        ws_xml_copy_node(subscription_node, subscription);
        
        
        // Send response
        xml_buf = nullptr;
        xml_buf_size = 0;
        ws_xml_dump_memory_enc(response, &xml_buf, &xml_buf_size, "UTF-8");
        
//         std::cerr << "Response: " << std::string(xml_buf, xml_buf_size);
                ws_xml_dump_doc(stderr, response);
        
        mg_printf(conn, "HTTP/1.1 200 OK\r\n");
        mg_printf(conn, "Content-Type: application/soap+xml;charset=UTF-8\r\n");
        mg_printf(conn, "Authorization: http://schemas.dmtf.org/wbem/wsman/1/wsman/secprofile/https/mutual\r\n");
        mg_printf(conn, "Content-Length: %d\r\n", xml_buf_size);
        mg_printf(conn, "\r\n");
        mg_printf(conn, "%.*s", xml_buf_size, xml_buf);

        return true;
    }
};

int main(int argc, char* argv[]) {
//     // Create subscription
//     WsXmlDocH subscription = ws_xml_create_envelope();
//     WsXmlNodeH header = ws_xml_get_soap_header(subscription);
//     WsXmlNodeH node;
// 
//     node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_ACTION, EVT_ACTION_SUBSCRIBE);
//     ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");
// 
//     node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_TO, WSA_TO_ANONYMOUS);
//     ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");
// 
//     node = ws_xml_add_child(header, XML_NS_WS_MAN, WSM_RESOURCE_URI, "http://schemas.microsoft.com/wbem/wsman/1/windows/EventLog");
//     ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");
// 
//     node = ws_xml_add_child(header, XML_NS_ADDRESSING, WSA_REPLY_TO, nullptr);
//     node = ws_xml_add_child(node, XML_NS_ADDRESSING, WSA_ADDRESS, WSA_TO_ANONYMOUS);
//     ws_xml_add_node_attr(node, XML_NS_SOAP_1_2, SOAP_MUST_UNDERSTAND, "true");
//     
//     ws_xml_dump_doc(stderr, subscription);

  std::vector<std::string> options;
  options.emplace_back("enable_keep_alive");
  options.emplace_back("yes");
  options.emplace_back("keep_alive_timeout_ms");
  options.emplace_back("15000");
  options.emplace_back("num_threads");
  options.emplace_back("1");
  options.emplace_back("listening_ports");
  options.emplace_back("5986s");
  options.emplace_back("ssl_certificate");
  options.emplace_back("/home/bakaid/certs/server.pem");
  options.emplace_back("ssl_ca_file");
  options.emplace_back("/home/bakaid/certs/ca.crt");
  options.emplace_back("ssl_verify_peer");
  options.emplace_back("no");

  CivetServer server(options);
  Responder responder;
  server.addHandler("**", responder);
  std::this_thread::sleep_for(std::chrono::seconds(3600));

  return 0;
}
