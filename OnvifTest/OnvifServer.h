#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace http = boost::beast::http; // from <boost/beast/http.hpp>
namespace pt = boost::property_tree; // from <boost/property_tree/ptree.hpp>

//Write an xml response using the Onvif headers
std::string AddXmlHeader(const std::string& action, const std::string& body) {
    pt::ptree xml;
    xml.put("<xmlattr>.xmlns:soap", "http://www.w3.org/2003/05/soap-envelope");
    xml.put("<xmlattr>.xmlns:wsa", "http://www.w3.org/2005/08/addressing");
    xml.put("<xmlattr>.xmlns:tds", "http://www.onvif.org/ver10/device/wsdl");
    xml.put("soap:Header.wsa:Action", action);
    xml.put("soap:Header.wsa:MessageID", "urn:uuid:" + boost::uuids::to_string(boost::uuids::random_generator()()));
    xml.put("soap:Header.wsa:To", "http://www.w3.org/2005/08/addressing/anonymous");
    xml.put("soap:Header.wsa:ReplyTo.wsa:Address", "http://www.w3.org/2005/08/addressing/anonymous");
    xml.put("soap:Body", body);

    //convert to string stream
    std::ostringstream oss;
    pt::write_xml(oss, xml);

    return oss.str();
}

class OnvifServer {
public:
    OnvifServer(boost::asio::io_context& ioc, unsigned short port) : m_acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
        Accept(); //start accepting connections
    }

//Private functions:
private:
    //Our function to accept connections
    void Accept() {
        //wait till someone connects
        m_acceptor.async_accept(
            [this](boost::beast::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->Start();
                }

                Accept(); //Start a new acceptor so we can accept more than a single connection
            });
    }

//private variables:
private:
    boost::asio::ip::tcp::acceptor m_acceptor;

//private classes:
private:
    //this is a bit hacky since it's thrown in here, I might move the session class somewhere else.
    //that being said it is of course unique to this server, which is why it is here in the first place.
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(boost::asio::ip::tcp::socket socket)
            : m_socket(std::move(socket)) {
        }

        void Start() {
            http::async_read(m_socket, m_buffer, m_request,
                [self = shared_from_this()](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    if (!ec) {
                        self->HandleRequest();
                    }
                });
        }

    private:
        void HandleRequest() {
            http::response<http::string_body> response;

            response.version(m_request.version());
            response.result(http::status::ok);
            response.set(http::field::server, "OnvifServer");
            response.set(http::field::content_type, "application/soap+xml");

            //parse the xml tree
            pt::ptree xml;
            std::istringstream iss(m_request.body());
            pt::read_xml(iss, xml);

            //get the action
            std::string action = xml.get<std::string>("soap:Header.wsa:Action");
            std::string body = xml.get<std::string>("soap:Body");

            if (action == "http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation") {
                std::string device_info = "<tds:GetDeviceInformationResponse>"
                    "<tds:Manufacturer>OnvifServer</tds:Manufacturer>"
                    "<tds:Model>OnvifServer</tds:Model>"
                    "<tds:FirmwareVersion>1.0</tds:FirmwareVersion>"
                    "<tds:SerialNumber>1234567890</tds:SerialNumber>"
                    "<tds:HardwareId>abcdef</tds:HardwareId>"
                    "</tds:GetDeviceInformationResponse>";
                response.body() = AddXmlHeader(action + "Response", device_info);
            }
            else {
                std::string fault = "<soap:Fault>"
                    "<soap:Code>"
                    "<soap:Value>soap:Sender</soap:Value>"
                    "</soap:Code>"
                    "<soap:Reason>"
                    "<soap:Text xml:lang=\"en\">Invalid action</soap:Text>"
                    "</soap:Reason>"
                    "</soap:Fault>";
                response.body() = AddXmlHeader(action + "Fault", fault);
            }

            response.set(http::field::content_length, std::to_string(response.body().size()));

            //send the socket it's reply
            http::async_write(m_socket, response,
                [self = shared_from_this()](boost::beast::error_code ec, std::size_t bytes_transferred) {
                    //this may require editing since it closes the socket after a reply has been sent
                    if (!ec) {
                        self->m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send);
                    }
                });
        }

        //Our private variables:
    private:
        boost::asio::ip::tcp::socket m_socket;
        boost::beast::flat_buffer m_buffer;
        http::request<http::string_body> m_request;
    };
};