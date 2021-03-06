#define _GNU_SOURCE

#include <stdio.h>            // printf() and sprintf()
#include <stdlib.h>           // free(), alloc, and calloc()
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <net/if.h>           // struct ifreq
#include <linux/if_ether.h>   // ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <netinet/udp.h>      // struct udphdr
#include <sys/mman.h>         // mmap()
#include <sys/epoll.h>        // epoll_wait(), epoll_event
#include <sys/poll.h>         // pollfd, poll
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <linux/filter.h>     // struct sock_fprog 

#include "udp_raw_common.h"
#include "../utils.h"
#include "../config.h"


int set_fanout(int sd_rx) {
    int fanout_group_id = getpid() & 0xffff;
    if (fanout_group_id) {
            // PACKET_FANOUT_LB - round robin
            // PACKET_FANOUT_CPU - send packets to CPU where packet arrived
            int fanout_type = PACKET_FANOUT_CPU; 
            int fanout_arg = (fanout_group_id | (fanout_type << 16));
            int setsockopt_fanout = setsockopt(sd_rx, SOL_PACKET, PACKET_FANOUT, &fanout_arg, sizeof(fanout_arg));
            if (setsockopt_fanout < 0) {
                printf("Can't configure fanout\n");
                return EXIT_FAILURE;
            }
    }
    return EXIT_SUCCESS;
}

// Super shitty hack. Do not try this at home kids.
int set_packet_filter(int sd, char *addr, char *iface, int port) {
    struct sock_fprog filter;
    int i, lineCount = 0;
    char cmd[512];
    FILE* tcpdump_output;
    sprintf(cmd, "tcpdump -i %s dst port %d and ip6 net %s/16 -ddd", iface, ntohs(port), addr); 
    //sprintf(cmd, "tcpdump -i %s ether[56:2] == 0x%02x and ether[38:2] == 0x%02x%02x -ddd",iface, ntohs(port), addr[0],addr[1]);
    printf("Active Filter: %s\n",cmd );
    if ( (tcpdump_output = popen(cmd, "r")) == NULL )
        RETURN_ERROR(-1, "Cannot compile filter using tcpdump.");
    if (fscanf(tcpdump_output, "%d\n", &lineCount) < 1 )
        RETURN_ERROR(-1, "cannot read lineCount.");
    filter.filter = calloc(1, sizeof(struct sock_filter)*lineCount);
    filter.len = lineCount;
    for (i = 0; i < lineCount; i++) {
        if (fscanf(tcpdump_output, "%u %u %u %u\n", (unsigned int *)&(filter.filter[i].code),(unsigned int *) &(filter.filter[i].jt),(unsigned int *) &(filter.filter[i].jf), &(filter.filter[i].k)) < 4 ) {
            free(filter.filter);
            RETURN_ERROR(-1, "fscanf: error in reading");
        }
        setsockopt(sd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter));
    }
    pclose(tcpdump_output);
    free(filter.filter);
    return EXIT_SUCCESS;
}

struct rx_ring setup_packet_mmap(int sd_rx) {

    struct tpacket_req tpacket_req = {
        .tp_block_size = C_BLOCKSIZE,
        .tp_block_nr = RX_RING_BLOCKS,
        .tp_frame_size = C_FRAMESIZE,
        .tp_frame_nr = C_RING_FRAMES * RX_RING_BLOCKS
    };

    uint64_t size = tpacket_req.tp_frame_nr * tpacket_req.tp_frame_size;
    if (setsockopt(sd_rx, SOL_PACKET, PACKET_RX_RING, &tpacket_req, sizeof tpacket_req))
        close(sd_rx),perror("packet_mmap setsockopt"),exit(EXIT_FAILURE);

    void *mapped_memory = NULL;
    mapped_memory = mmap64(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, sd_rx, 0);
    if (MAP_FAILED == mapped_memory) close(sd_rx),perror("packet_mmap failed"),exit(EXIT_FAILURE);

    struct rx_ring ring_rx;
    memset(&ring_rx, 0, sizeof(ring_rx));
    ring_rx.first_tpacket_hdr = mapped_memory;
    ring_rx.mapped_memory_size = size;
    ring_rx.tpacket_req = tpacket_req;

    return ring_rx;
}

int init_epoll(int sd_rx) {
    struct epoll_event event = {
        .events = EPOLLIN,
        .data = {.fd = sd_rx }
    };
    int epoll_fd = epoll_create1(0);
    if (-1 == epoll_fd) perror("epoll_create failed."),exit(EXIT_FAILURE);
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd_rx, &event))
        close(epoll_fd),perror("epoll_ctl failed"),exit(EXIT_FAILURE);
    return epoll_fd;
}

struct pollfd init_poll(int sd_rx) {
    struct pollfd poll_fd;
    memset(&poll_fd, 0, sizeof(poll_fd));
    poll_fd.fd = sd_rx;
    poll_fd.events = POLLIN | POLLERR;
    poll_fd.revents = 0;
    return poll_fd;
}

int setup_rx_socket(struct config *cfg, int my_port, int thread_id, int *my_epoll, struct pollfd *my_poll, struct rx_ring *my_ring) {

    struct sockaddr_ll device;
    char my_addr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &cfg->src_addr, my_addr, INET6_ADDRSTRLEN);
    memset(&device, 0, sizeof(struct sockaddr_ll));
    if ((device.sll_ifindex = if_nametoindex (cfg->interface)) == 0)
        perror ("if_nametoindex() failed to obtain interface index "), exit (EXIT_FAILURE);
    device.sll_family = AF_PACKET;
    device.sll_protocol = htons (ETH_P_ALL);
    printf("Interface name: %s Index: %d\n",cfg->interface, device.sll_ifindex);
    int sd_rx = socket(AF_PACKET, SOCK_RAW|SOCK_NONBLOCK, htons(ETH_P_ALL));
    if (-1 == sd_rx) perror("Could not set socket"), exit(EXIT_FAILURE);
    //int one = 1;
    //setsockopt(sd_rx, SOL_PACKET, PACKET_QDISC_BYPASS, &one, sizeof(one));

    //set_packet_filter(sd_rx, (char*) &cfg->src_addr, cfg->interface, htons((ntohs(my_port) + thread_id)));
    set_packet_filter(sd_rx, my_addr, cfg->interface, htons((ntohs(my_port) + thread_id)));
    if (my_epoll)
        *my_epoll = init_epoll(sd_rx);
    if (my_poll)
        *my_poll = init_poll(sd_rx);
    *my_ring  = setup_packet_mmap(sd_rx);
    if (-1 == bind(sd_rx, (struct sockaddr *)&device, sizeof(device)))
        perror("Could not bind socket."), exit(EXIT_FAILURE);
    return sd_rx;
}

void close_epoll(int epoll_fd, struct rx_ring ring_rx) {
    close(epoll_fd);
    munmap(ring_rx.first_tpacket_hdr, ring_rx.mapped_memory_size);
}

void close_poll(struct rx_ring ring_rx){
    munmap(ring_rx.first_tpacket_hdr, ring_rx.mapped_memory_size);
}


struct tpacket_hdr *get_packet(struct rx_ring *ring_p) {
    return (void *)((char *)ring_p->first_tpacket_hdr + ring_p->tpacket_i * ring_p->tpacket_req.tp_frame_size);
}

void next_packet(struct rx_ring *ring_p) {
   ring_p->tpacket_i = (ring_p->tpacket_i + 1) % ring_p->tpacket_req.tp_frame_nr;
}
