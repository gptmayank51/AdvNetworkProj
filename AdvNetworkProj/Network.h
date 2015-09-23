#pragma once
class Network
{
public:
	static void sendPacket(char* ip, int port, int size, char* buf);
	Network();
	~Network();
};

