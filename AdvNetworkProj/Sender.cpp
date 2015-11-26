#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <WinSock2.h>
#include <Windows.h>
#include <time.h>
#include <sys\timeb.h> 
#include <WS2tcpip.h>
#include "port.h"
#include "TcpPacket.h"
#include "TcpConnection.h"
#include <fstream>
#include <list>
#include <queue>

#define BUFLEN 2048

LONGLONG getTime() {
  SYSTEMTIME time;
  GetSystemTime(&time);
  WORD fe = 0;
  return time.wHour * 86400000ll + time.wMinute * 60000ll + time.wSecond * 1000ll + time.wMilliseconds;
}

int main(void) {
  /* Do 3 way handshake and decide on seq nos */
  TcpConnection connection = TcpConnection(SERVER, SERVICE_PORT);
  int seqNo = TcpConnection::seqNo;
  std::list<TcpPacket *> packetList;

  std::ofstream myfile;
  myfile.open("log.txt");


  std::ifstream is("test.mp3", std::ios::binary);
  if (is) {

    bool fileComplete = false;
    // get length of file:
    is.seekg(0, is.end);
    int fileLength = is.tellg();
    is.seekg(0, is.beg);
    printf("Reading file of length %d\n", fileLength);


    /* Do further communication now */
    struct sockaddr_in myaddr, remaddr;
    int fd, slen = sizeof(remaddr);
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
      printf("inet_pton() failed\n");
      perror("inet_pton() failed");
    }

    /* Set ssthreshold and congestion window size in terms of no of packets */
    int ssThresh = 32;
    int cwnd = 1;
    int lastAcknowledged = seqNo;	/* ACK no of the last packet received */
    bool slowStart = true;
    bool fastRetransmit = false;
    int CAacksReceived = 0;		/* ACK received in Congestion avoidance phase since last window increase */
    int lostAck = -1;				/* ACK no of the lost packet indicated by triple dup ack */
    int dupAcks = 0;
    int FTcwnd = -1;				/* cnwd when entering the congestion avoidance phase */
    int lastSeqNo = -1;				/* SeqNo of last */
    int recover = -1;				/* SeqNo of packet when entering Fast Retransmit */
    bool same = false;
	int qSeqNo = -1;
	std::list<TcpPacket *>::iterator currentPack = packetList.end();

    struct timeb startT, endT;
    ftime(&startT);
    /* Send one packet */
    char *buffer = (char *) malloc(sizeof(char)*CONTENT_SIZE);
    memset(buffer, '\0', CONTENT_SIZE);
    is.read(buffer, CONTENT_SIZE - 1);
    printf("File read\n");
    bool flags[] = { false, false, false, false, false, false, false, false, false };
    LONGLONG millis = getTime();
    TcpPacket * newPacket = new TcpPacket(seqNo, 1, flags, cwnd, millis, (unsigned int) is.gcount(), buffer);
	qSeqNo = seqNo;
    printf("Sending packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
    myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
    free(buffer);
    //free(buffer);
    //sprintf_s(buf, tcpPacket.buf, i);
    if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
      perror("sendto");
      exit(1);
    }
    //packetQueue.push(newPacket);
	packetList.push_front(newPacket);

    /* Check if file completely sent */
    if (is.eof()) {
      fileComplete = true;
      seqNo++;
      printf("Sending FIN Packet indicating transmission complete");
      bool flags[] = { false, false, false, false, false, false, false, false, true };

      millis = getTime();
      TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, millis, PACKET_SIZE, nullptr);
      if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
        perror("sendto");
        exit(1);
      }
      lastSeqNo = seqNo;
      printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
      myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
      //packetQueue.push(newPacket);
	  packetList.push_front(newPacket);
    }

    /* now receive an acknowledgement from the server */
    while (true) {
      printf("Waiting for ACK\n");
      /* Set socket timeout */
      //TcpPacket* packet = packetQueue.front();
	  TcpPacket* packet = packetList.front();

      millis = getTime();
      long long diff = millis;
      char *endp;
      long long start = _strtoi64(TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE, TIMESTAMP_SIZE), &endp, 10);
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 200000 - (long) (diff - start);					/* NEED TO CHECK IF ALWAYS POSITIVE? */
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
        /* Reset all variables */
        same = false;
        fastRetransmit = false;
        dupAcks = 0;
        recover = -1;
        lostAck = -1;
		currentPack = packetList.begin();
		seqNo = lastAcknowledged;
        cwnd = 1;
        //ssThresh = 64;
        slowStart = true;

        //TcpPacket* packet = packetQueue.front();
		TcpPacket* packet = packetList.front();
        int packetSeqNo = atoi(TcpPacket::getBytes(packet->buf, 0, SEQUENCE_SIZE));
        if (packetSeqNo != lastAcknowledged) {
          printf("Queue consistency check failed, Queue top is %d while lastAcked is %d\n", packetSeqNo, lastAcknowledged);
          exit(1);
        }
		int nL = atoi(TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE, DATA_SIZE_SIZE));
		char * nBuf = TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE+DATA_SIZE_SIZE , nL);
		millis = getTime();
		bool flags[] = { false, false, false, true, false, false, false, false, false };
		TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)nL, nBuf);
        if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
          perror("sendto");
          exit(1);
        }
		packetList.pop_back();
		free(packet);
		packetList.push_back(newPacket);
		packet = newPacket;
		currentPack++;
        myfile << "T " << lastAcknowledged << " " << cwnd << " " << ssThresh << " " << millis << "\n";
        printf("Timeout, restarting transimission from seq no %d\n", packetSeqNo);
        continue;
      } else if (n == -1)
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

        char **endp1 = nullptr;

        cwnd = min(cwnd, strtoul(TcpPacket::getBytes(buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE, WINDOW_SIZE_SIZE), endp1, 10));


        char* ackno = TcpPacket::getBytes(buf, SEQUENCE_SIZE, ACK_SIZE);
        int ano = atoi(ackno);
        /* If ack got reordered and we are getting an older ack */
		free(ackno);
		if (ano < lastAcknowledged) {
          continue;
        }


        printf("Packet received with ACK no %d\n", ano);
        myfile << "A " << ano << "\n";
        
        // Check if this is the last ACK expected.
        if (fileComplete && ano == lastSeqNo+1) {
          printf("All packets acknowledged! Yay!!! :D :D :D\n");
          ftime(&endT);
          diff = (int) (1000.0 * (endT.time - startT.time)
            + (endT.millitm - startT.millitm));

          printf("\n********* Operation took %u milliseconds and throughput is %d ************\n", diff,((1000.0*fileLength)/(double)diff));
          break;
        }

        /* Clear packets from front of queue */
		TcpPacket* packet = packetList.front();
        int packetNo = atoi(TcpPacket::getBytes(packet->buf, 0, SEQUENCE_SIZE));
        while (ano > packetNo)
        {
          free(packet);
		  packetList.pop_back();
		  if (!packetList.empty()) {
            packet = packetList.front();
            packetNo = atoi(TcpPacket::getBytes(packet->buf, 0, SEQUENCE_SIZE));
          } else {
            break;
          }
        }

        /* packet now contains the packet at top of queue and packetNo is the sequence no of the packet */
        /* lastAcknowledged == ano during fast retransmit as well */
        if (!same && lastAcknowledged == ano) {
          dupAcks++;
        }
        if (same && lastAcknowledged != ano) {
          lastAcknowledged = ano;
          dupAcks = 0;
          same = false;
        }
        if (dupAcks == 3) {	/* dupacks is 2 on 3 duplicate acks */
          if (!fastRetransmit) {
            fastRetransmit = true;
            recover = seqNo;
            ssThresh = max(cwnd / 2, 2);
            cwnd = ssThresh + 3;
            FTcwnd = ssThresh;
          }
          dupAcks = 0;
          same = true;
          lostAck = lastAcknowledged;
          if (lostAck != packetNo) {
            perror("Consistency error\n");
            exit(1);
          }

          millis = getTime();
          printf("FastRestransmit: Re-Sending packet with seq no %d\n", lostAck);
          myfile << "F " << lostAck << " " << cwnd << " " << ssThresh << " " << millis << "\n";
          //sprintf_s(buf, tcpPacket.buf, i);
          if (sendto(fd, packet->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
            perror("sendto");
            exit(1);
          }
          continue;
        }
        /* CHECK FOR WHAT HAPPENS IF TRIPLE DUP ACK IN SLOW START PHASE */

        if (fastRetransmit && ano > lostAck && ano >= recover) {
          printf("Leaving fast retransmit");
          fastRetransmit = false;
          same = false;
          recover = -1;
          CAacksReceived = 0;
          cwnd = min(FTcwnd, seqNo - ano + 1);
          if (cwnd < ssThresh) {
            slowStart = true;
          }
          lostAck = -1;
        } else if (fastRetransmit && ano > lostAck && ano < recover) {
          cwnd = cwnd - (ano - lostAck);
          lostAck = ano;
          printf("Received an ACK(%d) for retransmitted packet(%d), but still less than recover(%d). Reducind window to %d\n", ano, lostAck, recover, cwnd);
        }

        /* IMPLEMENT TCP FLOW CONTROL */

        if (fileComplete) {
		  lastAcknowledged = ano;
          printf("File completely sent, no more packets to send. Waiting for acks.\n");
          continue;
        }

        if (fastRetransmit) {
		  lastAcknowledged = ano;
          printf("FastRetransmit: Increasing window by 1. cwnd is %d", cwnd);
          cwnd++;
          int packetsInAir = seqNo - (lastAcknowledged - 1);
          int canBeSent = cwnd - packetsInAir;
          /* Send newer packets if you can send more packets */
		  for (int i = 0; i < canBeSent; i++) {
			  seqNo++;
			  if (currentPack != packetList.end()) {
				  /* Packet exists in queue already */
				  packet = *currentPack;
				  
				  int nL = atoi(TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE, DATA_SIZE_SIZE));
				  char * nBuf = TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE + DATA_SIZE_SIZE, nL);
				  millis = getTime();
				  bool flags[] = { false, false, false, false, false, false, false, false, false };
				  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)nL, nBuf);
				  *currentPack = newPacket;
				  free(packet);
				  printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
				  myfile << "F " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
				  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  currentPack++;
			  }
			  else {
				  /* Check if file completely sent */
				  if (is.eof()) {
					  fileComplete = true;
					  printf("Sending FIN Packet indicating transmission complete\n");
					  bool flags[] = { false, false, false, false, false, false, false, false, true };
					  millis = getTime();
					  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, millis, PACKET_SIZE, nullptr);
					  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
						  perror("sendto");
						  exit(1);
					  }
					  lastSeqNo = seqNo;
					  printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
					  myfile << "F " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
					  packetList.push_front(newPacket);
					  break;
				  }
				  char *buffer = new char[CONTENT_SIZE];
				  memset(buffer, '\0', CONTENT_SIZE);
				  is.read(buffer, CONTENT_SIZE - 1);

				  bool flags[] = { false, false, false, false, false, false, false, false, false };

				  millis = getTime();
				  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)is.gcount(), buffer);
				  printf("FastRestransmit: Sending packet with seq no %d and size %d\n", seqNo, (unsigned int)is.gcount());
				  myfile << "F " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";

				  //free(buffer);
				  //sprintf_s(buf, tcpPacket.buf, i);
				  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  packetList.push_front(newPacket);

			  }
		  }
        } else if (slowStart) {
          /* May be the ack was such that it acknowledged reception of so many packets that the
          increase in window size exceeds ssThresh */

          if (ano - lastAcknowledged > 10) {
			  printf("SHOULDN'T BE HERE!!!");
            cwnd = cwnd + 1;
          } else {
            cwnd = min(cwnd + (ano - lastAcknowledged), ssThresh);
          }
          lastAcknowledged = ano;
          printf("Slow start: Window increased to %d where ssThresh is %d\n", cwnd, ssThresh);
          int packetsInAir = seqNo - (lastAcknowledged - 1);
          int canBeSent = cwnd - packetsInAir;
          char *buffer = new char[CONTENT_SIZE];
          memset(buffer, '\0', CONTENT_SIZE);
          /* Send newer packets */
		  for (int i = 0; i < canBeSent; i++) {
			  seqNo++;
			  if (currentPack != packetList.end()) {
				  /* Packet exists in queue already */
				  packet = *currentPack;
				  
				  int nL = atoi(TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE, DATA_SIZE_SIZE));
				  char * nBuf = TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE + DATA_SIZE_SIZE, nL);
				  millis = getTime();
				  bool flags[] = { false, false, false, false, false, false, false, false, false };
				  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)nL, nBuf);
				  *currentPack = newPacket;
				  free(packet);
				  printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);

				  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
				  if (sendto(fd, packet->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  currentPack++;
			  }
			  else
			  {
				/* Check if file completely sent */
				if (is.eof()) {
				  fileComplete = true;
				  printf("Sending FIN Packet indicating transmission complete SLOW START\n");
				  bool flags[] = { false, false, false, false, false, false, false, false, true };

				  millis = getTime();
				  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, millis, PACKET_SIZE, nullptr);
				  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  lastSeqNo = seqNo;
				  printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
				  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
				  packetList.push_front(newPacket);
				  break;
				}

				  memset(buffer, '\0', CONTENT_SIZE);
				  is.read(buffer, CONTENT_SIZE - 1);
				  bool flags[] = { false, false, false, false, false, false, false, false, false };
				  millis = getTime();
				  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)is.gcount(), buffer);
				  printf("Sending packet of size %d with seq no %d\n", (unsigned int)is.gcount(), seqNo);
				  myfile << "S " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
				  //free(buffer);
				  //sprintf_s(buf, tcpPacket.buf, i);
				  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  packetList.push_front(newPacket);
			}
          }
          if (cwnd >= ssThresh) {
            slowStart = false;
            CAacksReceived = 0;
          }
        } else {
          //printf("Congestion avoidance phase\n");
          CAacksReceived++;
          if (CAacksReceived == cwnd) {
            cwnd++;
            CAacksReceived = 0;
            printf("It's time to increase congestion window to %d\n", cwnd);
          }
          lastAcknowledged = ano;
          printf("Congestion avoidance: CAacksRecieved is %d, Window is %d & ssThresh is %d\n", CAacksReceived, cwnd, ssThresh);
          int packetsInAir = seqNo - (lastAcknowledged - 1);
          int canBeSent = cwnd - packetsInAir;
          /* Send newer packets */
		  for (int i = 0; i < canBeSent; i++) {
			  seqNo++;
			  if (currentPack != packetList.end()) {
				  /* Packet exists in queue already */
				  packet = *currentPack;
				  int nL = atoi(TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE, DATA_SIZE_SIZE));
				  char * nBuf = TcpPacket::getBytes(packet->buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE + DATA_SIZE_SIZE, nL);
				  millis = getTime();
				  bool flags[] = { false, false, false, false, false, false, false, false, false };
				  TcpPacket *newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)nL, nBuf);
				  *currentPack = newPacket;
				  free(packet);

				  printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
				  myfile << "C " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
				  if (sendto(fd, packet->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  currentPack++;
			  }
			  else
			  {
			  /* Check if file completely sent */
			  if (is.eof()) {
				  fileComplete = true;

				  printf("Sending FIN Packet indicating transmission complete\n");
				  bool flags[] = { false, false, false, false, false, false, false, false, true };
				  millis = getTime();
				  TcpPacket* newPacket = new TcpPacket(seqNo, 1, flags, cwnd, millis, PACKET_SIZE, nullptr);
				  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
					  perror("sendto");
					  exit(1);
				  }
				  lastSeqNo = seqNo;
				  printf("Sending last packet with seq no %d to %s port %d\n", seqNo, SERVER, SERVICE_PORT);
				  myfile << "C " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
				  packetList.push_front(newPacket);
				  break;
			  }

			  char *buffer = new char[CONTENT_SIZE];
			  memset(buffer, '\0', CONTENT_SIZE);
			  is.read(buffer, CONTENT_SIZE - 1);
			  bool flags[] = { false, false, false, false, false, false, false, false, false };
			  millis = getTime();
			  TcpPacket * newPacket = new TcpPacket(seqNo, 0, flags, 0, millis, (unsigned int)is.gcount(), buffer);
			  printf("Sending packet with seq no %d and size %d\n", seqNo, (unsigned int)is.gcount());
			  myfile << "C " << seqNo << " " << cwnd << " " << ssThresh << " " << millis << "\n";
			  //free(buffer);
			  //sprintf_s(buf, tcpPacket.buf, i);
			  if (sendto(fd, newPacket->buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
				  perror("sendto");
				  exit(1);
			  }
			  packetList.push_front(newPacket);
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
