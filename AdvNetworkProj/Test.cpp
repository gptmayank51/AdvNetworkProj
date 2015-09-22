#include "TcpPacket.h"
#include <iostream>
void main() {
		unsigned short csum = (unsigned short)123;
		bool flags[] = { true, false, true, false, true, false, true, false, true };
		TcpPacket tcpPacket(123U, 456u, flags, 2048u, csum, 19238293801ull);
		std::cout << tcpPacket.buf;
}