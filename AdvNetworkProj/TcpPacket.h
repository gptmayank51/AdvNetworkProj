#pragma once
#define PACKET_SIZE 2048
#define SEQUENCE_SIZE 10
#define ACK_SIZE 10
#define FLAG_SIZE 9
#define WINDOW_SIZE_SIZE 10
#define CHECKSUM_SIZE 5
#define TIMESTAMP_SIZE 13
class TcpPacket
{
		void setBufferValues(int start, int end, char * value);

public:
		char buf[PACKET_SIZE];
		TcpPacket(unsigned int sequence_number,
				unsigned int ack_number,
				bool flags[],
				unsigned int window_size,
				unsigned short checksum,
				unsigned long long int timestamp);
		~TcpPacket();
};