#pragma once
#define PACKET_SIZE 2048
#define SEQUENCE_SIZE 11
#define ACK_SIZE 11
#define FLAG_SIZE 10
#define WINDOW_SIZE_SIZE 11
#define CHECKSUM_SIZE 4
#define TIMESTAMP_SIZE 14
class TcpPacket {
  static void setBufferValues(char* buf, int start, int end, char * value);

public:
#define ACKBIT 4
#define SYNBIT 7
  static char* calculateCsum(char* buf);
  static void setCsum(char*, char*);
  static bool* getFlags(char* buf);
  static char* getBytes(char* buf, int start, int size);
  char* buf;

  TcpPacket(unsigned int sequence_number,
    unsigned int ack_number,
    bool flags[],
    unsigned int window_size,
    unsigned long long int timestamp);
  ~TcpPacket();
};