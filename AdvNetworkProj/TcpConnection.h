#pragma once
class TcpConnection
{
public:
	static int seqNo;
	TcpConnection(char* ip, int port);
	~TcpConnection();
};

