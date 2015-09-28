#include "TcpConnection.h"
#include "TcpPacket.h"
#include "Network.h"
#include "port.h"
#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <iostream>

#define BUFLEN 2048

int TcpConnection::seqNo;
TcpConnection::TcpConnection(char* ip, int port) {

	struct sockaddr_in myaddr, remaddr;
	int fd, slen = sizeof(remaddr);
	char buf[BUFLEN];	/* message buffer */
	int recvlen;		/* # bytes in acknowledgement message */
	

	srand(time(NULL));
	bool flags[] = { false, false, false, false, false, false, false, true, false };
	seqNo = rand() % 10000;
	TcpPacket synPacket(seqNo, rand(), flags, 1u, time(0));

	/* Initialize Network */
	Network network = Network();

	/* create a socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		printf("socket created\n");

	/* bind it to all local addresses and pick any port number */

  memset((char *) &myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(0);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return;
	}

	/* now define remaddr, the address to whom we want to send messages */
	/* For convenience, the host address is expressed as a numeric IP address */
	/* that we will convert to a binary format via inet_aton */

  memset((char *) &remaddr, 0, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(SERVICE_PORT);
	if (inet_pton(remaddr.sin_family, SERVER, &remaddr.sin_addr) == 0) {
		fprintf(stderr, "inet_pton() failed\n");
		exit(1);
	}

	/* Set socket timeout */
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(tv)) < 0) {
		perror("socket timeout set failed");
		exit(1);
	}
	

	printf("Sending packet to %s port %d\n", SERVER, SERVICE_PORT);
	if (sendto(fd, synPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
		perror("sendto");
		exit(1);
	}
	printf("syn packet sent\n");
	/* now receive an acknowledgement from the server */
	recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
	if (recvlen >= 0) {
		/* check if ACK & SYN flag set in response*/
		bool* Aflags = TcpPacket::getFlags(buf);
		if (*(Aflags + ACKBIT) && *(Aflags + SYNBIT)) {
			printf("Ack received\n");
			char* seqno = TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE);
			int AseqNo = atoi(seqno);
			char* ackno = TcpPacket::getBytes(buf, SEQUENCE_SIZE, ACK_SIZE);
			int ano = atoi(ackno);
			if (ano != seqNo + 1) {
				perror("seq error mismtach");
				exit(1);
			}

			/* construct ack for the packet just received*/
			bool flags[] = { false, false, false, false, true, false, false, false, false };
			seqNo++;
      TcpPacket ackPacket(seqNo, AseqNo + 1, flags, 1u, time(0));
			if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
				perror("ackPacket");
				exit(1);
			}
			printf("Three way handshake complete\n");
			
			/* Free memory */
			free(seqno);
			free(ackno);
    } else {
			perror("Ack not received");
			exit(1);
		}
		free(Aflags);
	}
	closesocket(fd);
	
}


TcpConnection::~TcpConnection() {}
