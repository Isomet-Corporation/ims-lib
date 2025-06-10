#ifndef TFTPCLIENT
#define TFTPCLIENT

#include "tftp_packet.h"
#include <cstdint>

#define TFTP_CLIENT_SERVER_TIMEOUT 2000

#define TFTP_CLIENT_ERROR_TIMEOUT 0
#define TFTP_CLIENT_ERROR_SELECT 1
#define TFTP_CLIENT_ERROR_CONNECTION_CLOSED 2
#define TFTP_CLIENT_ERROR_RECEIVE 3
#define TFTP_CLIENT_ERROR_NO_ERROR 4
#define TFTP_CLIENT_ERROR_PACKET_UNEXPECTED 5

#include "boost/container/deque.hpp"

class TFTPClient {

	private:

		sockaddr_in m_server;
		int	m_server_port;

		//- kliento socketo descriptorius
		int socket_descriptor;

		TFTP_Packet received_packet;

	protected:

		int sendBuffer(char *);
		int sendPacket(TFTP_Packet* packet);

	public:

		TFTPClient(SOCKADDR *server, int port);
		~TFTPClient();

		bool getFile(const char* filename, boost::container::deque<std::uint8_t>&);
		bool sendFile(const boost::container::deque<std::uint8_t>&, const char* destination);

		int waitForPacket(TFTP_Packet* packet, int timeout_ms = TFTP_CLIENT_SERVER_TIMEOUT);
		bool waitForPacketACK(int packet_number, int timeout_ms = TFTP_CLIENT_SERVER_TIMEOUT);
		int waitForPacketData(int packet_number, int timeout_ms = TFTP_CLIENT_SERVER_TIMEOUT);

		void errorReceived(TFTP_Packet* packet);

};

class ETFTPSocketCreate: public std::exception {
  virtual const char* what() const throw() {
    return "Unable to create a socket";
  }
public:
  // override default destructor
  		virtual ~ETFTPSocketCreate() {}
};

class ETFTPSocketInitialize: public std::exception {
  virtual const char* what() const throw() {
    return "Unable to find socket library";
  }
public:
  // override default destructor
  		virtual ~ETFTPSocketInitialize() {}
};

void DEBUGMSG(char*);

#endif
