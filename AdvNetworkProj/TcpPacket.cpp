#include "TcpPacket.h"
#include <stdio.h>
#include <string>

void TcpPacket::setBufferValues(int start, int end, char * value)
{
		for (int currentDigit = start; currentDigit < end; currentDigit++) {
				buf[currentDigit] = *(value + start - currentDigit);
		}
}

TcpPacket::TcpPacket(
		unsigned int sequence_number,
		unsigned int ack_number,
		bool flags[],
		unsigned int window_size,
		unsigned short checksum,
		unsigned long long int timestamp) {
		memset(buf, 0, PACKET_SIZE);
		char unsigned_int_buffer[10];
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

		char short_buffer[5];
		_itoa_s(checksum, short_buffer, 10);
		setBufferValues(current_bit, CHECKSUM_SIZE, short_buffer);
		current_bit += CHECKSUM_SIZE;


		char* timestamp_buffer;
		timestamp_buffer = (char *)malloc(sizeof(char) * sizeof(timestamp));
		for (int i = 0; i < sizeof(timestamp); i++)
		{
				timestamp_buffer[i] = ((timestamp >> (i * 8)) & 0XFF);
		}
		setBufferValues(current_bit, CHECKSUM_SIZE, timestamp_buffer);
		current_bit += TIMESTAMP_SIZE;
}

TcpPacket::~TcpPacket() {
		free(buf);
}
