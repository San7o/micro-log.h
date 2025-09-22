// SPDX-License-Identifier: MIT

#define MICRO_LOG_IMPLEMENTATION
#define MICRO_LOG_SOCKETS
#include "../micro-log.h"

#include <assert.h>

int main(void)
{
  assert(micro_log_init(MICRO_LOG_FLAG_TIME) == MICRO_LOG_OK);

  assert(micro_log_set_socket_inet("127.0.0.1", 5000, MICRO_LOG_PROTO_TCP) == MICRO_LOG_OK);

  assert(micro_log_set_out(MICRO_LOG_OUT_SOCK_INET) == MICRO_LOG_OK);
  
  assert(micro_log_info("Lorem ipsum dolor sit amet") == MICRO_LOG_OK);
  
  assert(micro_log_close() == MICRO_LOG_OK);
  return 0;
}
