#include "TcpPacket.h"
#include <stdio.h>
#include <string>

void TcpPacket::setBufferValues(int start, int size, char * value)
{
		for (int currentDigit = start; currentDigit < start + size; currentDigit++) {
			/*if (*(value + start - currentDigit) == '\0') {
			}*/
				buf[currentDigit] = *(value + currentDigit - start);
		}
}


char TcpPacket::calculateCsum(char* buf) {
	char csum = *buf;
	for (int i = 0; i < PACKET_SIZE; i++) {
		csum = csum^*(buf + i);
	}
	return csum;
}

char* TcpPacket::getBytes(char* buf, int start, int size) {
	char* ans = (char *)malloc(sizeof(char)*size);
	for (int i = 0; i < size; i++) {
		*(ans + i) = *(buf + start + i);
	}
	return ans;
}

bool* TcpPacket::getFlags(char* buf) {
	bool *flags = (bool *)malloc(sizeof(bool)*FLAG_SIZE);
	memset(flags, 0, FLAG_SIZE);
	
	int start = SEQUENCE_SIZE + ACK_SIZE;
	// Run loop to 1 less because last character is null character
	for (int i = 0; i < FLAG_SIZE-1; i++) {
		if (*(buf + start + i) == '1') {
			*(flags + i) = true;
		}
		else {
			*(flags + i) = false;
		}
	}
	return flags;
}

void TcpPacket::setCsum(unsigned short checksum) {
	char short_buffer[CHECKSUM_SIZE];
	_itoa_s(checksum, short_buffer, 10);
	setBufferValues(SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE, CHECKSUM_SIZE, short_buffer);
}

TcpPacket::TcpPacket(
		unsigned int sequence_number,
		unsigned int ack_number,
		bool flags[],
		unsigned int window_size,
		unsigned long long int timestamp) {
		buf = (char *)malloc(sizeof(char)*PACKET_SIZE);
		memset(buf, 0, PACKET_SIZE);
		char unsigned_int_buffer[SEQUENCE_SIZE];
		int current_bit = 0;
		
		_itoa_s(sequence_number, unsigned_int_buffer, 10);
		setBufferValues(current_bit, SEQUENCE_SIZE, unsigned_int_buffer);
		current_bit += SEQUENCE_SIZE;
		
		_itoa_s(ack_number, unsigned_int_buffer, 10);
		setBufferValues(current_bit, ACK_SIZE, unsigned_int_buffer);
		current_bit += ACK_SIZE;

		char bool_buffer[9];
		for (int i = 0; i < 9; i++) {
				if (flags[i]) {
						bool_buffer[i] = '1';
				}
				else {
						bool_buffer[i] = '0';
				}
		}
		setBufferValues(current_bit, FLAG_SIZE, bool_buffer);
		current_bit += FLAG_SIZE;

		_itoa_s(window_size, unsigned_int_buffer, 10);
		setBufferValues(current_bit, WINDOW_SIZE_SIZE, unsigned_int_buffer);
		current_bit += WINDOW_SIZE_SIZE;

		// Leave space for checksum
		current_bit += CHECKSUM_SIZE;

		char timestamp_buffer[TIMESTAMP_SIZE];
		_i64toa_s(timestamp, timestamp_buffer,TIMESTAMP_SIZE,10);
		setBufferValues(current_bit, TIMESTAMP_SIZE, timestamp_buffer);
		current_bit += TIMESTAMP_SIZE;

		// Calculate checksum and set it
		char csum = calculateCsum(buf);
		setCsum(csum);
}

TcpPacket::~TcpPacket() {
		free(buf);
}
