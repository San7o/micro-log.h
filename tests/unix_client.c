// SPDX-License-Identifier: MIT

#define MICRO_LOG_IMPLEMENTATION
#define MICRO_LOG_SOCKETS
#include "../micro-log.h"

#include <assert.h>

int main(void)
{
  assert(micro_log_init() == MICRO_LOG_OK);

  assert(micro_log_set_socket_unix("/tmp/my-unix-socket") == MICRO_LOG_OK);

  assert(micro_log_set_out(MICRO_LOG_OUT_SOCK_UNIX) == MICRO_LOG_OK);
  
  assert(micro_log_info("Lorem ipsum dolor sit amet") == MICRO_LOG_OK);
  
  assert(micro_log_close() == MICRO_LOG_OK);
  return 0;
}
