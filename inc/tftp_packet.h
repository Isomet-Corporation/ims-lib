#ifndef TFTP_PACKET
#define TFTP_PACKET

typedef unsigned char BYTE;
typedef unsigned short WORD;

#ifdef _WIN32
#else
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
#define INVALID_SOCKET (0xffff)
#define SOCKET_ERROR -1
#endif

#define TFTP_OPCODE_READ     1
#define TFTP_OPCODE_WRITE    2
#define TFTP_OPCODE_DATA     3
#define TFTP_OPCODE_ACK      4
#define TFTP_OPCODE_ERROR    5

#define TFTP_PACKET_MAX_SIZE 1024
#define TFTP_PACKET_DATA_SIZE 512

		//"netascii", "octet", or "mail"
#define TFTP_DEFAULT_TRANSFER_MODE "octet"

#define TFTP_ERROR_0 "Not defined, see error message (if any)"
#define TFTP_ERROR_1 "File not found"
#define TFTP_ERROR_2 "Access violation"
#define TFTP_ERROR_3 "Disk full or allocation exceeded"
#define TFTP_ERROR_4 "Illegal TFTP operation"
#define TFTP_ERROR_5 "Unknown transfer ID"
#define TFTP_ERROR_6 "File already exists"
#define TFTP_ERROR_7 "No such user"

#define TFTP_DEFAULT_PORT 69

class TFTP_Packet {

	private:
		

	protected:
		
		int current_packet_size;
		unsigned char data[TFTP_PACKET_MAX_SIZE];

	public:

		TFTP_Packet();
		~TFTP_Packet();
		void clear();

		int getSize();
		bool setSize(int size);

		void dumpData();

		bool addByte(BYTE b);
		bool addWord(WORD w);
		bool addString(const char* str);
		bool addMemory(const char* buffer, int len);
		
		BYTE getByte(int offset);
		WORD getWord(int offset = 0);
		bool getString(int offset, char* buffer, int length);
		WORD getNumber();
		unsigned char* getData(int offset = 0);
		int copyData(int offset, char* dest, int length);

		bool createRRQ(const char* filename);
		bool createWRQ(const char* filename);
		bool createACK(int packet_num);
		bool createData(int block, char* data, int data_size);
		bool createError(int error_code, char* message);

		bool sendPacket(TFTP_Packet*);

		bool isRRQ();
		bool isWRQ();
		bool isACK();
		bool isData();
		bool isError();

};

#endif
