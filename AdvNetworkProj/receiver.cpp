#include <queue>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include "Network.h"
#include "port.h"
#include "TcpPacket.h"

#define RECEIVE_BUFFER_SIZE 2048

using namespace std;

struct comparePacketContents {
  bool operator()(char* lhs, char* rhs) const {
    return atoi(TcpPacket::getBytes(lhs, 0, SEQUENCE_SIZE)) < atoi(TcpPacket::getBytes(rhs, 0, SEQUENCE_SIZE));
  }
};

int main(int argc, char **argv) {
  struct sockaddr_in myaddr;  /* our address */
  struct sockaddr_in remaddr; /* remote address */
  socklen_t addrlen = sizeof(remaddr);        /* length of addresses */
  int recvlen;            /* # bytes received */
  int fd;             /* our socket */
  int msgcnt = 0;         /* count # of messages we received */
  char *buf; /* receive buffer */
  buf = (char *) malloc(PACKET_SIZE * sizeof(char));

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
  priority_queue<char*, vector<char*>, comparePacketContents> receiveBuffer;
  int nextExpectedSeqno;
  /* now loop, receiving data and printing what we received */
  for (;;) {
    printf("waiting on port %d\n", SERVICE_PORT);
    recvlen = recvfrom(fd, buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
    bool* Aflags = TcpPacket::getFlags(buf);

    /* Check if Packet is a SYN packet */
    if (*(Aflags + SYNBIT)) {
      printf("SYN msg received\n");
      int recdSeqNo = atoi(TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE));
      nextExpectedSeqno = recdSeqNo + 1;

      /* construct SYN-ACK for the packet just received*/
      bool flags[] = { false, false, false, false, true, false, false, true, false };
      TcpPacket ackPacket(sendSeqNo++, nextExpectedSeqno, flags, (unsigned int) RECEIVE_BUFFER_SIZE, time(0));
      if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
        perror("error in sending ackPacket");
        exit(1);
      }
      printf("SYN-ACK sent\n");

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
          exit(1); // we might want to do something else over here later
        }
      } else {
        perror("ACK bit not properly set\n");
        exit(1); // we might want to do something else over here later
      }
      free(Aflags);
    } else if (false /* insert condition for FIN here*/) {
      // handle FIN packet and terminate connection
    } else {
      // handle normal packet
      printf("normal packet received\n");
      int recdSeqNo = atoi(TcpPacket::getBytes(buf, 0, SEQUENCE_SIZE));
      if (recdSeqNo >= RECEIVE_BUFFER_SIZE + nextExpectedSeqno) {
        // receive buffer cannot handle so many packets, so we ignore the receipt of this packet
      } else {
        if (recdSeqNo == nextExpectedSeqno) {
          // processStream(buf);
          nextExpectedSeqno++;
          while (atoi(TcpPacket::getBytes(receiveBuffer.top(), 0, SEQUENCE_SIZE)) == nextExpectedSeqno) {
            // processStream(receiveBuffer.pop());
            nextExpectedSeqno++;
          }
        } else {
          receiveBuffer.push(buf);
        }

        // Generate ACK for received packet
        bool flags[] = { false, false, false, false, true, false, false, true, false };
        TcpPacket ackPacket(sendSeqNo++, nextExpectedSeqno, flags, (unsigned int) (RECEIVE_BUFFER_SIZE - receiveBuffer.size()), time(0));
        if (sendto(fd, ackPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, addrlen) == -1) {
          perror("error in sending ackPacket");
          exit(1);
        }
        printf("ACK sent\n");

      }
    }
  }
  /* never exits */
}