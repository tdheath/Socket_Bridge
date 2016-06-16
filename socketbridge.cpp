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

#include "socketbridge.h"

//Constructor.
SocketBridge::SocketBridge(const std::string& tcpIp, 
                           const uint& tcpPort, 
                           const std::string& udpIp, 
                           const uint& udpPort)
    : tcpIp(tcpIp),
      tcpPort(std::to_string(tcpPort)),
      udpIp(udpIp),
      udpPort(udpPort)  
{

}

//method to start the socket connectios
void SocketBridge::startConnections()
{

  try
  {
    std::cout<<"Starting SocketBridge, connecting TCP "
             <<tcpIp<<":"<<tcpPort
             <<" to UDP "<<udpIp<<":"<<udpPort
             <<std::endl;

    //Use boost resolver to configure the tcp port
    boost::asio::io_service io_service;
    tcp::resolver resolver(io_service);
    auto endpoint_iterator = resolver.resolve({ this->tcpIp, this->tcpPort });

    UDP_Server udpSock(io_service, this->udpIp, this->udpPort);
    TCP_Client tcpSock(io_service, endpoint_iterator);

    //pass pointers so the services can interact with eachother
    tcpSock.setUdp(&udpSock);
    udpSock.setTcp(&tcpSock);

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }
}

/*---------------
TCP CLIENT 
----------------*/
//Constructor, which also starts operations
SocketBridge::TCP_Client::TCP_Client(boost::asio::io_service& io_service,
                                    const tcp::resolver::iterator& endpoint)
	: socket_(io_service)
{

	boost::asio::async_connect(socket_, endpoint,
        [this](boost::system::error_code ec, tcp::resolver::iterator)
    {
        if(!ec)
        {
            std::cout<<"SocketBridge: TCP client connected"<<std::endl;
            this->start_read();
        }
        else
        {
            std::cerr<<"SocketBridge: tcp failed to connect: "<<ec<<std::endl;
        }
    });
}

//Method to accept a UDP socket pointer, which will be used to send messages
void SocketBridge::TCP_Client::setUdp(SocketBridge::UDP_Server* newUdp)
{
	this->udpServer = newUdp;
}

//Method to read messages and forward them to the UDP socket
void SocketBridge::TCP_Client::start_read()
{
	boost::asio::async_read(socket_, boost::asio::buffer(recv_buffer_, packetSize), 
		[this](boost::system::error_code ec, std::size_t /*length*/)
    {
        if(!ec)
        {
            this->udpServer->handle_send(recv_buffer_);
            this->start_read();
        }
        else
        {
            std::cerr<<"SocketBridge: TCP failed read: "<<ec<<std::endl;
            this->socket_.close();
        }
    });
}

//Method to write messages received from the UDP socket
void SocketBridge::TCP_Client::handle_write(const boost::array<char, ::packetSize> message)
{
	boost::asio::async_write(socket_, boost::asio::buffer(message, ::packetSize),
		[this](boost::system::error_code ec, std::size_t /*length*/)
    {
        if(ec)
        {
            std::cerr<<"SocketBridge: TCP writing failed: "<<ec<<std::endl;
            this->socket_.close();
        }
    });
}

/*---------------
UDP SERVER
----------------*/
//Constructor, which also binds and starts the read operations
SocketBridge::UDP_Server::UDP_Server(boost::asio::io_service& io_service, 
                                    const std::string& host, 
                                    const uint& port)
: socket_(io_service, udp::v4()), 
    remote_endpoint_(udp::endpoint(boost::asio::ip::address::from_string(host), port))
{
	std::cout<<"SocketBridge: UDP Server connected"<<std::endl;

	boost::asio::socket_base::reuse_address option(true);
	boost::system::error_code ec;
	socket_.set_option(option, ec);

	if(ec)
	{
		std::cerr<<ec.message()<<std::endl;
	}

	socket_.bind(remote_endpoint_);
	start_read();
}

//Method to accept a TCP socket pointer, which will be used to send incoming messages
void SocketBridge::UDP_Server::setTcp(SocketBridge::TCP_Client* newTcp)
{
	this->tcpServer = newTcp;
}

//Method to start read opertions and forward messages to the TCP socket
void SocketBridge::UDP_Server::start_read()
{
	socket_.async_receive_from(
		boost::asio::buffer(recv_buffer_,::packetSize), remote_endpoint_,
		[this](boost::system::error_code ec, std::size_t /*length*/)
    {
        if (!ec)
        {
            //have TCP server write data here
            this->tcpServer->handle_write(recv_buffer_);
            start_read();
        }
        else
        {
            std::cerr<<"SocketBridge: UDP reading failed: "<<ec<<std::endl;
        }
    });
}

//Method to send messages received from the TCP socket
void SocketBridge::UDP_Server::handle_send(const boost::array<char, ::packetSize> message)
{

	socket_.async_send_to(
		boost::asio::buffer(message, ::packetSize), remote_endpoint_, 
		[this](boost::system::error_code ec, std::size_t /*length*/)
    {
        if (ec)
        {
            std::cerr<<"SocketBridge: UDP sending failed: "<<ec<<std::endl;
        }
    });
}
