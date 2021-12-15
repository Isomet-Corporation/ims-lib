#ifdef WIN32
// Winsock interface
#include "winsock2.h"
#include "ws2tcpip.h"
#pragma comment(lib, "Ws2_32.lib")
#else
// Native BSD Sockets Interface
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include "sys/select.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
//#include <fstream>
#include <exception>
#include <vector>

#include "tftp_packet.h"
#include "tftp_client.h"

using namespace std;

TFTPClient::TFTPClient(SOCKADDR *server, int port) : m_server(*(sockaddr_in*)server), m_server_port(port) {
	socket_descriptor = INVALID_SOCKET;

	char str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(m_server.sin_addr), str, INET_ADDRSTRLEN);
	//cout << "Connecting to " << str << " on port " << m_server_port << endl;
    socket_descriptor = socket(PF_INET, SOCK_DGRAM, 0);

	if (socket_descriptor == INVALID_SOCKET) {
        throw new ETFTPSocketCreate;
    }

    m_server.sin_port = htons(m_server_port);
}

int TFTPClient::sendBuffer(char *buffer) {
	socklen_t m = sizeof(m_server);

    return sendto(socket_descriptor, buffer, (int)strlen(buffer), 0, (struct sockaddr *)&m_server, m);

}

int TFTPClient::sendPacket(TFTP_Packet* packet) {
	socklen_t m = sizeof(m_server);

	return sendto(socket_descriptor, (char*)packet->getData(), packet->getSize(), 0, (struct sockaddr *)&m_server, m);

}

bool TFTPClient::getFile(const char* filename, boost::container::deque<std::uint8_t>& dest) {

	TFTP_Packet packet_rrq, packet_ack;
	char buffer[TFTP_PACKET_DATA_SIZE];

	packet_rrq.createRRQ(filename);

	sendPacket(&packet_rrq);

	int last_packet_no = 1;
	int wait_status;
	int timeout_count = 0;
	boost::container::deque<std::uint8_t>::iterator arrptr = dest.begin();
	dest.clear();

	while (true) {

		wait_status = waitForPacketData(last_packet_no, TFTP_CLIENT_SERVER_TIMEOUT);
		if (wait_status == TFTP_CLIENT_ERROR_PACKET_UNEXPECTED) {
			return false;
		}
		else if (wait_status == TFTP_CLIENT_ERROR_TIMEOUT) {
			timeout_count++;
			if (timeout_count < 2) { //-  Since this is the first timeout, we resend the last ACK
				sendPacket(&packet_ack);
				continue;
			}
			else {
				//cout << "Connection timeout" << endl;
				return false;
			}
		}

		if (last_packet_no != received_packet.getNumber()) {
			/* TFTP recognizes only one error condition that does not cause
			   termination, the source port of a received packet being incorrect.
			   In this case, an error packet is sent to the originating host. */

			/* Taip negali nutikti, nes pas mus naudojamas ACK Lock`as */

			//cout << "This should not happen!" << endl; //- aisku kada kadanors tai vis tiek atsitiks :)
		}
		else {
			//received_packet.dumpData();
			last_packet_no++;

			//-	If it's a timeout package, then let's do it and let it repeat it
			if (timeout_count == 1) {
				timeout_count = 0;
			}

			int len;
			if ((len = received_packet.copyData(4, buffer, TFTP_PACKET_DATA_SIZE)) > -1) {
				if (len) dest.insert(dest.end(), buffer, &buffer[len]);
				//- A data packet of less than 512 bytes signals termination of a transfer.

				if (received_packet.getSize() - 4 < TFTP_PACKET_DATA_SIZE) {

					/* The host acknowledging the final DATA packet may terminate its side
					   of the connection on sending the final ACK. */
					packet_ack.createACK((last_packet_no - 1));
					if (sendPacket(&packet_ack)) {
						break;
					}
				}
				else {
					//- Each data packet contains one block of data, and must be acknowledged by 
					//- an acknowledgment packet before the next packet can be sent.

					packet_ack.createACK((last_packet_no - 1)); //- siunciam toki paketo numeri, kuri gavom paskutini
					sendPacket(&packet_ack);
				}

			}
			else return false;
		}
	}

	return true;

}

int TFTPClient::waitForPacket(TFTP_Packet* packet, int timeout_ms) {

	packet->clear();

	fd_set fd_reader;		  // soketu masyvo struktura
	timeval connection_timer; // laiko struktura perduodama select()

	connection_timer.tv_sec = timeout_ms / 1000; // s
	connection_timer.tv_usec = 0; // neveikia o.0 timeout_ms; // ms 

	FD_ZERO(&fd_reader);
	// laukiam, kol bus ka nuskaityti
	FD_SET(socket_descriptor, &fd_reader);

	int select_ready = select(socket_descriptor + 1, &fd_reader, NULL, NULL, &connection_timer);

	if (select_ready == -1) {

#ifdef WIN32
		//cout << "Error in select(), no: " << WSAGetLastError() << endl;
#else
		//cout << "Error in select(): " << endl;
#endif
		
    return TFTP_CLIENT_ERROR_SELECT;

	} else if (select_ready == 0) {

		//DEBUGMSG("Timeout");
		return TFTP_CLIENT_ERROR_TIMEOUT;

	}

	//- turim sekminga event`a

	int receive_status;

	SOCKADDR from;
	socklen_t l = sizeof(from);
	receive_status = recvfrom(socket_descriptor, (char*)packet->getData(), TFTP_PACKET_MAX_SIZE, 0, &from, &l);

	if (receive_status == 0) {
		//cout << "Connection was closed by server\n";
		return TFTP_CLIENT_ERROR_CONNECTION_CLOSED;
    }

	if (receive_status == SOCKET_ERROR)	{
		//DEBUGMSG("recv() error in waitForPackage()");
		return TFTP_CLIENT_ERROR_RECEIVE;
	}

	//- receive_status - gautu duomenu dydis
	m_server.sin_port = ((struct sockaddr_in*)&from)->sin_port;
	packet->setSize(receive_status);

	return TFTP_CLIENT_ERROR_NO_ERROR;

}

bool TFTPClient::waitForPacketACK(int packet_number, int timeout_ms) {

	TFTP_Packet received_packet;
	int wait_result = waitForPacket(&received_packet, timeout_ms);
	if (TFTP_CLIENT_ERROR_NO_ERROR == wait_result) {
		if (received_packet.isError()) {
	      //cout << "ACK expected, but got Error" << endl;
				errorReceived(&received_packet);
								return false;
		}

		if (received_packet.isACK()) {
	      //cout << "ACK for packet " << received_packet.getNumber() << "(expected: " << packet_number << ")" << endl;
			return true;
		}

		if (received_packet.isData()) {
	      //cout << "DATAK for packet " << received_packet.getNumber() << "(expected: " << packet_number << ")" << endl;
	      return false;
		}

	    //cout << "Unhandled packet" << endl;
	} else {
//    DEBUGMSG("We have an error in waitForPacket()");
  }
	return false;

}

int TFTPClient::waitForPacketData(int packet_number, int timeout_ms) {

	int wait_status = waitForPacket(&received_packet, timeout_ms);

	if (wait_status == TFTP_CLIENT_ERROR_NO_ERROR) {

		if (received_packet.isError()) {

			errorReceived(&received_packet);

			return TFTP_CLIENT_ERROR_PACKET_UNEXPECTED;

		}

		if (received_packet.isData()) {
			
			return TFTP_CLIENT_ERROR_NO_ERROR;

		}

	}

	return wait_status;

}

bool TFTPClient::sendFile(const boost::container::deque<std::uint8_t>& src, const char* destination) {
	
	TFTP_Packet packet_wrq, packet_data;
	//ifstream file(filename, ifstream::binary);
	char memblock[TFTP_PACKET_DATA_SIZE];

	packet_wrq.createWRQ(destination);
	//packet_wrq.dumpData();

	sendPacket(&packet_wrq);

	int last_packet_no = 0;
	int send_completion = 0;

	boost::container::deque<std::uint8_t>::const_iterator data_it = src.cbegin();
	while (true) {
		if (waitForPacketACK(last_packet_no++, TFTP_CLIENT_SERVER_TIMEOUT)) {
//			file.read(memblock, TFTP_PACKET_DATA_SIZE);
			if (!send_completion) {
				std::copy(data_it, (data_it + TFTP_PACKET_DATA_SIZE), memblock);
				data_it += TFTP_PACKET_DATA_SIZE;

				packet_data.createData(last_packet_no, (char*)memblock, TFTP_PACKET_DATA_SIZE);
			}
			else {
				packet_data.createData(last_packet_no, (char*)memblock, 0);
			}
			sendPacket(&packet_data);

			if (send_completion) {
				if (waitForPacketACK(last_packet_no, TFTP_CLIENT_SERVER_TIMEOUT)) {
					break;
				}
				else {
					return false;
				}
			}
			if (data_it >= src.cend()) {
				send_completion = 1;
				continue;
//				break;
			}

		} else {
			//cout << "Server has timed out" << endl;
			//file.close();
			return false;
		}
	}
	return true;
}

void TFTPClient::errorReceived(TFTP_Packet* packet) {

//	int error_code = packet->getWord(2);

	/*cout << "Error! Error code: " << error_code << endl;
	cout << "Error message: ";
	
	switch (error_code) {

		case 1: cout << TFTP_ERROR_1; break;
		case 2: cout << TFTP_ERROR_2; break;
		case 3: cout << TFTP_ERROR_3; break;
		case 4: cout << TFTP_ERROR_4; break;
		case 5: cout << TFTP_ERROR_5; break;
		case 6: cout << TFTP_ERROR_6; break;
		case 7: cout << TFTP_ERROR_7; break;
		case 0: 
		default: cout << TFTP_ERROR_0; break;

	}

	cout << endl;*/

//	this->~TFTPClient();

}

TFTPClient::~TFTPClient() {

    if (socket_descriptor != INVALID_SOCKET) {

        #ifdef WIN32

            closesocket(socket_descriptor);
           // WSACleanup();
            
        #else

            close(socket_descriptor);

        #endif

    }

}

void DEBUGMSG(char *msg) {
    #ifdef DEBUG
	std::cout << msg << "\n";
    #endif
}
