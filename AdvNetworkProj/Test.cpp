#include "TcpPacket.h"
#include <iostream>
int hello(void) {
		unsigned short csum = (unsigned short)123;
		bool flags[] = { true, false, true, false, true, false, true, false, true };
		TcpPacket tcpPacket(123U, 456u, flags, 2048u, 19238293801ull);
		std::cout << tcpPacket.buf;
		for (int i = 0; i < PACKET_SIZE; i++) {
			std::cout << *(tcpPacket.buf + i);
		}
		return 0;
}