/*
 ** client.c -- a stream socket client demo
 */

#include "538_utils.h"
#include "debug.h"
#include <sys/time.h>

struct in6_addr allocateRemoteMem(int sockfd, struct sockaddr_in6 * targetIP);
int writeRemoteMem(int sockfd, struct sockaddr_in6 * targetIP, char * payload,  struct in6_addr * toPointer);
int freeRemoteMem(int sockfd, struct sockaddr_in6 * targetIP,  struct in6_addr * toPointer);
char * getRemoteMem(int sockfd, struct sockaddr_in6 * targetIP, struct in6_addr * toPointer);
int migrateRemoteMem(int sockfd, struct sockaddr_in6 * targetIP, struct in6_addr * toPointer, int machineID);
