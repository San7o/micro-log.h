// SPDX-License-Identifier: MIT

#define MICRO_LOG_IMPLEMENTATION
#define MICRO_LOG_MULTITHREADED
#include "micro-log.h"

#include <stdio.h>

int main(void)
{
  micro_log_init(MICRO_LOG_FLAG_LEVEL 
                 | MICRO_LOG_FLAG_DATE  
                 | MICRO_LOG_FLAG_TIME  
                 | MICRO_LOG_FLAG_PID   
                 | MICRO_LOG_FLAG_TID   
                 // | MICRO_LOG_FLAG_JSON  
                 // | MICRO_LOG_FLAG_COLOR 
                 | MICRO_LOG_FLAG_FILE  
                 | MICRO_LOG_FLAG_LINE);

  micro_log_set_file("out");

  int x = 69;
  const char * say_it = "WORKS";
  
  micro_log_trace("x = %d", x);
  micro_log_debug("IT, JUST, %s", say_it);
  micro_log_info("I’d just like to interject for a moment");
  micro_log_warn("Memento mori");
  micro_log_error("This is wrong and you should be ashamed");
  micro_log_fatal("Segmentation fault (core dumped) ((just kidding hahahah))");
  
  micro_log_close();  
  return 0;
}
