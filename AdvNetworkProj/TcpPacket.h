#pragma once
#define PACKET_SIZE 2048
#define SEQUENCE_SIZE 11
#define ACK_SIZE 11
#define FLAG_SIZE 10
#define WINDOW_SIZE_SIZE 11
#define CHECKSUM_SIZE 6
#define TIMESTAMP_SIZE 14
class TcpPacket
{
		void setBufferValues(int start, int end, char * value);

public:
		char* buf;
		TcpPacket(unsigned int sequence_number,
				unsigned int ack_number,
				bool flags[],
				unsigned int window_size,
				unsigned short checksum,
				unsigned long long int timestamp);
		~TcpPacket();
};