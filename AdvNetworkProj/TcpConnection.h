#pragma once
class TcpConnection
{
public:
	int seqNo;
	TcpConnection(char* ip, int port);
	~TcpConnection();
};

