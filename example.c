// SPDX-License-Identifier: MIT

#define MICRO_LOG_IMPLEMENTATION
#define MICRO_LOG_MULTITHREADED
#define MICRO_LOG_SOCKETS
#include "micro-log.h"

#include <stdio.h>

int main(void)
{
  // Sample data
  int x = 69;
  const char * say_it = "WORKS";

  assert(micro_log_init() == MICRO_LOG_OK);

  // Read settings from file
  //
  // Please see the example file named `settings` for additional
  // information
  assert(micro_log_from_file("settings") == MICRO_LOG_OK);
  
  assert(micro_log_trace("x = %d", x) == MICRO_LOG_OK);
  assert(micro_log_debug("IT, JUST, %s", say_it) == MICRO_LOG_OK);
  assert(micro_log_info("I’d just like to interject for a moment") == MICRO_LOG_OK);
  assert(micro_log_warn("Memento mori") == MICRO_LOG_OK);
  assert(micro_log_error("This is wrong and you should be ashamed") == MICRO_LOG_OK);
  assert(micro_log_fatal("Segmentation fault (core dumped) ((just kidding hahahah))") == MICRO_LOG_OK);
  
  assert(micro_log_close() == MICRO_LOG_OK);
  return 0;
}
