#ifndef THRIFT_COMMON
#define THRIFT_COMMON
struct result {
  uint64_t alloc;
  uint64_t read;
  uint64_t write;
  uint64_t free;
  uint64_t rpc_start;
  uint64_t rpc_end;
};

FILE* generate_file_handle(const char * results_dir, char* method_name, char* operation, int size);
ip6_memaddr_block get_pointer(struct sockaddr_in6 *targetIP, uint64_t size);
void marshall_shmem_ptr(GByteArray **ptr, ip6_memaddr *addr);
void unmarshall_shmem_ptr(ip6_memaddr *result_addr, GByteArray *result_ptr);
void populate_array(uint8_t *array, int array_len, uint8_t start_num, gboolean random);
#endif