#include "TcpPacket.h"
#include <stdio.h>
#include <string>

void TcpPacket::setBufferValues(char* buf, int start, int size, char *value) {
  if (value == nullptr) {
    return;
  }
  for (int currentDigit = start; currentDigit < start + size; currentDigit++) {
    *(buf + currentDigit) = *(value + currentDigit - start);
  }
}

char* TcpPacket::calculateCsum(char* buf) {
  char* csum = (char *) malloc(sizeof(char) * CHECKSUM_SIZE);
  memset(csum, '\0', CHECKSUM_SIZE);
  for (unsigned int i = 0; i < strlen(buf) / 16; i++) {
    for (int j = 0; j < 16; j++) {
      *(csum + j) = *(csum + j) ^ *(buf + 16 * i + j);
    }
  }
  return csum;
}

char* TcpPacket::getBytes(char* buf, int start, int size) {
  char* ans = (char *) malloc(sizeof(char)*size);
  for (int i = 0; i < size; i++) {
    *(ans + i) = *(buf + start + i);
  }
  return ans;
}

bool* TcpPacket::getFlags(char* buf) {
  bool *flags = (bool *) malloc(sizeof(bool)*FLAG_SIZE);
  memset(flags, 0, FLAG_SIZE);

  int start = SEQUENCE_SIZE + ACK_SIZE;
  // Run loop to 1 less because last character is null character
  for (int i = 0; i < FLAG_SIZE - 1; i++) {
    if (*(buf + start + i) == '1') {
      *(flags + i) = true;
    } else {
      *(flags + i) = false;
    }
  }
  return flags;
}

void TcpPacket::setCsum(char* buf, char* checksum) {
  setBufferValues(buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE, CHECKSUM_SIZE, checksum);
}

unsigned long long int TcpPacket::getTimeStamp(char *buf) {
  char *endptr;
  return strtoull(TcpPacket::getBytes(buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE, TIMESTAMP_SIZE), &endptr, 10);
}

TcpPacket::TcpPacket(
  unsigned int sequence_number,
  unsigned int ack_number,
  bool flags[],
  unsigned int window_size,
  unsigned long long int timestamp,
  unsigned int data_size,
  char * content) {
  if (data_size == 0) {
    buf = (char *) malloc(sizeof(char) * PACKET_SIZE - HEADER_SIZE);
  } else {
    buf = (char *) malloc(sizeof(char) * PACKET_SIZE);
  }
  memset(buf, 0, PACKET_SIZE);
  char unsigned_int_buffer[SEQUENCE_SIZE];
  int current_bit = 0;

  _itoa_s(sequence_number, unsigned_int_buffer, 10);
  setBufferValues(buf, current_bit, SEQUENCE_SIZE, unsigned_int_buffer);
  current_bit += SEQUENCE_SIZE;

  _itoa_s(ack_number, unsigned_int_buffer, 10);
  setBufferValues(buf, current_bit, ACK_SIZE, unsigned_int_buffer);
  current_bit += ACK_SIZE;

  char bool_buffer[9];
  for (int i = 0; i < 9; i++) {
    if (flags[i]) {
      bool_buffer[i] = '1';
    } else {
      bool_buffer[i] = '0';
    }
  }
  setBufferValues(buf, current_bit, FLAG_SIZE, bool_buffer);
  current_bit += FLAG_SIZE;

  _itoa_s(window_size, unsigned_int_buffer, 10);
  setBufferValues(buf, current_bit, WINDOW_SIZE_SIZE, unsigned_int_buffer);
  current_bit += WINDOW_SIZE_SIZE;

  // Leave space for checksum
  current_bit += CHECKSUM_SIZE;

  char timestamp_buffer[TIMESTAMP_SIZE];
  _i64toa_s(timestamp, timestamp_buffer, TIMESTAMP_SIZE, 10);
  setBufferValues(buf, current_bit, TIMESTAMP_SIZE, timestamp_buffer);
  current_bit += TIMESTAMP_SIZE;

  _itoa_s(data_size, unsigned_int_buffer, 10);
  setBufferValues(buf, current_bit, DATA_SIZE_SIZE, unsigned_int_buffer);
  current_bit += DATA_SIZE_SIZE;

  if (data_size != 0) {
    setBufferValues(buf, current_bit, data_size, content);
  }

  // Calculate checksum and set it
  char* csum = calculateCsum(buf);
  setCsum(buf, csum);
  free(csum);
}

TcpPacket::~TcpPacket() {
  free(buf);
}
