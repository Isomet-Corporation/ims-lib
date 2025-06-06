/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Implementation for ENET Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_ENET.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-17 18:00:31 +0000 (Fri, 17 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 678 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#if defined(_WIN32) || defined(__QNXNTO__) || defined(__linux__)

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <iomanip>
#include <atomic>

#ifdef WIN32
// Winsock interface
#include "winsock2.h"
#include "ws2tcpip.h"
#pragma comment(lib, "Ws2_32.lib")
#else
#define __EXT_UNIX_MISC /* For u_int type */
// Native BSD Sockets Interface
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
#endif

#include "CM_ENET.h"
#include "IMSSystem.h"
#include "tftp_client.h"
#include "PrivateUtil.h"

#if defined _WIN32 && defined _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace iMS {

#if !defined (WIN32)

#define SOCKET int
#define SOCKADDR struct sockaddr
#define SOCKADDR_IN struct sockaddr_in
#define IN_ADDR struct in_addr
#define TIMEVAL struct timeval
#define SD_BOTH 2
//#define INVALID_SOCKET -1

typedef struct _INTERFACE_INFO  {
	  unsigned int    iiFlags;
	  struct sockaddr iiAddress;
	  struct sockaddr iiBroadcastAddress;
	  struct sockaddr iiNetmask;
	} INTERFACE_INFO;

#endif

static std::string logErrorString(int err = INT_MAX) {
	static char msgbuf[256];

	memset(msgbuf, '\0', 256 * sizeof(char));

#ifdef WIN32
	DWORD werr = err;
	if (err == INT_MAX) {
		werr = WSAGetLastError();
	}

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
		NULL,                // lpsource
		werr,                 // message id
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),    // languageid
		msgbuf,              // output buffer
		sizeof(msgbuf),     // size of msgbuf, bytes
		NULL);               // va_list of arguments
	if (!*msgbuf) {
		sprintf(msgbuf, "%d", werr);  // provide error # if no string available
	}
#else
	if (err == INT_MAX) {
		err = errno;	
	}
	strncpy(msgbuf, strerror(err), sizeof(msgbuf) );
#endif
		return std::string(msgbuf);
	}


	class CM_ENET::MsgContext
	{
	public:
		static const int MaxPacketSize = 4 * (((IOReport::PAYLOAD_MAX_LENGTH + IOReport::OVERHEAD_MAX_LENGTH) / 4) + 1);
		static const int MaxBufferSize = 1024;

		SOCKET socket;
		std::vector<uint8_t> *Buffer;
#ifdef WIN32
		WSAOVERLAPPED OvLap;
		WSABUF DataBuf;
		DWORD BytesXfer;
		LONG bufLen;
#else
		unsigned int BytesXfer;
		unsigned long bufLen;
#endif
		MessageHandle handle;

		MsgContext();
		~MsgContext();
	};

	CM_ENET::MsgContext::MsgContext()
	{
#ifdef WIN32
		SecureZeroMemory((PVOID)&this->OvLap, sizeof(WSAOVERLAPPED));
		// Create an event handle and setup the overlapped structure.
		this->OvLap.hEvent = WSACreateEvent();
		if (this->OvLap.hEvent == NULL) {
			BOOST_LOG_SEV(lg::get(), sev::error) << "WSACreateEvent failed with error:: " << WSAGetLastError() << std::endl;
		}
#endif
		this->Buffer = new std::vector<uint8_t>(MsgContext::MaxPacketSize, 0);
		this->bufLen = MsgContext::MaxPacketSize;
		this->BytesXfer = 0;
	}

	CM_ENET::MsgContext::~MsgContext()
	{
#ifdef WIN32
		WSACloseEvent(this->OvLap.hEvent);
#endif
		delete this->Buffer;
	}

	class CM_ENET::FastTransfer
	{
	public:
		FastTransfer(boost::container::deque<std::uint8_t>& data, const std::array<std::uint8_t, 16>& uuid, int len) :
			m_data(data), m_uuid(uuid), m_len(len) {
			m_data_it = m_data.cbegin();
		}

		static const int TRANSFER_GRANULARITY = 512;
		static const int TRANSFER_QUEUE_SZ = 16;

		boost::container::deque<std::uint8_t>& m_data;
		std::array<std::uint8_t, 16> m_uuid;
		int m_len;

		boost::container::deque<std::uint8_t>::const_iterator m_data_it;
	};


	// All private data and member functions contained within Impl class
	class CM_ENET::Impl
	{
	public:
		Impl(IConnectionManager* parent);
		~Impl();

		const std::string Ident = "CM_ETH";
		ListBase<std::string> PortMask;

		struct InterfaceConnectionDetail
		{
			std::string serialNo;
			unsigned char MACAddress[6];
			std::string RemoteIPAddr;
			std::string HostIPAddr;
		};

		std::list<InterfaceConnectionDetail>* intf_detail;

		// Declare some constants here; see definitions below
		static const int ANNOUNCE_DEST_PORT;
		static const int ANNOUNCE_SRC_PORT;
		static const int IMSMSG_PORT;
		static const int IMSINTR_PORT;
		static const int ANNOUNCE_WAIT;
		static const int MTU_SIZE;

		int discovery_timeout;

		std::vector<IMSSystem> ListConnectedDevices();

		//std::shared_ptr<CM_ENET::MsgContext> SocketArray[WSA_MAXIMUM_WAIT_EVENTS];
#ifdef WIN32
		WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];
		WSADATA wsaData;
#endif
		SOCKET msgSock, intrSock;

		std::atomic<_FastTransferStatus> FastTransferStatus{ _FastTransferStatus::IDLE };
		FastTransfer *fti = nullptr;

		SOCKADDR ConnectedServer;

		//std::deque<std::shared_ptr<CM_ENET::MsgContext>> m_rxBuf;
		//int m_rxbuf_size;
		//mutable std::mutex m_rxBufmutex;
		//std::condition_variable m_rxBufcond;

		// Memory Transfer Thread
		std::thread memoryTransferThread;
		mutable std::mutex m_tfrmutex;
		std::condition_variable m_tfrcond;

		// Interrupt receiving thread
		std::thread interruptThread;
		std::shared_ptr<std::vector<uint8_t>> interruptData;

		std::string conn_string;
	private:
		IConnectionManager * m_parent;
	};

	const int CM_ENET::Impl::ANNOUNCE_DEST_PORT = 28242;
	const int CM_ENET::Impl::ANNOUNCE_SRC_PORT = 28243;
	const int CM_ENET::Impl::IMSMSG_PORT = 28244;
	const int CM_ENET::Impl::IMSINTR_PORT = 28245;

	// Allow 1sec for responses to arrive
	const int CM_ENET::Impl::ANNOUNCE_WAIT = 1000;

	// Max size of UDP response buffer
	const int CM_ENET::Impl::MTU_SIZE = 1560;

	CM_ENET::Impl::Impl(IConnectionManager* parent) : m_parent(parent)
	{
		intf_detail = new std::list<InterfaceConnectionDetail>();
#ifdef WIN32
		int iResult;

		// Initialize Winsock
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			BOOST_LOG_SEV(lg::get(), sev::error) << "WSAStartup failed: " << iResult << std::endl;
		}
#endif
		discovery_timeout = ANNOUNCE_WAIT;
	}

	CM_ENET::Impl::~Impl()
	{
		m_parent->Disconnect();

		delete intf_detail;
		intf_detail = nullptr;
#ifdef WIN32
		WSACleanup();
#endif
	}

	const std::string& CM_ENET::Ident() const
	{
		return mImpl->Ident;
	}

	std::vector<IMSSystem> CM_ENET::Impl::ListConnectedDevices()
	{
		SOCKET  AnnounceSocket;
		SOCKADDR_IN  AnnounceSrcAddr, AnnounceDestAddr;
		char mess[64], recvBuf[MTU_SIZE];
		SOCKADDR_IN recvAddr;
		socklen_t recvAddrLen;
		int numBytes, err;

		int nNumInterfaces = 0;

		INTERFACE_INFO InterfaceList[20];
#ifdef WIN32
		// Create a new socket to query interfaces with
		AnnounceSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (AnnounceSocket == INVALID_SOCKET) {
			BOOST_LOG_SEV(lg::get(), sev::error) << "announce: unable to create socket: " << WSAGetLastError() << std::endl;
			return std::vector<IMSSystem>();
		}

		unsigned long nBytesReturned;
		if (WSAIoctl(AnnounceSocket, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
				sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) {

			BOOST_LOG_SEV(lg::get(), sev::error) << "announce: get interface list error: " << WSAGetLastError() << std::endl;
			//std::cout << "Failed calling WSAIoctl: error " << WSAGetLastError() << std::endl;
			return std::vector<IMSSystem>();
		}

		closesocket(AnnounceSocket);
		nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);
#else

		struct ifaddrs *ifaddr, *ifa;
		int n;
		if (getifaddrs(&ifaddr) == -1) {
			BOOST_LOG_SEV(lg::get(), sev::error) << "getifaddrs error: " << strerror(errno) << std::endl;
			return std::vector<IMSSystem>();
		}

		/* Walk through linked list, maintaining head pointer so we can free list later */
		for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
			if (ifa->ifa_addr == NULL)
				continue;


			if (AF_INET == ifa->ifa_addr->sa_family)
			{
				memcpy(&InterfaceList[nNumInterfaces].iiAddress, ifa->ifa_addr, sizeof(struct sockaddr));
				memcpy(&InterfaceList[nNumInterfaces].iiBroadcastAddress, ifa->ifa_broadaddr, sizeof(struct sockaddr));
				memcpy(&InterfaceList[nNumInterfaces].iiNetmask, ifa->ifa_netmask, sizeof(struct sockaddr));
#if !defined(__QNXNTO__)
				InterfaceList[nNumInterfaces].iiFlags = ifa->ifa_flags;
#endif

				nNumInterfaces++;
			}
		}
		freeifaddrs(ifaddr);
#endif

		// The IPv4 family
		AnnounceSrcAddr.sin_family = AF_INET;
		// Port no.
		AnnounceSrcAddr.sin_port = htons(ANNOUNCE_SRC_PORT);
		//memset(AnnounceSrcAddr.sin_zero, '\0', sizeof(AnnounceSrcAddr.sin_zero));

		intf_detail->clear();

		for (int i = 0; i < nNumInterfaces; ++i) {
			if (!this->PortMask.empty())
			{
				// User supplied a mask list. Check if this interface is included
				bool match = false;
				for (std::list<std::string>::const_iterator it = this->PortMask.cbegin(); it != this->PortMask.cend(); ++it)
				{
					char InterfaceAddr[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, (void *)&((struct sockaddr_in *)&InterfaceList[i].iiAddress)->sin_addr, InterfaceAddr, INET_ADDRSTRLEN);
					if (!strcmp(InterfaceAddr, it->c_str())) {
						match = true;
						break;
					}
				}
				if (!match) continue;
			}

#if !defined(__QNXNTO__)
			// Don't use interface if it is down, loopback or not broadcast capable
			if ((!(InterfaceList[i].iiFlags & IFF_UP)) ||
					(InterfaceList[i].iiFlags & IFF_LOOPBACK) ||
					(!(InterfaceList[i].iiFlags & IFF_BROADCAST)))
				continue;
#endif
			// Create a new socket to send "who is out there" message
			AnnounceSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (INVALID_SOCKET == AnnounceSocket) {
				BOOST_LOG_SEV(lg::get(), sev::error) << "announce socket error: " << logErrorString() << std::endl;
				return std::vector<IMSSystem>();
			}
			// Set socket to non-blocking
#ifdef WIN32
			u_long iMode = 1;
			ioctlsocket(AnnounceSocket, FIONBIO, &iMode);
#else
			fcntl(AnnounceSocket, F_SETFL, O_NONBLOCK);
#endif

				int on = 1;
				// Clean out any old fragments of socket connections hanging around
			if (setsockopt(AnnounceSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)) == -1) {
				BOOST_LOG_SEV(lg::get(), sev::error) << "announce: setsockopt (SO_REUSEADDR) " << logErrorString() << std::endl;
					return std::vector<IMSSystem>();
			}
				// Enable Broadcast Transmission
			if (setsockopt(AnnounceSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof(on)) == -1) {
				BOOST_LOG_SEV(lg::get(), sev::error) << "announce: setsockopt (SO_REUSEADDR) " << logErrorString() << std::endl;
					return std::vector<IMSSystem>();
			}

				// Copy source IP address
				sockaddr_in *pAddress;
				pAddress = (sockaddr_in *)& (InterfaceList[i].iiAddress);
				memcpy(&AnnounceSrcAddr.sin_addr, (const void *)&pAddress->sin_addr, sizeof(IN_ADDR));

				// Bind Broadcast Socket to Port
				if (bind(AnnounceSocket, (SOCKADDR *)&AnnounceSrcAddr, sizeof(AnnounceSrcAddr)) == SOCKET_ERROR) {
					continue;
				}

				// Get local subnet broadcast address
				memset(&AnnounceDestAddr, '\0', sizeof(struct sockaddr_in));
				AnnounceDestAddr.sin_family = AF_INET;
				AnnounceDestAddr.sin_port = htons(ANNOUNCE_DEST_PORT);
				pAddress = (sockaddr_in *)& (InterfaceList[i].iiBroadcastAddress);
				memcpy(&AnnounceDestAddr.sin_addr, (const void *)&pAddress->sin_addr, sizeof(IN_ADDR));

				memset(mess, '\0', sizeof(mess));
				strncpy(mess, "Discovery: Who is out there?\n", sizeof(mess));

			{
				char BroadcastAddr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, (void *)&AnnounceDestAddr.sin_addr, BroadcastAddr, INET_ADDRSTRLEN);
				BOOST_LOG_SEV(lg::get(), sev::info) << "Sending Discovery packet: " << BroadcastAddr << " port " << ANNOUNCE_SRC_PORT << std::endl;
			}
				// Send the discovery packet
				/*int TotalByteSent = */sendto(AnnounceSocket, mess, (int)strlen(mess), 0,
						(SOCKADDR *)&AnnounceDestAddr, sizeof(AnnounceDestAddr));

				// Wait for responses to arrive
				std::this_thread::sleep_for(std::chrono::milliseconds(discovery_timeout));

				do {
					recvAddrLen = sizeof(recvAddr);
					memset(&recvBuf, '\0', sizeof(recvBuf));
					if ((numBytes = recvfrom(AnnounceSocket, recvBuf, sizeof(recvBuf), 0, (SOCKADDR *)&recvAddr, &recvAddrLen)) == -1) {
#ifdef WIN32
						if (err = WSAGetLastError() != WSAEWOULDBLOCK) {
#else
							if (err = errno != EWOULDBLOCK) {
#endif
						BOOST_LOG_SEV(lg::get(), sev::error) << "announce: recvfrom error" << logErrorString(err) << std::endl;

								return std::vector<IMSSystem>();
							}
						}
						if (numBytes > 0) {
							char RemoteAddr[INET_ADDRSTRLEN], *eol, *rxPtr;
							InterfaceConnectionDetail detail;
							// Save some info from the sender side
							//getpeername(AnnounceSocket, (SOCKADDR *)&recvAddr, &recvAddrLen);
							inet_ntop(AF_INET, (void *)&recvAddr.sin_addr, RemoteAddr, INET_ADDRSTRLEN);
							detail.RemoteIPAddr = std::string(RemoteAddr);
							//std::cout << "Sending IP Address : " << szHwAddr << " and port# : " << htons(recvAddr.sin_port) << std::endl;
							rxPtr = recvBuf;
							while ((eol = strchr(rxPtr, '\n')) != NULL)
							{
								if (!strncmp(rxPtr, "SNO: ", 5))
								{
									// Get Serial Number
									*(eol - 1) = '\0';
									detail.serialNo = std::string(&rxPtr[5]);
								}
								else if (!strncmp(rxPtr, "MAC: ", 5))
								{
									// Get MAC Address
									for (int i = 0; i < 6; i++)
									{
										detail.MACAddress[i] = (unsigned char)strtoul(rxPtr + 5 + 3 * i, NULL, 16);
									}
								}
								else if (!strncmp(rxPtr, "ReqIP: ", 7))
								{
									// Get Requestor IP
									*(eol - 1) = '\0';
									detail.HostIPAddr = std::string(&rxPtr[7]);
								}
								rxPtr = eol + 1;
							}
					BOOST_LOG_SEV(lg::get(), sev::info) << "Response received from " << detail.serialNo << " at " << RemoteAddr << std::endl;
							intf_detail->push_back(detail);
						}
					} while (numBytes > 0);

					// Close socket
#ifdef WIN32
					closesocket(AnnounceSocket);
#else
					close(AnnounceSocket);
#endif
				}

		if (intf_detail->empty()) {
				BOOST_LOG_SEV(lg::get(), sev::info) << "announce: No valid responses received" << std::endl;
			}

		std::vector<IMSSystem> IMSList;
		for (std::list<InterfaceConnectionDetail>::iterator it = intf_detail->begin(); it != intf_detail->end(); ++it)
		{
			// Then open device and send a Magic Number query message
			m_parent->Connect(it->serialNo);

			std::string serial = it->serialNo + ":" + it->RemoteIPAddr;

			if (m_parent->Open())
			{
				// Found a suitable connection interface, let's query its contents.
				IMSSystem thisiMS(m_parent, serial);

				thisiMS.Initialise();

				if (thisiMS.Ctlr().IsValid() || thisiMS.Synth().IsValid()) {
					int count = 0;
					for (std::vector<IMSSystem>::const_iterator it2 = IMSList.cbegin(); it2 != IMSList.cend(); ++it2)
					{
						if (it2->ConnPort() == serial) count++;
					}
					if (count > 0) {
						serial += std::string("-") += count;
					}
					IMSList.push_back(thisiMS);
				}
			}

			m_parent->Disconnect();
		}
		return IMSList;
	}

	// Default Constructor
	CM_ENET::CM_ENET() : mImpl(new CM_ENET::Impl(this))
	{
		sendTimeout = std::chrono::milliseconds(500);
		rxTimeout = std::chrono::milliseconds(10000);
		autoFreeTimeout = std::chrono::milliseconds(30000);
	}

	CM_ENET::~CM_ENET()
	{
		delete mImpl;
		mImpl = NULL;
	}

	std::vector<IMSSystem> CM_ENET::Discover(const ListBase<std::string>& PortMask)		
	{
//		std::cout << "CM_ENET::Discover()" << std::endl;
		mImpl->PortMask = PortMask;
		std::vector<IMSSystem> v = mImpl->ListConnectedDevices();
		return v;
	}

	void CM_ENET::Connect(const std::string& serial)
	{
		if (!DeviceIsOpen)
		{
			// If connecting without first scanning system, do a scan here to populate interface detail list
			if (mImpl->intf_detail->empty()) mImpl->ListConnectedDevices();

			// Connect TCP Client to Server
			SOCKADDR_IN ServerAddr, InterruptAddr;
			SOCKET InterruptSock;

#ifdef WIN32
			mImpl->msgSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
			InterruptSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
#else
			mImpl->msgSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			InterruptSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
			if ((mImpl->msgSock == INVALID_SOCKET) || (InterruptSock == INVALID_SOCKET))
			{
				BOOST_LOG_SEV(lg::get(), sev::error) << "client socket error: " << logErrorString() << std::endl;

				// Exit 
				return;
			}
			memset((void *)&ServerAddr, '\0', sizeof(SOCKADDR));
			// IPv4
			ServerAddr.sin_family = AF_INET;
			// Port no.
			ServerAddr.sin_port = htons(mImpl->IMSMSG_PORT);

			// Just in case we've been passed a port connection (serial:ipaddr) instead of just a serial number
			std::string serial_ = serial.substr(0,serial.find_first_of(":"));

			// The IP address
			for (std::list<Impl::InterfaceConnectionDetail>::iterator it = mImpl->intf_detail->begin(); it != mImpl->intf_detail->end(); ++it)
			{
				if (it->serialNo == serial_)
				{
					inet_pton(AF_INET, it->RemoteIPAddr.c_str(), (void*)&ServerAddr.sin_addr);
					//ServerAddr.sin_addr.s_addr = inet_addr(it->RemoteIPAddr.c_str());
				}
			}
			memcpy(&InterruptAddr, (void *)&ServerAddr, sizeof(SOCKADDR));
			InterruptAddr.sin_port = htons(mImpl->IMSINTR_PORT);

			// Unlike the messaging socket, the interrupt socket it only a listener, so we can quite happily bind to any interface
			InterruptAddr.sin_addr.s_addr = INADDR_ANY;

			int on = 1;
			// Clean out any old fragments of socket connections hanging around
			if (setsockopt(InterruptSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)) == -1) {
#ifdef WIN32
				int err = WSAGetLastError();
				BOOST_LOG_SEV(lg::get(), sev::error) << "setsockopt (SO_REUSEADDR): " << logErrorString(err) << std::endl;
				closesocket(mImpl->msgSock);
				closesocket(InterruptSock);
#else
				BOOST_LOG_SEV(lg::get(), sev::error) << "setsockopt (SO_REUSEADDR): " << strerror(errno) << std::endl;
				close(mImpl->msgSock);
				close(InterruptSock);
#endif
				return;
			}

			// Bind interrupt socket
			int bindResult = bind(InterruptSock, (SOCKADDR *)&InterruptAddr, sizeof(InterruptAddr));
			if (INVALID_SOCKET == bindResult) {
#ifdef WIN32
				int err = WSAGetLastError();
				BOOST_LOG_SEV(lg::get(), sev::error) << "interrupt socket failed to bind: " << logErrorString(err) << std::endl;
				closesocket(mImpl->msgSock);
				closesocket(InterruptSock);
#else
				BOOST_LOG_SEV(lg::get(), sev::error) << "bind error: " << strerror(errno) << std::endl;
				close(mImpl->msgSock);
				close(InterruptSock);
#endif
				return;
			}

			//set the sockets to non-blocking mode
#ifdef WIN32
			u_long iMode = 1;
			int iResult = ioctlsocket(mImpl->msgSock, FIONBIO, &iMode);
			iResult |= ioctlsocket(InterruptSock, FIONBIO, &iMode);
			if (iResult != NO_ERROR)
			{
				BOOST_LOG_SEV(lg::get(), sev::error) << "ioctlsocket (FIONBIO) failed with error: " << iResult << std::endl;
				return;
			}

			// Force the socket to send each message as it arrives in the buffer (disables Nagle algorithm: warning - this will impede network performance)
			{
				BOOL fNodelay = TRUE;
				setsockopt(mImpl->msgSock, IPPROTO_TCP, TCP_NODELAY, (const char FAR*)&fNodelay, sizeof(fNodelay));
			}

#else
			fcntl(mImpl->msgSock, F_SETFL, O_NONBLOCK);
			fcntl(InterruptSock, F_SETFL, O_NONBLOCK);
			// Force the socket to send each message as it arrives in the buffer (disables Nagle algorithm: warning - this will impede network performance)
			{
				int flag = 1;
				int result = setsockopt(mImpl->msgSock,            /* socket affected */
				                        IPPROTO_TCP,     /* set option at TCP level */
				                        TCP_NODELAY,     /* name of option */
				                        (void *) &flag,  /* the cast is historical cruft */
				                        sizeof(int));    /* length of option value */
				 if (result < 0) {
					BOOST_LOG_SEV(lg::get(), sev::error) << "setsockopt (TCP_NODELAY): " << strerror(errno) << std::endl;
					 return;
				 }
			}
#endif

			// And start listening
			int listenResult = listen(InterruptSock, SOMAXCONN);
			if (INVALID_SOCKET == listenResult) {
				BOOST_LOG_SEV(lg::get(), sev::error) << "interrupt socket failed to listen: " << listenResult << std::endl;
#ifdef WIN32
				closesocket(mImpl->msgSock);
				closesocket(InterruptSock);
#else
				close(mImpl->msgSock);
				close(InterruptSock);
#endif
				return;
			}

			TIMEVAL Timeout;
			Timeout.tv_sec = 4;
			Timeout.tv_usec = 0;


			// Make a connection to the server with socket SendingSocket.
			int RetCode = connect(mImpl->msgSock, (SOCKADDR *)&ServerAddr, sizeof(ServerAddr));
			if (RetCode < 0)
			{
#ifdef WIN32
				int err = WSAGetLastError();
				if (err != WSAEWOULDBLOCK) {
					BOOST_LOG_SEV(lg::get(), sev::error) << "Client: connect() failed! " << logErrorString(err) << std::endl;
					// Close the socket
					closesocket(mImpl->msgSock);
					closesocket(InterruptSock);
#else
				int err = errno;
				if ((err != EWOULDBLOCK) && (err != EINPROGRESS)) {
					BOOST_LOG_SEV(lg::get(), sev::error) << "Client: connect() failed! " << logErrorString(err) << std::endl;
					// Close the socket
					close(mImpl->msgSock);
					close(InterruptSock);
#endif
					// Exit 
					return;
				}
				else
				{
					fd_set Write, Err;
					FD_ZERO(&Write);
					FD_ZERO(&Err);
					FD_SET(mImpl->msgSock, &Write);
					FD_SET(mImpl->msgSock, &Err);

					// check if the socket is ready
					select(mImpl->msgSock+1, NULL, &Write, &Err, &Timeout);
					if (FD_ISSET(mImpl->msgSock, &Write))
					{
					}
					else {
						BOOST_LOG_SEV(lg::get(), sev::error) << "Client: connect() timed out! " << logErrorString() << std::endl;
#ifdef WIN32
						// Close the socket
						closesocket(mImpl->msgSock);
						closesocket(InterruptSock);
#else
						close(mImpl->msgSock);
						close(InterruptSock);
#endif
						// Exit 
						return;
					}
					//printf("Client: connect() is OK, got connected...\n");
					//printf("Client: Ready for sending and receiving data...\n");
				}
			}

			// Finally, we are expecting the device to try to connect to our interrupt socket, but not all
			// firmware versions support this, so allow a timeout to continue
				{
					fd_set Read;
					FD_ZERO(&Read);
					FD_SET(InterruptSock, &Read);
					select(InterruptSock+1, &Read, NULL, NULL, &Timeout);
					if (FD_ISSET(InterruptSock, &Read))
					{
						mImpl->intrSock = accept(InterruptSock, NULL, NULL);
						if (INVALID_SOCKET == mImpl->intrSock) {
							BOOST_LOG_SEV(lg::get(), sev::warning) << "Client: accept() failed! Continuing without interrupts " << logErrorString() << std::endl;
						} else {
#ifdef __linux__
							// v1.8.5 (Linux)
							// intrSock does not inherit the flags of InterruptSock, unlike in BSD and Windows
							// https://stackoverflow.com/questions/8053294/is-sockets-accept-return-descriptor-blocking-or-non-blocking/8053432
							fcntl(mImpl->intrSock, F_SETFL, O_NONBLOCK);
#endif
						}
					}
					else {
						mImpl->intrSock = INVALID_SOCKET;
							BOOST_LOG_SEV(lg::get(), sev::warning) << "Client: accept() timed out! Continuing without interrupts " << logErrorString() << std::endl;
					}
#ifdef WIN32
					// No longer need server socket
					closesocket(InterruptSock);
#else
					close(InterruptSock);
#endif

				}


			DeviceIsOpen = true;
			mImpl->conn_string = serial;

			// Save the IP address of the connected server
			mImpl->ConnectedServer = *((SOCKADDR*)&ServerAddr);

			// Clear Message Lists
			m_list.clear();
			while (!m_queue.empty()) m_queue.pop();
			//while (!mImpl->m_rxBuf.empty()) mImpl->m_rxBuf.pop_front();
			while (!m_rxCharQueue.empty()) m_rxCharQueue.pop();

			// Start Report Sending Thread
			senderThread = std::thread(&CM_ENET::MessageSender, this);

			// Start Response Receiver Thread
			receiverThread = std::thread(&CM_ENET::ResponseReceiver, this);

			// Start Response Parser Thread
			parserThread = std::thread(&CM_ENET::MessageListManager, this);

			// Start Memory Transferer Thread
			mImpl->memoryTransferThread = std::thread(&CM_ENET::MemoryTransfer, this);

			// Start Interrupt receiving thread
			mImpl->interruptThread = std::thread(&CM_ENET::InterruptReceiver, this);

			BOOST_LOG_SEV(lg::get(), sev::info) << "iMS System " << serial << " connected." << std::endl;
		}
	}
	void CM_ENET::Disconnect()
	{
		if (DeviceIsOpen)
		{
			BOOST_LOG_SEV(lg::get(), sev::info) << "Disconnecting from iMS System " << mImpl->conn_string << " ..." << std::endl;
	
			// Disable Interrupts
			BOOST_LOG_SEV(lg::get(), sev::info) << "Disabling interrupts." << std::endl;
			HostReport *iorpt;
			iorpt = new HostReport(HostReport::Actions::CTRLR_INTREN, HostReport::Dir::WRITE, 0);
			iorpt->Payload<int>(0);
			ReportFields f = iorpt->Fields();
			f.len = sizeof(int);
			iorpt->Fields(f);
			this->SendMsg(*iorpt);
			delete iorpt;

			BOOST_LOG_SEV(lg::get(), sev::info) << "Waiting for pending messages in the send queue" << std::endl;
			bool msg_pending{ true };
			while (msg_pending)
			{
				std::unique_lock <std::mutex> txlck{ m_txmutex };
				msg_pending = !m_queue.empty(); // wait for all messages to be sent
				txlck.unlock();
			}

			// wait for all messages to have been processed
			BOOST_LOG_SEV(lg::get(), sev::info) << "Waiting for all messages to complete processing" << std::endl;
			bool msg_waiting{ false };
			do
			{
				std::unique_lock<std::mutex> list_lck{ m_listmutex };
				for (std::list<std::shared_ptr<Message>>::iterator it = m_list.begin(); it != m_list.end(); ++it)
				{
					std::shared_ptr<Message> msg = (*it);
					if ((msg->getStatus() == Message::Status::SENT) ||
						(msg->getStatus() == Message::Status::RX_PARTIAL))
					{
						msg_waiting = true;
					}
				}
				list_lck.unlock();
				if (msg_waiting)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
					msg_waiting = false;
				}
				else {
					break;
				}
			} while (1);

			// Stop Threads
			BOOST_LOG_SEV(lg::get(), sev::info) << "Stopping threads" << std::endl;
			DeviceIsOpen = false;  // must set this to cancel threads
			senderThread.join();
			BOOST_LOG_SEV(lg::get(), sev::debug) << "sender thread joined" << std::endl;
			receiverThread.join();
			BOOST_LOG_SEV(lg::get(), sev::debug) << "receiver thread joined" << std::endl;
			parserThread.join();
			BOOST_LOG_SEV(lg::get(), sev::debug) << "parser thread joined" << std::endl;
			mImpl->memoryTransferThread.join();  // TODO: need to abort in-flight transfers
			BOOST_LOG_SEV(lg::get(), sev::debug) << "memory transfer thread joined" << std::endl;
			mImpl->interruptThread.join();
			BOOST_LOG_SEV(lg::get(), sev::debug) << "interrupt thread joined" << std::endl;
			
			BOOST_LOG_SEV(lg::get(), sev::info) << "Closing sockets" << std::endl;
			//shutdown(mImpl->msgSock, SD_BOTH);
			//shutdown(mImpl->intrSock, SD_BOTH);
#ifdef WIN32
			if (closesocket(mImpl->msgSock) != 0) {
				BOOST_LOG_SEV(lg::get(), sev::warning) << "Client: Cannot close \"SendingSocket\" socket.  Err: " <<  logErrorString() << std::endl;
			}
			if (mImpl->intrSock != INVALID_SOCKET) {
				if (closesocket(mImpl->intrSock) != 0) {
					BOOST_LOG_SEV(lg::get(), sev::warning) << "Client: Cannot close \"InterruptSocket\" socket.  Err: " <<  logErrorString() << std::endl;
				}
			}
#else
			if (close(mImpl->msgSock) != 0)
				BOOST_LOG_SEV(lg::get(), sev::warning) << "Client: Cannot close \"SendingSocket\" socket.  Err: " <<  logErrorString() << std::endl;
			if (mImpl->intrSock != INVALID_SOCKET) {
				if (close(mImpl->intrSock) != 0)
					BOOST_LOG_SEV(lg::get(), sev::warning) << "Client: Cannot close \"InterruptSocket\" socket.  Err: " <<  logErrorString() << std::endl;
			}
#endif
			BOOST_LOG_SEV(lg::get(), sev::info) << "Disconnected." << std::endl;
		}

	}

	void CM_ENET::SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int discover_timeout_ms)
	{
		sendTimeout = std::chrono::milliseconds(send_timeout_ms);
		rxTimeout = std::chrono::milliseconds(rx_timeout_ms);
		autoFreeTimeout = std::chrono::milliseconds(free_timeout_ms);
		mImpl->discovery_timeout = discover_timeout_ms;
	}

	void CM_ENET::MessageSender()
	{
		while (DeviceIsOpen == true)
		{
			std::unique_lock<std::mutex> lck{ m_txmutex };
			m_txcond.wait_for(lck, std::chrono::milliseconds(100));
			// Unblock every 100ms to allow thread to terminate or to process any missed notifications
			if (DeviceIsOpen == false)
			{
				lck.unlock();
				break;
			}

			while (!m_queue.empty())
			{
				std::shared_ptr<Message> m = m_queue.front();

				//std::shared_ptr<CM_ENET::MsgContext> inContext = std::make_shared<CM_ENET::MsgContext>();
				//inContext->handle = m->getMessageHandle();
				
				std::shared_ptr<CM_ENET::MsgContext> outContext = std::make_shared<CM_ENET::MsgContext>();
				outContext->handle = m->getMessageHandle();
				// Get HostReport bytes and send to device
				*outContext->Buffer = m->SerialStream();
				outContext->bufLen = (LONG)outContext->Buffer->size();

				int err;
				m->setStatus(Message::Status::UNSENT);
				std::chrono::time_point<std::chrono::high_resolution_clock> tm_start = std::chrono::high_resolution_clock::now();

#ifdef WIN32
				outContext->DataBuf.buf = (CHAR *)&outContext->Buffer->at(0);
				outContext->DataBuf.len = outContext->bufLen;
				int ret = WSASend(mImpl->msgSock, &outContext->DataBuf, 1, NULL, 0, &outContext->OvLap, NULL);

				if ((ret == SOCKET_ERROR) && (WSA_IO_PENDING != (err = WSAGetLastError()))) {
					// Handle Error
					m->setStatus(Message::Status::SEND_ERROR);
				}

				while (m->getStatus() == Message::Status::UNSENT)
				{
					if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tm_start)) > sendTimeout)
					{
						// Do something with timeout
						m->setStatus(Message::Status::TIMEOUT_ON_SEND);
						mMsgEvent.Trigger<int>(this, MessageEvents::TIMED_OUT_ON_SEND, m->getMessageHandle());  // Notify listeners
					}

					ret = WSAWaitForMultipleEvents(1, &outContext->OvLap.hEvent, TRUE, 10, FALSE);
					if (ret == WSA_WAIT_FAILED) {
						//std::cout << "Wait Failed with error " << WSAGetLastError() << std::endl;
						// Handle Error
						m->setStatus(Message::Status::SEND_ERROR);
					}
					else if (ret == WSA_WAIT_EVENT_0) {
						break;
					}
				} 

				DWORD Flags;
				ret = WSAGetOverlappedResult(mImpl->msgSock, &outContext->OvLap, &outContext->BytesXfer, FALSE, &Flags);
				if (ret == FALSE || outContext->BytesXfer == 0)
				{
					// Handle Error
					m->setStatus(Message::Status::SEND_ERROR);
				}

				if (outContext->BytesXfer != outContext->bufLen)
				{
					// Handle Error
					m->setStatus(Message::Status::SEND_ERROR);
				}


				if (m->getStatus() == Message::Status::UNSENT) {
					m->setStatus(Message::Status::SENT);
				}

#else
				/*int ret = send(mImpl->msgSock, (const void *)&outContext->Buffer->at(0), outContext->bufLen, 0);
				err = errno;

				if (0 > ret) {
					if ((err != EWOULDBLOCK) && (err != EAGAIN)) {
						// Handle Error
						m->setStatus(Message::Status::SEND_ERROR);
					}
				} else {
					outContext->BytesXfer += ret;
				}*/

				int ret;
				while (m->getStatus() == Message::Status::UNSENT) {
					struct pollfd fds;
					fds.fd = mImpl->msgSock;
					fds.events = POLLOUT;

					if (-1 == (ret = poll(&fds, 1, sendTimeout.count())))
					{
						BOOST_LOG_SEV(lg::get(), sev::error) << "Send Error (poll): " <<  logErrorString() << std::endl;
						m->setStatus(Message::Status::SEND_ERROR);
						break;
					}
					else if ( (!ret) ||
						 ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tm_start)) > sendTimeout) )
					{
						// Do something with timeout
						m->setStatus(Message::Status::TIMEOUT_ON_SEND);
						mMsgEvent.Trigger<int>(this, MessageEvents::TIMED_OUT_ON_SEND, m->getMessageHandle());  // Notify listeners
						break;
					} else {
						if ((fds.revents & (POLLERR | POLLHUP)) != 0)
						{
							m->setStatus(Message::Status::SEND_ERROR);
							break;
						}
						else if ((fds.revents & POLLOUT) != 0) {
							ret = send(mImpl->msgSock, (const void *)&outContext->Buffer->at(outContext->BytesXfer), (outContext->bufLen-outContext->BytesXfer), 0);
							err = errno;

							if (0 > ret) {
								if ((err != EWOULDBLOCK) && (err != EAGAIN)) {
									// Handle Error
									BOOST_LOG_SEV(lg::get(), sev::error) << "Send Error (send): " << logErrorString() << std::endl;
									m->setStatus(Message::Status::SEND_ERROR);
									break;

								}
							}
							outContext->BytesXfer += ret;
						}
					}

					if (outContext->BytesXfer >= outContext->bufLen) {
						m->setStatus(Message::Status::SENT);
					}
				}
#endif

				m->MarkSendTime();

				// Place in list for processing by receive thread
				{
					std::unique_lock<std::mutex> list_lck{ m_listmutex };
					m_list.push_back(m);
					list_lck.unlock();
				}
				// Indicate to receive thread that a receive transfer has been started
				//if (m->getStatus() == Message::Status::SENT) {
//					mImpl->m_rxBufcond.notify_one();
				//}

				m_queue.pop();  // delete from queue
			}
			lck.unlock();
		}
	}

	void CM_ENET::ResponseReceiver()
	{
		char szBuffer[CM_ENET::MsgContext::MaxPacketSize];
		fd_set Read;
		int lastRet = 0;

		TIMEVAL Timeout;
		Timeout.tv_sec = 0;
		Timeout.tv_usec = 250000;

		while (DeviceIsOpen == true)
		{
			FD_ZERO(&Read);
			FD_SET(mImpl->msgSock, &Read);
			select(mImpl->msgSock+1, &Read, NULL, NULL, &Timeout);
			if (FD_ISSET(mImpl->msgSock, &Read)) {
				// Receive any bytes present in socket
				//{
				//std::unique_lock<std::mutex> lck{ mImpl->m_rxBufmutex };
				//mImpl->m_rxBufcond.wait_for(lck, std::chrono::milliseconds(25));

				// Receive any bytes present in socket
				int ret = recv(mImpl->msgSock, szBuffer, CM_ENET::MsgContext::MaxPacketSize, 0);
#ifdef WIN32
				int err = WSAGetLastError();
				if ((ret == SOCKET_ERROR) && (WSAEWOULDBLOCK != err))
#else
				int err = errno;
				if ((ret == -1) && (EWOULDBLOCK != err))
#endif
				{
					// Handle Error
					if (lastRet != ret) {
						// Only log once
						BOOST_LOG_SEV(lg::get(), sev::error) << "Receive Error (recv): " <<  logErrorString(err) << " (" << err << ")";
					}
				}
				else if (ret > 0) {
					std::unique_lock<std::mutex> rxlck{ m_rxmutex };
					for (int i = 0; i < ret; i++)
					{
						m_rxCharQueue.push(szBuffer[i]);
						//std::cout << std::hex;
						//std::cout << std::setfill('0') << std::setw(2) << static_cast<int>(szBuffer[i]) << " ";
					}
					// Signal Parser thread
					rxlck.unlock();
					m_rxcond.notify_one();
				}
				lastRet = ret;
			}
		}
	}

	bool CM_ENET::MemoryDownload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		BOOST_LOG_SEV(lg::get(), sev::debug) << "CM_ENET::MemoryDownload addr = " << start_addr << " index = " << image_index << " size = " << arr.size() << std::endl;
		// Only proceed if idle
		if (mImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			BOOST_LOG_SEV(lg::get(), sev::error) << "Memory Transfer not idle" << std::endl;
			return false;
		}
		// Setup transfer
		int length = (int)arr.size();
		length = (((length - 1) / FastTransfer::TRANSFER_GRANULARITY) + 1) * FastTransfer::TRANSFER_GRANULARITY;
		arr.resize(length);  // Increase the buffer size to the transfer granularity
		{
			std::unique_lock<std::mutex> tfr_lck{ mImpl->m_tfrmutex };
			mImpl->fti = new FastTransfer(arr, uuid, length);
		}

		// Signal thread to do the grunt work
		mImpl->FastTransferStatus.store(_FastTransferStatus::DOWNLOADING);
		mImpl->m_tfrcond.notify_one();

		return true;
	}

	bool CM_ENET::MemoryUpload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int len, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		BOOST_LOG_SEV(lg::get(), sev::debug) << "CM_ENET::MemoryUpload addr = " << start_addr << " index = " << image_index << " size = " << len << std::endl;
		// Only proceed if idle
		if (mImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			BOOST_LOG_SEV(lg::get(), sev::error) << "Memory Transfer not idle" << std::endl;
			return false;
		}

		{
			std::unique_lock<std::mutex> tfr_lck{ mImpl->m_tfrmutex };
			mImpl->fti = new FastTransfer(arr, uuid, 0);
		}

		// Signal thread to do the grunt work
		mImpl->FastTransferStatus.store(_FastTransferStatus::UPLOADING);
		mImpl->m_tfrcond.notify_one();

		return true;
	}


	void CM_ENET::MemoryTransfer()
	{
		while (DeviceIsOpen == true)
		{
			{
				std::unique_lock<std::mutex> lck{ mImpl->m_tfrmutex };
				while (!mImpl->m_tfrcond.wait_for(lck, std::chrono::milliseconds(100), [&] {return mImpl->fti != nullptr; }))
				{
					if (mImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE)
					{
						break;
					}
					// Timeout every 100ms to allow threads to terminate on disconnect
					if (DeviceIsOpen == false) break;
				}
				if (DeviceIsOpen == false)
				{
					// End thread
					lck.unlock();
					break;
				}

				BOOST_LOG_SEV(lg::get(), sev::trace) << "Initiating TFTP transfer" << std::endl;
				TFTPClient* client = nullptr;
				
				//int i = sizeof(struct sockaddr_in);
				try {
					client = new TFTPClient(&mImpl->ConnectedServer, TFTP_DEFAULT_PORT);
				} catch (ETFTPSocketCreate e)
				{
					//std::cout << "Unable to connect to iMS Image Server" << std::endl;
					BOOST_LOG_SEV(lg::get(), sev::error) << "Unable to create TFTP socket" << std::endl;
					mMsgEvent.Trigger<int>(this, MessageEvents::DEVICE_NOT_AVAILABLE, -1);
					delete client;
					continue;
				}

				if (mImpl->FastTransferStatus.load() == _FastTransferStatus::DOWNLOADING) {
					if (!client->sendFile(mImpl->fti->m_data, UUIDToStr(mImpl->fti->m_uuid).c_str()))
					{
						mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_ERROR, -1);
					}
				}
				else if (mImpl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) {
					if (!client->getFile(UUIDToStr(mImpl->fti->m_uuid).c_str(), mImpl->fti->m_data))
					{
						mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_ERROR, -1);
					}
				}
				delete client;

				int bytesTransferred = (int)mImpl->fti->m_data.size();
				delete mImpl->fti;
				mImpl->fti = nullptr;

				mImpl->FastTransferStatus.store(_FastTransferStatus::IDLE);
				mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_COMPLETE, bytesTransferred);
			}
		}
	}

	void CM_ENET::InterruptReceiver()
	{
		bool errorLogged = false;
		while (DeviceIsOpen == true)
		{
			std::vector<uint8_t> interruptData(64, 0);

			TIMEVAL Timeout;
			Timeout.tv_sec = 0;
			Timeout.tv_usec = 250000;

			fd_set Read;
			FD_ZERO(&Read);
			FD_SET(mImpl->intrSock, &Read);
			int rc = select(mImpl->intrSock + 1, &Read, NULL, NULL, &Timeout);
			if (rc < 0) {
				// Error
#ifdef WIN32
				int err = WSAGetLastError();
#else
				int err = errno;
#endif
				if (!errorLogged) {
					BOOST_LOG_SEV(lg::get(), sev::error) << "Interrupt Select Error (recv): " << logErrorString(err) << std::endl;
					errorLogged = true;
				}
			}
			else if (rc == 0) {
				// Timeout
			}
			else {

				if (FD_ISSET(mImpl->intrSock, &Read)) {
					// Receive any bytes present in socket
					int ret = recv(mImpl->intrSock, (char*)&interruptData[0], 64, 0);
#ifdef WIN32
					int err = WSAGetLastError();
					if ((ret == SOCKET_ERROR) && (WSAEWOULDBLOCK != err))
#else
					int err = errno;
					if ((ret == -1) && (EWOULDBLOCK != err))
#endif
					{
						// Handle Error
						if (!errorLogged) {
							BOOST_LOG_SEV(lg::get(), sev::error) << "Interrupt Receive Error (recv): " << logErrorString(err) << " (" << err << ")" << std::endl;
							errorLogged = true;
						}
					}
					else if (ret > 0) {
						std::shared_ptr<Message> m = std::make_shared<Message>(HostReport());
						m->MarkSendTime();
						m->setStatus(Message::Status::INTERRUPT);
						interruptData.resize(ret);
						m->AddBuffer(interruptData);

						// Place in list for processing by receive thread
						{
							std::unique_lock<std::mutex> list_lck{ m_listmutex };
							m_list.push_back(m);
							list_lck.unlock();
						}
						// Signal Parser thread
						m_rxcond.notify_one();

						errorLogged = false;
					}
				}
			}
		}
	}
}

#endif
