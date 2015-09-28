#include "Network.h"
#include <WinSock2.h>
#include <iostream>

Network::Network() {
  WSADATA wsa;

  //Initialise winsock
  printf("\nInitialising Winsock...");
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
  {
    printf("Failed. Error Code : %d", WSAGetLastError());
    exit(EXIT_FAILURE);
  }
  printf("Initialised.\n");

}


Network::~Network() {}
