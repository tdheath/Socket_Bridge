/**
 * Class to handle bridging of UDP and TCP socket connections. Each instance
 * has two threads handling a TCP client object and UDP server object, respectively. 
 * Both are nested classes of SocketBridge. On one receiving a packet, it then 
 * retransmits that packet via the other (see diagram below). No manipulation of 
 * the data is performed.
 *           ____________       ____________
 *           |          |       |          |
 *  -------->| UDP RECV |------>| TCP SEND |------>
 *           |          |       |          |
 *  <--------| UDP SEND |<------| TCP RECV |<------
 *           |__________|       |__________|
 */

#ifndef SOCKETBRIDGE_H
#define SOCKETBRIDGE_H

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

//Size of the buffer to handle packets
const int packetSize = 56;

//The SocketBridge class manages both socket classes and instantiates
//them. It is the front-facing aspect of the bridge.
class SocketBridge
{

public:
  
    //Constructor, takes in the port and ip info for each socket.
    SocketBridge(const std::string& tcpIp, 
                 const uint& tcpPort, 
                 const std::string& udpIp, 
                 const uint& udpPort);

   
    //Method to start the services. This will create the sockets, then
    //run them with local io_services. 
    void startConnections();

private:

    std::string tcpIp;
    std::string tcpPort;
    std::string udpIp;
    uint udpPort;

    //Forward declaration of UDP_Server, so it is usable by the TCP_Client
    class UDP_Server;

    //Nested TCP client class, manages the TCP socket
    class TCP_Client
    {
      friend class UDP_Server;

    public:
    
      //Constructor, initiates the TCP connection with the given io service and
      // using the given endpoint
      TCP_Client(boost::asio::io_service& io_service, 
                 const tcp::resolver::iterator& endpoint);

      //Method to pass in a pointer to a UDP_server object for the client to use.
      void setUdp(SocketBridge::UDP_Server* newUdp);

    private:

      //Method to start a socket read operation.
      void start_read();


      //Method to handle writing the given message to the socket.
      void handle_write(const boost::array<char, packetSize> message);

      tcp::socket socket_;
      tcp::endpoint remote_endpoint_;
      boost::array<char, ::packetSize> recv_buffer_;

	  //The UDP server object on which packets are sent
      UDP_Server* udpServer; 
    };

    //Nested UDP server class, manages the UDP socket
    class UDP_Server
    {
      friend class TCP_Client;

    public:
     
      //Constructor, initiates the UDP connection to the given host and port. 
	  //Handled by the given io service.
      UDP_Server(boost::asio::io_service& io_service, 
                 const std::string& host,
                 const uint& port);

      //Method to pass a pointer to a TCP_Client object for the server to use.
      void setTcp(SocketBridge::TCP_Client* newTcp);

    private:

      //Method to start a socket read operation.
      void start_read();

      //Method to handle sending the given message on the socket.
      void handle_send(const boost::array<char, ::packetSize> message);

      udp::socket socket_;
      udp::endpoint remote_endpoint_;
      boost::array<char, ::packetSize> recv_buffer_;

      //The TCP server object on which packets are sent
      TCP_Client* tcpServer;
    };

};

#endif //SOCKETBRIDGE_H
