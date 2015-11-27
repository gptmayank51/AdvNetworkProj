#include <algorithm>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include "Network.h"
#include "port.h"
#include "TcpPacket.h"

#define RECEIVE_BUFFER_SIZE 2048

struct LastPacket {
  unsigned long long int timestamp;
  unsigned int packetNumber;
};

LastPacket *newLastPacket(unsigned long long int timestamp, unsigned int packetNumber) {
  LastPacket *lastPacket = (LastPacket *) malloc(sizeof(LastPacket));
  lastPacket->timestamp = timestamp;
  lastPacket->packetNumber = packetNumber;
  return lastPacket;
}

bool comparator(char* lhs, char* rhs) {
  return atoi(TcpPacket::getBytes(lhs, 0, SEQUENCE_SIZE)) > atoi(TcpPacket::getBytes(rhs, 0, SEQUENCE_SIZE));
}

LastPacket *processStream(char *stream) {
  std::ofstream fout;
  fout.open("received.mp3", std::ios::app | std::ios::binary);
  int sizeOfData = atoi(TcpPacket::getBytes(stream, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE + CHECKSUM_SIZE + TIMESTAMP_SIZE, DATA_SIZE_SIZE));
  printf("Writing %d bytes of data to file\n", sizeOfData);
  char* content = TcpPacket::getBytes(stream, HEADER_SIZE, sizeOfData);
  fout.write(content, sizeOfData);
  free(content);
  fout.close();
  return newLastPacket(TcpPacket::getTimeStamp(stream), atoi(TcpPacket::getBytes(stream, 0, SEQUENCE_SIZE)));
}

int main(int argc, char **argv) {
  struct sockaddr_in myaddr;  /* our address */
  struct sockaddr_in remaddr; /* remote address */
  socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
  int recvlen;            /* # bytes received */
  int fd;             /* our socket */
  int msgcnt = 0;         /* count # of messages we received */
  char *buf; /* receive buffer */
  std::ofstream myfile;
  myfile.open("write.txt");

  if (remove("received.mp3") != 0) {
    perror("Error in deleting the file\n");
  }

  //Initialise winsock  
  Network();

  /* create a UDP socket */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    perror("cannot create socket\n");
    return 0;
  }

  /* bind the socket to any valid IP address and a specific port */
  memset((char *) &myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(SERVICE_PORT);

  if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("bind failed");
    return 0;
  }

  int sendSeqNo = rand() % 1000;
  std::vector<char*> receiveBuffer;
  int nextExpectedSeqno = 0;
  int packetCount = 0;
  unsigned long long int lastGroupedTime = 0;
  bool sendAck = false;
  LastPacket* lastOrderedPacket = nullptr;
  /* now loop, receiving data and printing what we received */
  for (;;) {
    printf("waiting on port %d\n", SERVICE_PORT);
    buf = (char *) malloc(PACKET_SIZE * sizeof(char));
    recvlen = recvfrom(fd, buf, PACKET_SIZE, 0, (struct sockaddr *) &remaddr, &addrlen);
    if (packetCount == DELAY_PARAMETER) {
      sendAck = true;
      packetCount = 0;
    }
    packetCount++;
    // Check the checksum
    char* checksumSent = TcpPacket::getBytes(buf, SEQUENCE_SIZE + ACK_SIZE + FLAG_SIZE + WINDOW_SIZE_SIZE, CHECKSUM_SIZE);
    char* checkZeros = (char *) malloc(CHECKSUM_SIZE * sizeof(char));
    memset(checkZeros, '\0', CHECKSUM_SIZE);
    TcpPacket::setCsum(buf, checkZeros);
    char* checksumRecd = TcpPacket::calculateCsum(buf);
    for (int i = 0; i < CHECKSUM_SIZE; i++) {
      if (*(checksumRecd + i) != *(checksumSent + i)) {
        // Packet received has incorrect checksum, ACK the previous packet again
        bool flags[] = { false, false, false, false, true, false, false, true, false };
        TcpPacket ackPacket(sendSeqNo++, nextExpectedSeqno, flags, (unsigned int) (RECEIVE_BUFFER_SIZE - receiveBuffer.size()), TcpPacket::getTimeStamp(buf), 0, (char*) nullptr);
        if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
          perror("error in sending ackPacket");
          exit(1);
        }
        printf("Checksum incorrect. ACK sent expecting packet sequence number %d\n", nextExpectedSeqno);
        break;
      }
    }

    bool* Aflags = TcpPacket::getFlags(buf);
    myfile << "R " << TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE) << std::endl;

    if (*(Aflags + URGBIT)) {
      printf("Timeout indication received. Clearing out the receiveBuffer\n");
      for (std::vector<char *>::iterator it = receiveBuffer.begin(); it != receiveBuffer.end(); it++) {
        free(*it);
      }
      receiveBuffer.clear();
      myfile << "Q cleared " << receiveBuffer.size() << std::endl;
    }

    /* Check if Packet is a SYN packet */
    if (*(Aflags + SYNBIT)) {
      int recdSeqNo = atoi(TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE));
      nextExpectedSeqno = recdSeqNo + 1;

      printf("SYN msg received with sequence number %d\n", recdSeqNo);
      /* construct SYN-ACK for the packet just received*/
      bool flags[] = { false, false, false, false, true, false, false, true, false };
      TcpPacket ackPacket(sendSeqNo++, nextExpectedSeqno, flags, (unsigned int) RECEIVE_BUFFER_SIZE, TcpPacket::getTimeStamp(buf), 0, nullptr);
      if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
        perror("error in sending ackPacket");
        exit(1);
      }
      free(buf);
      printf("SYN-ACK sent with ACK number %d\n", nextExpectedSeqno);

      buf = (char *) malloc(PACKET_SIZE * sizeof(char));
      recvlen = recvfrom(fd, buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
      /* Check if Packet is a SYN packet */
      // TODO: is it not possible for reordering to happen in such a way in the network that the packet received is actually not a SYN packet but a normal one?
      bool* Aflags = TcpPacket::getFlags(buf);
      if (*(Aflags + ACKBIT)) {
        // ACKBIT will not be set in the packet that is sent by the sender because we have unidirected socket connections. Sending and receiving of packets cannot be bidirectional
        printf("SYN msg received\n");

        int recdAck = atoi(TcpPacket::getBytes(buf, SEQUENCE_SIZE, ACK_SIZE));
        if (recdAck == sendSeqNo) {
          sendSeqNo++;
          printf("3 Way Handshake complete\n");
        } else {
          perror("Ack no. wrong\n");
          exit(1); // TODO: we might want to do something else over here later
        }
      } else {
        perror("ACK bit not properly set\n");
        exit(1); // TODO: we might want to do something else over here later
      }
      free(buf);
      free(Aflags);
    } else if (*(Aflags + FINBIT)) {
      if (receiveBuffer.empty()) {
        bool flags[] = { false, false, false, false, true, false, false, true, false };
        TcpPacket ackPacket(sendSeqNo++, ++nextExpectedSeqno, flags, (unsigned int) (RECEIVE_BUFFER_SIZE - receiveBuffer.size()), TcpPacket::getTimeStamp(buf), 0, nullptr);
        if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
          perror("error in sending finAckPacket");
          exit(1);
        }
        printf("ACK sent for FIN packet with expectation for %d\n", nextExpectedSeqno);
        free(buf);
        free(Aflags);
        break;
      } else {
        receiveBuffer.push_back(buf);
      }
    } else {
      // handle normal packet
      int recdSeqNo = atoi(TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE));
      printf("normal packet received with sequence number %d\n", recdSeqNo);
      if (recdSeqNo >= RECEIVE_BUFFER_SIZE + nextExpectedSeqno) {
        // receive buffer cannot handle so many packets, so we ignore the receipt of this packet
        free(buf);
      } else {
        if (lastOrderedPacket != nullptr && (unsigned int) recdSeqNo <= lastOrderedPacket->packetNumber) {
          if (sendAck) {
            bool flags[] = { false, false, false, false, true, false, false, true, false };
            TcpPacket ackPacket(sendSeqNo++, recdSeqNo + 1, flags, (unsigned int) (RECEIVE_BUFFER_SIZE - receiveBuffer.size()), TcpPacket::getTimeStamp(buf), 0, nullptr);
            if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
              perror("error in sending ackPacket");
              exit(1);
            }
            sendAck = false;
          }
          printf("Fake ACK sent expecting packet sequence number %d\n", recdSeqNo + 1);
          free(buf);
        } else if (recdSeqNo == nextExpectedSeqno) {
          myfile << "W " << TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE) << std::endl;
          free(lastOrderedPacket);
          lastOrderedPacket = processStream(buf);
          free(buf);
          nextExpectedSeqno++;
          while (!receiveBuffer.empty() && atoi(TcpPacket::getBytes(receiveBuffer.back(), 0, SEQUENCE_SIZE)) <= nextExpectedSeqno) {
            if (atoi(TcpPacket::getBytes(receiveBuffer.back(), 0, SEQUENCE_SIZE)) == nextExpectedSeqno) {
              if (*(TcpPacket::getFlags(receiveBuffer.back()) + FINBIT)) {
                bool flags[] = { false, false, false, false, true, false, false, true, false };
                TcpPacket ackPacket(sendSeqNo++, nextExpectedSeqno, flags, (unsigned int) (RECEIVE_BUFFER_SIZE - receiveBuffer.size()), lastOrderedPacket->timestamp, 0, nullptr);
                if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *) &remaddr, addrlen) == -1) {
                  perror("error in sending finAckPacket");
                  exit(1);
                }
                printf("ACK sent for FIN packet with expectation for %d\n", nextExpectedSeqno);
                free(buf);
                free(Aflags);
                break;
              }
              free(lastOrderedPacket);
              lastOrderedPacket = processStream(receiveBuffer.back());
              myfile << "W " << TcpPacket::getBytes(receiveBuffer.back(), 0, SEQUENCE_SIZE) << std::endl;
              nextExpectedSeqno++;
            }
            free(receiveBuffer.back());
            receiveBuffer.pop_back();
            myfile << "Q " << receiveBuffer.size() << std::endl;
            if (!receiveBuffer.empty()) {
              printf("Loop: Top of queue has packet number %d\n", atoi(TcpPacket::getBytes(receiveBuffer.back(), 0, SEQUENCE_SIZE)));
            }
          }
        } else {
          printf("Pushing packet number %d into queue\n", recdSeqNo);
          receiveBuffer.push_back(buf);
          sort(receiveBuffer.begin(), receiveBuffer.end(), comparator);
          myfile << "Q " << receiveBuffer.size() << std::endl;
          printf("Push: Top of queue has packet number %d\n", atoi(TcpPacket::getBytes(receiveBuffer.back(), 0, SEQUENCE_SIZE)));
        }

        // Generate ACK for received packet
        if (sendAck) {
          bool flags[] = { false, false, false, false, true, false, false, true, false };
          TcpPacket ackPacket(sendSeqNo++, nextExpectedSeqno, flags, (unsigned int) (RECEIVE_BUFFER_SIZE - receiveBuffer.size()), lastOrderedPacket->timestamp, 0, nullptr);
          if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
            perror("error in sending ackPacket");
            exit(1);
          }
          sendAck = false;
        }
        printf("ACK sent expecting packet sequence number %d\n", nextExpectedSeqno);
        free(Aflags);
      }
    }
  }
  myfile.close();
  /* never exits */
}