#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../lib/server_lib.h"
#include "../lib/utils.h"

/*
 * Request handler for socket sock_fd
 * TODO: get message format
 */
void handleClientRequests(char *receiveBuffer, struct sockaddr_in6 *targetIP, struct in6_memaddr *remoteAddr) {
    char *splitResponse;
    // Switch on the client command
    if (memcmp(receiveBuffer, ALLOC_CMD,2) == 0) {
        print_debug("******ALLOCATE******");
        allocateMem(targetIP);
    } else if (memcmp(receiveBuffer, WRITE_CMD,2) == 0) {
        splitResponse = receiveBuffer+2;
        print_debug("******WRITE DATA: ");
        if (DEBUG) {
            printNBytes((char *) remoteAddr, IPV6_SIZE);
        }
        writeMem(splitResponse, targetIP, remoteAddr);
    } else if (memcmp(receiveBuffer, GET_CMD,2) == 0) {
        print_debug("******GET DATA: ");
        // printNBytes((char *) ipv6Pointer,IPV6_SIZE);
        getMem(targetIP, remoteAddr);
    } else if (memcmp(receiveBuffer, FREE_CMD,2) == 0) {
        print_debug("******FREE DATA: ");
        if (DEBUG) {
            printNBytes((char *) remoteAddr,IPV6_SIZE);
        }
        freeMem(targetIP, remoteAddr);
    } else {
        printf("Cannot match command!\n");
        if (sendUDPRaw("Hello, world!", 13, targetIP) == -1) {
            perror("ERROR writing to socket");
            exit(1);
        }
    }
}

/*
 * Main workhorse method. Parses command args and does setup.
 * Blocks waiting for connections.
 */
int main(int argc, char *argv[]) {

    struct config myConf = configure_bluebridge(argv[1], 1);

//    struct sockaddr_in6 *temp = init_rcv_socket_old(argv[1]);
    struct sockaddr_in6 *targetIP = init_rcv_socket(&myConf);
    init_send_socket(&myConf);
   // Start waiting for connections
    struct in6_memaddr *remoteAddr = malloc(sizeof(struct in6_memaddr));
    char receiveBuffer[BLOCK_SIZE];
    while (1) {
        //TODO: Error handling (numbytes = -1)
        receiveUDPIPv6Raw(receiveBuffer, BLOCK_SIZE, targetIP, remoteAddr);
        handleClientRequests(receiveBuffer, targetIP, remoteAddr);
    }
    free (remoteAddr);
    close_sockets();
    return 0;
}
