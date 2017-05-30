#include "./lib/client_lib.h"


#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
const int NUM_ITERATIONS = 10000;

/////////////////////////////////// TO DOs ////////////////////////////////////
//	1. Check correctness of pointer on server side, it should never segfault.
//		(Ignore illegal operations)
//		-> Maintain list of allocated points
//		-> Should be very efficient
//		-> Judy array insert and delete or hashtable?
//	2. Implement userfaultd on the client side
//  3. We have nasty memory leaks that are extremely low level ()
//	4. Implement IP subnet state awareness
//		(server allocates memory address related to its assignment)
//	5. Remove unneeded code and print statements
//		Move all buffers to stack instead of heap.
//		Check memory leaks
//	8. Integrate Mihir's asynchronous code and use raw linux threading:
//		http://nullprogram.com/blog/2015/05/15/
///////////////////////////////////////////////////////////////////////////////

//To add the current correct route
//sudo ip -6 route add local ::3131:0:0:0:0/64  dev lo
//ovs-ofctl add-flow s1 dl_type=0x86DD,ipv6_dest=0:0:01ff:0:ffff:ffff:0:0,actions=output:2
struct LinkedPointer {
	struct in6_addr AddrString;
	struct LinkedPointer * Pointer;
};



void basicOperations( int sockfd, struct addrinfo * p) {
	uint64_t *alloc_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(alloc_latency);
    memset(alloc_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);

    uint64_t *read_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(read_latency);
    memset(read_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);

    uint64_t *write_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(write_latency);
    memset(write_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);

    uint64_t *free_latency = malloc(sizeof(uint64_t) * NUM_ITERATIONS);
    assert(free_latency);
    memset(free_latency, 0, sizeof(uint64_t) * NUM_ITERATIONS);
	int i;
	// Initialize remotePointers array
	struct LinkedPointer * rootPointer = (struct LinkedPointer *) malloc( sizeof(struct LinkedPointer));
	struct LinkedPointer * nextPointer = rootPointer;
	//init the root element
	nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));
	nextPointer->AddrString = allocateRemoteMem(sockfd, p);
	srand(time(NULL));
	for (i = 0; i < NUM_ITERATIONS; i++) {
		nextPointer = nextPointer->Pointer;
		nextPointer->Pointer = (struct LinkedPointer * ) malloc( sizeof(struct LinkedPointer));

		uint64_t start = getns();
		nextPointer->AddrString = allocateRemoteMem(sockfd, p);
		alloc_latency[i] = getns() - start;
	}
	// don't point to garbage
	// temp fix
	nextPointer->Pointer = NULL;
	
	i = 1;
	srand(time(NULL));
	nextPointer = rootPointer;
	while(nextPointer != NULL)	{

		print_debug("Iteration %d", i);
		struct in6_addr remoteMemory = nextPointer->AddrString;
		print_debug("Using Pointer: %p", (void *) getPointerFromIPv6(nextPointer->AddrString));
		print_debug("Creating payload");
		unsigned char * payload = gen_rdm_bytestream(BLOCK_SIZE);

		uint64_t wStart = getns();
		writeRemoteMem(sockfd, p, (char *) payload, &remoteMemory);
		write_latency[i - 1] = getns() - wStart;

		uint64_t rStart = getns();
		char * test = getRemoteMem(sockfd, p, &remoteMemory);
		read_latency[i - 1] = getns() - rStart;

		print_debug("Results of memory store: %.50s", test);
		
		uint64_t fStart = getns();
		freeRemoteMem(sockfd, p, &remoteMemory);
		free_latency[i-1] = getns() - fStart;
		free(payload);
		free(test);
		nextPointer = nextPointer->Pointer;
		i++;
	}
	nextPointer = rootPointer;
	while (nextPointer != NULL) {
		rootPointer = nextPointer; 
	    nextPointer = nextPointer->Pointer;
	    free (rootPointer);
	}
	//print_times(alloc_latency, read_latency, write_latency, free_latency, num_iters);

	free(alloc_latency);
	free(write_latency);
	free(read_latency);
	free(free_latency);
}
//TODO: Remove?
struct PointerMap {
	struct in6_addr Pointer;
};


/*
 * Interactive structure for debugging purposes
 */
void interactiveMode( int sockfd,  struct addrinfo * p) {
	long unsigned int len = 200;
	char input[len];
	char * localData;
	int count = 0;
	int i;
	struct in6_addr remotePointers[100];
	char * lazyZero = calloc(IPV6_SIZE, sizeof(char));
	char s[INET6_ADDRSTRLEN];
	
	// Initialize remotePointers array
	for (i = 0; i < 100; i++) {
		memset(remotePointers[i].s6_addr,0,IPV6_SIZE);
	}
	
	int active = 1;
	while (active) {
		srand(time(NULL));
		memset(input, 0, len);
		getLine("Please specify if you would like to (L)ist, (A)llocate, (F)ree, (W)rite, or (R)equest data.\nPress Q to quit the program.\n", input, sizeof(input));
		if (strcmp("A", input) == 0) {
			memset(input, 0, len);
			getLine("Specify a number of address or press enter for one.\n", input, sizeof(input));

			if (strcmp("", input) == 0) {
				printf("Calling allocateRemoteMem\n");				
				struct in6_addr remoteMemory = allocateRemoteMem(sockfd, p);
				inet_ntop(p->ai_family,(struct sockaddr *) &remoteMemory.s6_addr, s, sizeof s);
				printf("Got this pointer from call%s\n", s);
				memcpy(&remotePointers[count++], &remoteMemory, sizeof(remoteMemory));
			} else {
				int num = atoi(input);
				printf("Received %d as input\n", num);
				int j; 
				for (j = 0; j < num; j++) {
					printf("Calling allocateRemoteMem\n");
					struct in6_addr remoteMemory = allocateRemoteMem(sockfd, p);					
					inet_ntop(p->ai_family,(struct sockaddr *) &remoteMemory.s6_addr, s, sizeof s);
					printf("Got this pointer from call%s\n", s);
					printf("Creating pointer to remote memory address\n");
					memcpy(&remotePointers[count++], &remoteMemory, sizeof(remoteMemory));
				}
			}
		} else if (strcmp("L", input) == 0){
			printf("Remote Address Pointer\n");
			for (i = 0; i < 100; i++){
				if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
					inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
					printf("%s\n", s);
				}
			}
		} else if (strcmp("R", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to read a custom memory address. A to read all pointers.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
						printf("Using pointer %s to read\n", s);
						localData = getRemoteMem(sockfd, p, &remotePointers[i]);
						printf("Retrieved Data (first 80 bytes): %.*s\n", 80, localData);
					}
				}
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) &pointer.s6_addr, s, sizeof s);
				printf("Reading from this pointer %s\n", s);
				localData = getRemoteMem(sockfd, p, &pointer);
				printf(ANSI_COLOR_CYAN "Retrieved Data (first 80 bytes):\n");
				printf("****************************************\n");
				printf("\t%.*s\t\n",80, localData);
				printf("****************************************\n");
				printf(ANSI_COLOR_RESET);
			}
		} else if (strcmp("W", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to write to a custom memory address. A to write to all pointers.\n", input, sizeof(input));

			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);					
						printf("Writing to pointer %s\n", s);
						memset(input, 0, len);
						getLine("Please enter your data:\n", input, sizeof(input));
						if (strcmp("", input) == 0) {
							printf("Writing random bytes\n");
							unsigned char * payload = gen_rdm_bytestream(BLOCK_SIZE);
							writeRemoteMem(sockfd, p, (char*) payload, &remotePointers[i]);
						} else {
							printf(ANSI_COLOR_MAGENTA "Writing:\n");
							printf("****************************************\n");
							printf("\t%.*s\t\n",80, input);
							printf("****************************************\n");
							printf(ANSI_COLOR_RESET);
							writeRemoteMem(sockfd, p, input, &remotePointers[i]);	
						}
					}
				}
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) pointer.s6_addr, s, sizeof s);
				printf("Writing to pointer %s\n", s);
				memset(input, 0, len);
				getLine("Please enter your data:\n", input, sizeof(input));
				if (strcmp("", input) == 0) {
					printf("Writing random bytes\n");
					unsigned char * payload = gen_rdm_bytestream(BLOCK_SIZE);
					writeRemoteMem(sockfd, p, (char*) payload, &remotePointers[i]);
				} else {
					printf("Writing: %s\n", input);
					writeRemoteMem(sockfd, p, input, &remotePointers[i]);	
				}
			}
		} else if (strcmp("M", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to free a custom memory address. A to migrate all pointers.\n", input, sizeof(input));
			
			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);					
						memset(input, 0, len);
						printf("Migrating pointer %s\n", s);
						getLine("Please enter your migration machine:\n", input, sizeof(input));
						if (atoi(input) <= NUM_HOSTS) {
							printf("Migrating\n");
							migrateRemoteMem(sockfd, p, &remotePointers[i], atoi(input));
						} else {
							printf("FAILED\n");	
						}
					}
				}
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) pointer.s6_addr, s, sizeof s);
				printf("Migrating pointer %s\n", s);
				memset(input, 0, len);
				getLine("Please enter your migration machine:\n", input, sizeof(input));
				if (atoi(input) <= NUM_HOSTS) {
					printf("Migrating\n");
					migrateRemoteMem(sockfd, p, &pointer, atoi(input));
				} else {
					printf("FAILED\n");	
				}
			}
		} else if (strcmp("F", input) == 0) {
			memset(input, 0, len);
			getLine("Enter C to free a custom memory address. A to free all pointers.\n", input, sizeof(input));
			
			if (strcmp("A", input) == 0) {
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						inet_ntop(p->ai_family,(struct sockaddr *) &remotePointers[i].s6_addr, s, sizeof s);
						printf("Freeing pointer %s\n", s);
						freeRemoteMem(sockfd, p, &remotePointers[i]);
						memset(remotePointers[i].s6_addr,0, IPV6_SIZE);
					}
				}	
			} else if (strcmp("C", input) == 0) {
				memset(input, 0, len);
				getLine("Please specify the target pointer:\n", input, sizeof(input));
				struct in6_addr pointer;
				inet_pton(AF_INET6, input, &pointer);
				inet_ntop(p->ai_family,(struct sockaddr *) &pointer.s6_addr, s, sizeof s);
				printf("Freeing pointer%s\n", s);				freeRemoteMem(sockfd, p, &pointer);
				for (i = 0; i < 100; i++) {
					if (memcmp(&remotePointers[i].s6_addr, lazyZero, IPV6_SIZE) != 0) {
						memset(remotePointers[i].s6_addr,0, IPV6_SIZE);
					}
				}
			}
		} else if (strcmp("Q", input) == 0) {
			active = 0;
			printf("Ende Gelaende\n");
		} else {
			printf("Try again.\n");
		}
	}
	free(lazyZero);
}

/*
 * Main workhorse method. Parses arguments, setups connections
 * Allows user to issue commands on the command line.
 */
int main(int argc, char *argv[]) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p = NULL;
	int rv;
	
	//specify interactive or automatic client mode
	int isAutoMode = 1;
	//Socket operator variables
	
	int c;
  	opterr = 0;
	while ((c = getopt (argc, argv, ":i")) != -1) {
	switch (c)
	  {
	  case 'i':
	    isAutoMode = 0;
	    break;
	  case '?':
	      fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	    return 1;
	  default:
	    abort ();
	  }
	}

	if (argc <= 2) {
		printf("Defaulting to standard values...\n");
		argv[1] = "::1";
		argv[2] = "5000";
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(NULL, "0", &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

/*	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("client: socket");
			continue;
		}
		const int on=1;
		setsockopt(sockfd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on));
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
		break;
	}*/
	p = bindSocket(p, servinfo, &sockfd);
	struct sockaddr_in6 *temp = (struct sockaddr_in6 *) p->ai_addr;
	temp->sin6_port = htons(strtol(argv[2], (char **)NULL, 10));
	if(isAutoMode) {
		basicOperations(sockfd, p);
	} else {
		interactiveMode(sockfd, p);
	}
	printf(ANSI_COLOR_RED "Finished\n");
	printf(ANSI_COLOR_RESET);
	freeaddrinfo(servinfo); // all done with this structure

	// TODO: send close message so the server exits
	close(sockfd);

	return 0;
}