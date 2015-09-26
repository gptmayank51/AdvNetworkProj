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

  /* now let's send the messages */
  unsigned short csum = (unsigned short) 123;
  bool flags[] = { true, false, true, false, true, false, true, false, true };
  TcpPacket tcpPacket(123U, 456u, flags, 2048u, 19238293801ull);
  for (i = 0; i < 1; i++) {
    printf("Sending packet %d to %s port %d\n", i, SERVER, SERVICE_PORT);
    //sprintf_s(buf, tcpPacket.buf, i);
    if (sendto(fd, tcpPacket.buf, PACKET_SIZE, 0, (struct sockaddr *)&remaddr, slen) == -1) {
      perror("sendto");
      exit(1);
    }
    /* now receive an acknowledgement from the server */
    while (true) {
      recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
      if (recvlen >= 0) {
        buf[recvlen] = 0;	/* expect a printable string - terminate it */
        printf("received message: \"%s\"\n", buf);
      }
      Sleep(100);
    }
  }
  closesocket(fd);
  return 0;
}
