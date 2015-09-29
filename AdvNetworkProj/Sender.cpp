#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <time.h>
#include <WS2tcpip.h>
#include "port.h"
#include "TcpPacket.h"
#include "TcpConnection.h"
#include <fstream>
#include <queue>
//using namespace std;

#define BUFLEN 2048

int main(void) {
  /* Do 3 way handshake and decide on seq nos */
  TcpConnection connection = TcpConnection(SERVER, SERVICE_PORT);
  int seqNo = TcpConnection::seqNo;
  std::queue<TcpPacket *> packetQueue;

  std::ofstream myfile;
  myfile.open("log.txt");

  std::ifstream is("test.txt", std::ifstream::binary);
  if (is) {

	  int bytestread = 0;
	  bool fileComplete;
	  // get length of file:
	  is.seekg(0, is.end);
	  int length = is.tellg();
	  is.seekg(0, is.beg);
	  printf("Reading file of length %d", length);

	  unsigned int toRead = min(length - bytestread, PACKET_SIZE - HEADER_SIZE);

	  /* Do further communication now */
	  struct sockaddr_in myaddr, remaddr;
	  int fd, slen = sizeof(remaddr);
	  char buf[BUFLEN];	/* message buffer */
	  int recvlen;		/* # bytes in acknowledgement message */


	  /* create a socket */

	  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		  printf("socket created\n");

	  /* bind it to all local addresses and pick any port number */

	  memset((char *)&myaddr, 0, sizeof(myaddr));
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

	  memset((char *)&remaddr, 0, sizeof(remaddr));
	  remaddr.sin_family = AF_INET;
	  remaddr.sin_port = htons(SERVICE_PORT);
	  if (inet_pton(remaddr.sin_family, SERVER, &remaddr.sin_addr) == 0) {
		  fprintf(stderr, "inet_pton() failed\n");
		  perror("inet_pton() failed");
	  }

	  /* Set ssthreshold and congestion window size in terms of no of packets */
	  int ssThresh = 64;
	  int cwnd = 1;
	  int lastAcknowledged = seqNo;	/* ACK no of the last packet received */
	  bool slowStart = true;
	  bool fastRetransmit = false;
	  int CAacksReceived = 0;		/* ACK received in Congestion avoidance phase since last window increase */
	  int lostAck = -1;				/* ACK no of the lost packet indicated by triple dup ack */
	  int dupAcks = 0;
	  int FTcwnd = -1;				/* cnwd when entering the congestion avoidance phase */

	  /* Send one packet */
	  char *buffer = new char[toRead];
	  is.read(buffer, toRead);
	  
	  bool flags[] = { false, false, false, false, false, false, false, false, false };
	  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, time(0), toRead,buffer);
	  printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
	  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
	  
	  bytestread += toRead;
	  toRead = min(length - bytestread, PACKET_SIZE - HEADER_SIZE);
	  free(buffer);
	  //sprintf_s(buf, tcpPacket.buf, i);
	  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
		  perror("sendto");
		  exit(1);
	  }
	  packetQueue.push(newPacket);
	  
	  /* Check if file completely sent */
	  if (toRead == 0) {
		  fileComplete = true;
		  seqNo++;
		  printf("Sending FIN Packet indicating transmission complete");
		  bool flags[] = { false, false, false, false, false, false, false, false, true };
		  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, time(0), PACKET_SIZE,nullptr);
		  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
			  perror("sendto");
			  exit(1);
		  }
		  printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
		  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
		  packetQueue.push(newPacket);
	  }

	  /* now receive an acknowledgement from the server */
	  while (true) {
		  printf("Waiting for ACK\n");
		  /* Set socket timeout */
		  TcpPacket* packet = packetQueue.front();
		  long long diff = time(0);
		  char *endp;
		  long long start = _strtoi64(TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE, TIMESTAMP_SIZE), &endp, 10);
		  struct timeval tv;
		  tv.tv_sec = 0;
		  tv.tv_usec = 3000000 - (long)(diff - start);					/* NEED TO CHECK IF ALWAYS POSITIVE? */
		  fd_set fds;
		  int n;

		  // Set up the file descriptor set.
		  FD_ZERO(&fds);
		  FD_SET(fd, &fds);

		  // Wait until timeout or data received.
		  n = select(fd, &fds, NULL, NULL, &tv);
		  if (n == 0)
		  {
			  /* Restart transmission from last acknowledged packet */
			  printf("Timeout Occurred :( \n");
			  cwnd = 1;
			  //ssThresh = 64;
			  slowStart = true;
			  seqNo = lastAcknowledged;

			  TcpPacket* packet = packetQueue.front();
			  int packetSeqNo = atoi(TcpPacket::getBytes(packet->buf, 0, SEQUENCE_SIZE));
			  if (packetSeqNo != lastAcknowledged) {
				  printf("Queue consistency check failed");
				  exit(1);
			  }
			  if (sendto(fd, packet->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
				  perror("sendto");
				  exit(1);
			  }
			  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
			  printf("Retransmitted packet with seq no %d\n", seqNo);
			  continue;
		  }
		  else if (n == -1)
		  {
			  printf("Error..\n");
			  return 1;
		  }

		  recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
		  if (recvlen >= 0) {
			  bool* Aflags = TcpPacket::getFlags(buf);
			  if (!*(Aflags + ACKBIT)) {
				  perror("ACK bit not set :o\n");
				  exit(1);
			  }
			  free(Aflags);
			  char* ackno = TcpPacket::getBytes(buf, SEQUENCE_SIZE, ACK_SIZE);
			  int ano = atoi(ackno);
			  /* If ack got reordered and we are getting an older ack */
			  if (ano < lastAcknowledged) {
				  continue;
			  }
			  printf("Packet received with ACK no %d\n", ano);
			  myfile << "A " << ano << "\n";
			  free(ackno);

			  /* Clear packets from front of queue */
			  TcpPacket* packet = packetQueue.front();
			  int packetNo = atoi(TcpPacket::getBytes(packet->buf, 0, SEQUENCE_SIZE));
			  while (ano > packetNo)
			  {
				  free(packet);
				  packetQueue.pop();
				  if (!packetQueue.empty()) {
					  packet = packetQueue.front();
					  packetNo = atoi(TcpPacket::getBytes(packet->buf, 0, SEQUENCE_SIZE));
				  }
				  else {
					  break;
				  }

			  }
			  /* packet now contains the packet at top of queue and packetNo is the sequence no of the packet */
			  /* lastAcknowledged == ano during fast retransmit as well */
			  if (!fastRetransmit && lastAcknowledged == ano) {
				  dupAcks++;
			  }
			  if (!fastRetransmit && dupAcks > 0 && lastAcknowledged != ano) {
				  dupAcks = 0;
			  }
			  if (!fastRetransmit && dupAcks == 2) {	/* dupacks is 2 on 3 duplicate acks */
				  fastRetransmit = true;
				  lostAck = lastAcknowledged;
				  if (lostAck != packetNo) {
					  perror("Consistency error\n");
					  exit(1);
				  }
				  cwnd = cwnd / 2;
				  FTcwnd = cwnd;
				  ssThresh = cwnd;
				  dupAcks = 0;
				  printf("FastRestransmit: Re-Sending packet with seq no %d\n", lostAck);
				  myfile << "F " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
				  //sprintf_s(buf, tcpPacket.buf, i);
				  if (sendto(fd, packet->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  continue;
			  }
			  /* CHECK FOR WHAT HAPPENS IF TRIPLE DUP ACK IN SLOW START PHASE */

			  if (fastRetransmit && ano > lostAck) {
				  fastRetransmit = false;
				  cwnd = FTcwnd;
				  lostAck = -1;
			  }

			  /* IMPLEMENT TCP FLOW CONTROL */

			  if (fileComplete) {
				  printf("File completely sent, no more packets to send. Waiting for acks");
				  continue;
			  }

			  if (fastRetransmit) {
				  //cwnd++;
				  int packetsInAir = seqNo - (lastAcknowledged - 1);
				  int canBeSent = cwnd - packetsInAir;
				  /* Send newer packets if you can send more packets */
				  for (int i = 0; i < canBeSent; i++) {
					  seqNo++;
					  char *buffer = new char[toRead];
					  is.read(buffer, toRead);
					  bool flags[] = { false, false, false, false, false, false, false, false, false };
					  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, time(0),toRead,buffer);
					  printf("FastRestransmit: Sending packet with seq no %d\n", seqNo);
					  myfile << "F " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
					  bytestread += toRead;
					  toRead = min(length - bytestread, PACKET_SIZE - HEADER_SIZE);

					  free(buffer);
					  //sprintf_s(buf, tcpPacket.buf, i);
					  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
						  perror("sendto");
						  exit(1);
					  }
					  packetQueue.push(newPacket);

					  /* Check if file completely sent */
					  if (toRead == 0) {
						  fileComplete = true;
						  seqNo++;
						  printf("Sending FIN Packet indicating transmission complete");
						  bool flags[] = { false, false, false, false, false, false, false, false, true };
						  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, time(0),PACKET_SIZE, nullptr);
						  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
							  perror("sendto");
							  exit(1);
						  }
						  printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
						  myfile << "F " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
						  packetQueue.push(newPacket);
						  break;
					  }
				  }
			  }
			  else if (slowStart) {
				  /* May be the ack was such that it acknowledged reception of so many packets that the
				  increase in window size exceeds ssThresh */

				  if (ano - lastAcknowledged > 10) {
					  cwnd = cwnd + 1;
				  }
				  else {
					  cwnd = min(cwnd + (ano - lastAcknowledged), ssThresh);
				  }
				  lastAcknowledged = ano;
				  printf("Slow start: Window increased to %d where ssThresh is %d\n", cwnd, ssThresh);
				  int packetsInAir = seqNo - (lastAcknowledged - 1);
				  int canBeSent = cwnd - packetsInAir;
				  /* Send newer packets */
				  for (int i = 0; i < canBeSent; i++) {
					  seqNo++;
					  char *buffer = new char[toRead];
					  is.read(buffer, toRead);
					  
					  bool flags[] = { false, false, false, false, false, false, false, false, false };
					  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, time(0), toRead, buffer);
					  printf("Sending packet with seq no %d\n", seqNo);
					  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
					  bytestread += toRead;
					  toRead = min(length - bytestread, PACKET_SIZE - HEADER_SIZE);
					  free(buffer);
					  //sprintf_s(buf, tcpPacket.buf, i);
					  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
						  perror("sendto");
						  exit(1);
					  }
					  packetQueue.push(newPacket);
					  /* Check if file completely sent */
					  if (toRead == 0) {
						  fileComplete = true;
						  seqNo++;
						  printf("Sending FIN Packet indicating transmission complete");
						  bool flags[] = { false, false, false, false, false, false, false, false, true };
						  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, time(0),PACKET_SIZE, nullptr);
						  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
							  perror("sendto");
							  exit(1);
						  }
						  printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
						  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
						  packetQueue.push(newPacket);
						  break;
					  }
				  }
				  if (cwnd >= ssThresh) {
					  slowStart = false;
					  CAacksReceived = 0;
				  }
			  }
			  else {
				  //printf("Congestion avoidance phase\n");
				  CAacksReceived++;
				  if (CAacksReceived == cwnd) {
					  cwnd++;
					  CAacksReceived = 0;
					  printf("It's time to increase congestion window to %d\n", cwnd);
				  }
				  lastAcknowledged = ano;
				  printf("Congestion avoidance: Window is %d & ssThresh is %d\n", cwnd, ssThresh);
				  int packetsInAir = seqNo - (lastAcknowledged - 1);
				  int canBeSent = cwnd - packetsInAir;
				  /* Send newer packets */
				  for (int i = 0; i < canBeSent; i++) {
					  seqNo++;
					  char *buffer = new char[toRead];
					  is.read(buffer, toRead);
					  
					  bool flags[] = { false, false, false, false, false, false, false, false, false };
					  TcpPacket * newPacket = new TcpPacket(seqNo, 0, flags, 0, time(0), toRead, buffer);
					  printf("Sending packet with seq no %d\n", seqNo);
					  myfile << "C " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
					  bytestread += toRead;
					  toRead = min(length - bytestread, PACKET_SIZE - HEADER_SIZE);
					  free(buffer);
					  //sprintf_s(buf, tcpPacket.buf, i);
					  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
						  perror("sendto");
						  exit(1);
					  }
					  packetQueue.push(newPacket);
					  /* Check if file completely sent */
					  if (toRead == 0) {
						  fileComplete = true;
						  seqNo++;
						  printf("Sending FIN Packet indicating transmission complete");
						  bool flags[] = { false, false, false, false, false, false, false, false, true };
						  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, time(0), PACKET_SIZE, nullptr);
						  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
							  perror("sendto");
							  exit(1);
						  }
						  printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
						  myfile << "C " << seqNo << " " << cwnd << " " << ssThresh << " " << time(0) << "\n";
						  packetQueue.push(newPacket);
						  break;
					  }
				  }

			  }

			  /* Increment the sequence no */
			  //seqNo++;

		  }
	  }
	  closesocket(fd);
  }
  myfile.close();
  return 0;
}
