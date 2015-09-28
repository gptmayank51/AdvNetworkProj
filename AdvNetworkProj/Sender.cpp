#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include "port.h"
#include "TcpPacket.h"
#include "TcpConnection.h"


#define BUFLEN 2048

int send(void) {
	/* Do 3 way handshake and decide on seq nos */
  TcpConnection connection = TcpConnection(SERVER, SERVICE_PORT);
	int seqNo = TcpConnection::seqNo;
	

	/* Do further communication now */
	struct sockaddr_in myaddr, remaddr;
	int fd, i, slen = sizeof(remaddr);
	char buf[BUFLEN];	/* message buffer */
	int recvlen;		/* # bytes in acknowledgement message */
	
	
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
			return 0;
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

	/* Set ssthreshold and congestion window size in terms of no of packets */
	int ssThresh = 16;
	int cwnd = 1;
	int lastAcknowledged = seqNo - 1;
	bool slowStart = true;
	int CAacksReceived = 0;


	/* Send one packet */
	bool flags[] = { false, false, false, false, false, false, false, false, false };
	TcpPacket packet = TcpPacket(seqNo, 1, flags, cwnd, time(0));
	printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
	//sprintf_s(buf, tcpPacket.buf, i);
			if (sendto(fd, tcpPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
		perror("sendto");
		exit(1);
	}
			/* now receive an acknowledgement from the server */
	while (true) {
			recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
			if (recvlen >= 0) {
				bool* Aflags = TcpPacket::getFlags(buf);
				if (!*(Aflags + ACKBIT)) {
					perror("ACK bit not set :o");
					exit(1);
				}
				free(Aflags);
				char* ackno = TcpPacket::getBytes(buf, SEQUENCE_SIZE, ACK_SIZE);
				int ano = atoi(ackno);
				/* If ack got reordered and we are getting an older ack */
				if (ano < lastAcknowledged) {
					continue;
				}
				printf("Packet received with ACK no %d", ano);
				free(ackno);

				/* IMPLEMENT TCP FLOW CONTROL */

				if (slowStart) {
					/* May be the ack was such that it acknowledged reception of so many packets that the 
					increase in window size exceeds ssThresh */
					cwnd = min(cwnd + (ano - lastAcknowledged), ssThresh);
					lastAcknowledged = ano;
					printf("Slow start: Window increased to %d where ssThresh is %d", cwnd, ssThresh);
					int packetsInAir = seqNo - lastAcknowledged;
					int canBeSent = cwnd - packetsInAir;
					/* Send newer packets */
					for (int i = 0; i < canBeSent; i++) {
						seqNo++;
						bool flags[] = { false, false, false, false, false, false, false, false, false };
						TcpPacket packet = TcpPacket(seqNo, 0, flags, 0, time(0));
						printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
						//sprintf_s(buf, tcpPacket.buf, i);
						if (sendto(fd, packet.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
							perror("sendto");
							exit(1);
						}
					}
					if (cwnd >= ssThresh) {
						slowStart = false;
						CAacksReceived = 0;
					}
				}
				else {
					printf("Congestion avoidance phase");
					lastAcknowledged = ano;
				}

				/* Increment the sequence no */
				seqNo++;
			
			}
			Sleep(100);
	}
	closesocket(fd);
return 0;
}
