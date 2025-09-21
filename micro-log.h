//////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
//
// micro-log.h
// -----------
//
// Header-only, configurable, thread safe logging framework in C99.
//
// Author:  Giovanni Santini
// Mail:    giovanni.santini@proton.me
// License: MIT
//
//
// Features
// --------
//
//  - Multiple logging levels
//  - Log to stdout, files, UNIX sockets, or network sockets
//  - Configurable metadata (level, date, time, pid, tid, etc.)
//  - JSON serialization support
//  - Thread-safe logging
//
// Initial implementation based on Oak: https://github.com/San7o/oak
//
//
// Usage
// -----
//
// Do this:
//
//   #define MICRO_LOG_IMPLEMENTATION
//
// before you include this file in *one* C or C++ file to create the
// implementation.
//
// i.e. it should look like this:
//
//   #include ...
//   #include ...
//   #include ...
//   #define MICRO_LOG_IMPLEMENTATION
//   #include "micro-log.h"
//
// You can tune the library by #defining certain values. See the
// "Config" comments under "Configuration" below.
//
// !!IMPORTANT: READ THIS FOR MULTITHREADING !!
// Multithreading support is optional as it adds a small performance
// setback since printing must be regulated via a mutex.  If you want
// to support logging from multiple threads, then define
// MICRO_LOG_MULTITHREADED before including this file.
//
// (Almost) All function have two versions: one that interacts with a
// global logger, and another that uses a logger instance you
// supply. This is needed because most of the time you just want a
// global logger without the need to pass references to loggers, but
// you may also need different loggers for different parts of your
// application, hence both options are provided.
//
// We will see some examples with the global logger. To use a local
// one, you just need to add a '2' after functions, for example
// `micro_log_error` becomes `micro_log_error2`.
//
// After including the library, you can initialize a logger with
// `micro_log_init`. You can pass some flags that tells the logger to
// write additional metadata to the output, for example:
//
// ```
// micro_log_init(MICRO_LOG_FLAG_LEVEL 
//                | MICRO_LOG_FLAG_DATE  
//                | MICRO_LOG_FLAG_TIME);
// ```
//
// If using `micro_log_init2`, the function will return a new logger.
//
// Remember to close the logger after you are done with
// `micro_log_close();`.
//
// You can set additional settings like the log level with
// `micro_log_set_level` or a file with `micro_log_set_file`, check
// out the function declarations.
//
// To log something, use the macro `micro_log_` with the level you
// want to use, like:
//
// ```
// micro_log_info("I’d like to interject for a moment. %s", text);
// ```
//
// You can format the logs like printf(3).
//
// Check out more examples at the end of the header.
//
//
// TODO
// ----
//
// - refactor code
// - read settings from file
// - log to socket (UNIX)
// - windows support
//

#ifndef _MICRO_LOG_H_
#define _MICRO_LOG_H_

#define MICRO_LOG_MAJOR 0
#define MICRO_LOG_MINOR 1

#ifdef __cplusplus
extern "C" {
#endif

//
// Configuration
//

// Config: Enable thread safety by defining MICRO_LOG_MULTITHREADED
// Note: This is optional since it adds a (tiny) performance setback
// since printing must be regulated via a mutex.
#ifdef MICRO_LOG_MULTITHREADED
  #ifdef _WIN32
    #error "TODO: Multithreading support on windows is not yet supported"
  #endif
#endif

// Config: Enable logging on operating system sockets
// Note: Optional since this may not be needed by most applications
#ifdef MICRO_LOG_SOCKETS
  #ifdef _WIN32
    // Windows
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define close_socket(s) closesocket(s)
    #pragma comment(lib, "ws2_32.lib")  // link against Winsock library
    #error "I don't even pretend this may work on windows, so TODO"
  #else
    // POSIX (Linux, macOS, BSD, etc.)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define close_socket(s) close(s)
#endif
#endif

//
// Macros
//

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#define MICRO_LOG_FLAG_NONE  (0)
#define MICRO_LOG_FLAG_LEVEL (1 << 0)
#define MICRO_LOG_FLAG_DATE  (1 << 1)
#define MICRO_LOG_FLAG_TIME  (1 << 2)
#define MICRO_LOG_FLAG_PID   (1 << 3)
#define MICRO_LOG_FLAG_TID   (1 << 4)
#define MICRO_LOG_FLAG_JSON  (1 << 5)
#define MICRO_LOG_FLAG_COLOR (1 << 6)
#define MICRO_LOG_FLAG_FILE  (1 << 7)
#define MICRO_LOG_FLAG_LINE  (1 << 8)

#define MICRO_LOG_OUT_STDOUT (1 << 0)
#define MICRO_LOG_OUT_FILE   (1 << 1)
#define MICRO_LOG_OUT_SOCKET (1 << 2)

#define MICRO_LOG_RST  "\x1B[0m"
#define MICRO_LOG_FRED(x) "\x1B[31m" x MICRO_LOG_RST
#define MICRO_LOG_FGRN(x) "\x1B[32m" x MICRO_LOG_RST
#define MICRO_LOG_FYEL(x) "\x1B[33m" x MICRO_LOG_RST
#define MICRO_LOG_FBLU(x) "\x1B[34m" x MICRO_LOG_RST
#define MICRO_LOG_FMAG(x) "\x1B[35m" x MICRO_LOG_RST
#define MICRO_LOG_FCYN(x) "\x1B[36m" x MICRO_LOG_RST
#define MICRO_LOG_FWHT(x) "\x1B[37m" x MICRO_LOG_RST
#define MICRO_LOG_BOLD(x) "\x1B[1m" x MICRO_LOG_RST
#define MICRO_LOG_UNDL(x) "\x1B[4m" x MICRO_LOG_RST
#define MICRO_LOG_FLGRAY(x) "\x1B[90m" x MICRO_LOG_RST

// Global logger
    
#define micro_log_write(log_level, ...)                       \
  micro_log_write2(&micro_log_global, log_level, __VA_ARGS__)

#define micro_log_trace(...)                          \
  micro_log_write(MICRO_LOG_LEVEL_TRACE, __VA_ARGS__)

#define micro_log_debug(...)                          \
  micro_log_write(MICRO_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define micro_log_info(...)                           \
  micro_log_write(MICRO_LOG_LEVEL_INFO, __VA_ARGS__)

#define micro_log_warn(...)                           \
  micro_log_write(MICRO_LOG_LEVEL_WARN, __VA_ARGS__)

#define micro_log_error(...)                          \
  micro_log_write(MICRO_LOG_LEVEL_ERROR, __VA_ARGS__)

#define micro_log_fatal(...)                          \
  micro_log_write(MICRO_LOG_LEVEL_FATAL, __VA_ARGS__)


// Local logger
    
#define micro_log_write2(micro_log, log_level, ...)                     \
  _micro_log_write_impl(micro_log, log_level, __FILE__, __LINE__, __VA_ARGS__);

#define micro_log_trace2(micro_log, ...)                                \
  micro_log_local_write(micro_log, MICRO_LOG_LEVEL_TRACE, __VA_ARGS__)

#define micro_log_debug2(micro_log, ...)                                \
  micro_log_local_write(micro_log, MICRO_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define micro_log_info2(micro_log, ...)                               \
  micro_log_local_write(micro_log, MICRO_LOG_LEVEL_INFO, __VA_ARGS__)

#define micro_log_warn2(micro_log, ...)                               \
  micro_log_local_write(micro_log, MICRO_LOG_LEVEL_WARN, __VA_ARGS__)

#define micro_log_error2(micro_log, ...)                                \
  micro_log_local_write(micro_log, MICRO_LOG_LEVEL_ERROR, __VA_ARGS__)

#define micro_log_fatal2(micro_log, ...)                                \
  micro_log_local_write(micro_log, MICRO_LOG_LEVEL_FATAL, __VA_ARGS__)

//
// Types
//

enum MicroLogLevel {
  MICRO_LOG_LEVEL_TRACE = 0,
  MICRO_LOG_LEVEL_DEBUG,
  MICRO_LOG_LEVEL_INFO,
  MICRO_LOG_LEVEL_WARN,
  MICRO_LOG_LEVEL_ERROR,
  MICRO_LOG_LEVEL_FATAL,
  MICRO_LOG_LEVEL_DISABLED,
  _MICRO_LOG_LEVEL_MAX,
};

#ifdef MICRO_LOG_SOCKETS
enum MICRO_LOG_PROTOCOL
{
  MICRO_LOG_PROTO_TCP = 0,
  MICRO_LOG_PROTO_UDP,
  _MICRO_LOG_PROTO_MAX
};
#endif // MICRO_LOG_SOCKETS

// The MicroLog logger
typedef struct {
  // MICRO_LOG_FLAG bitfield
  // Adds additionally log information based on the flags
  long unsigned int flags_bitfield;
  // MICRO_LOG_OUT bitfield
  // Default value is MICRO_LOG_OUT_STDOUT
  long unsigned int out_bitfield;
  // The current log level of the logger
  // Only logs that are of an higher or equal priority than this will
  // be logged.
  // Default value is MICRO_LOG_LEVEL_TRACE
  enum MicroLogLevel log_level;
  // (optional) Pointer to output file
  FILE *file;
  #ifdef MICRO_LOG_SOCKETS
  // (optional) Socket file descriptor
  int sock_fd;
  #endif
  #ifdef MICRO_LOG_MULTITHREADED
  // Mutex to protect all writes
  pthread_mutex_t write_mutex;
  #endif
} MicroLog;

//
// Declarations
//

// Global logger
extern MicroLog micro_log_global;

// The central log function
void _micro_log_write_impl(MicroLog *micro_log,
                           enum MicroLogLevel level,
                           const char* file,
                           int line,
                           const char *fmt, ...);

// Global logger functions

void micro_log_init(long unsigned int flags_bitfield);
void micro_log_close(void);
void micro_log_set_flags(long unsigned int flags_bitfield);
void micro_log_set_level(enum MicroLogLevel level);
void micro_log_set_out(int out_flags);
void micro_log_set_file(char* filename);
#ifdef MICRO_LOG_SOCKETS
void micro_log_set_socket(char* addr,
                          int port,
                          enum MicroLogProtocol protocol);
#endif

// Local logger functions

MicroLog micro_log_init2(long unsigned int flags_bitfield);
void micro_log_close2(MicroLog *micro_log);
void micro_log_set_flags2(MicroLog *micro_log,
                          long unsigned int flags_bitfield);
void micro_log_set_level2(MicroLog *micro_log,
                          enum MicroLogLevel level);
void micro_log_set_out2(MicroLog *micro_log, int out_flags);
void micro_log_set_file2(MicroLog *micro_log,
                         char* filename);
#ifdef MICRO_LOG_SOCKETS
void micro_log_set_socket2(MicroLog *micro_log,
                           char* addr,
                           int port,
                           enum MicroLogProtocol protocol);
#endif

// Misc

const char* micro_log_level_string(enum MicroLogLevel level, bool color);

//
// Implementation
//

#ifdef MICRO_LOG_IMPLEMENTATION

MicroLog micro_log_global;

void micro_log_init(long unsigned int flags_bitfield)
{
  micro_log_global = micro_log_init2(flags_bitfield);
  return;
}

void micro_log_close(void)
{
  micro_log_close2(&micro_log_global);
  return;
}

void micro_log_set_flags(long unsigned int flags_bitfield)
{
  micro_log_set_flags2(&micro_log_global, flags_bitfield);
  return;
}

void micro_log_set_level(enum MicroLogLevel level)
{
  micro_log_set_level2(&micro_log_global, level);
  return;
}

void micro_log_set_out(int out_flags)
{
  micro_log_set_out2(&micro_log_global, out_flags);
  return;
}

void micro_log_set_file(char* filename)
{
  micro_log_set_file2(&micro_log_global, filename);
  return;
}

#ifdef MICRO_LOG_SOCKETS
void micro_log_set_socket(char* addr,
                          int port,
                          enum MicroLogProtocol protocol)
{
  micro_log_set_socket2(&micro_log_global, addr, port, protocol);
  return;
}
#endif

MicroLog micro_log_init2(long unsigned int flags_bitfield)
{
  MicroLog micro_log = {
    .log_level = MICRO_LOG_LEVEL_TRACE,
    .flags_bitfield = flags_bitfield,
    .out_bitfield = MICRO_LOG_OUT_STDOUT,
    .file = NULL,
  };

  #ifdef MICRO_LOG_MULTITHREADED
  pthread_mutex_init(&micro_log.write_mutex, NULL);
  #endif

  micro_log_write2(&micro_log, MICRO_LOG_LEVEL_INFO, "Logger initialized");
  return micro_log;
}

void micro_log_close2(MicroLog *micro_log)
{
  if (micro_log == NULL) return;

  micro_log_write2(micro_log, MICRO_LOG_LEVEL_INFO, "Closing logger");

  if (micro_log->file != NULL)
  {
    if (fclose(micro_log->file) != 0)
      perror("Error closing file");
  }
  
  #ifdef MICRO_LOG_MULTITHREADED
  pthread_mutex_destroy(&micro_log->write_mutex);
  #endif
  
  return;
}

void micro_log_set_flags2(MicroLog *micro_log,
                          long unsigned int flags_bitfield)
{
  if (micro_log == NULL) return;
  micro_log->flags_bitfield = flags_bitfield;
  return;
}

void micro_log_set_level2(MicroLog *micro_log,
                          enum MicroLogLevel level)
{
  if (micro_log == NULL) return;
  micro_log->log_level = level;
  return;
}

void micro_log_set_out2(MicroLog *micro_log, int out_flags)
{
  if (micro_log == NULL) return;
  micro_log->out_bitfield = out_flags;
  return;
}

void micro_log_set_file2(MicroLog *micro_log,
                         char* filename)
{
  if (micro_log == NULL) return;

  if (micro_log->file != NULL)
  {
    if (fclose(micro_log->file) != 0)
      perror("Error closing file");
  }
  
  FILE *file = fopen(filename, "w+");
  if (file == NULL)
  {
    perror("Error opening file");
    return;
  }

  micro_log->out_bitfield |= MICRO_LOG_OUT_FILE;
  micro_log->file = file;
  return;
}

#ifdef MICRO_LOG_SOCKETS
void micro_log_set_socket2(MicroLog *micro_log,
                           char* addr,
                           int port,
                           enum MicroLogProtocol protocol)
{
  // "TODO: Implement micro_log_set_socket2"
  assert(false);
  return;
}
#endif

_Static_assert(_MICRO_LOG_LEVEL_MAX == 7,
               "Updated MICRO_LOG_LEVEL, should also update micro_log_level_string");
const char* micro_log_level_string(enum MicroLogLevel level, bool color)
{
  switch(level)
  {
  case MICRO_LOG_LEVEL_TRACE:
    return color ? MICRO_LOG_FMAG("TRACE") : "TRACE";
  case MICRO_LOG_LEVEL_DEBUG:
    return color ? MICRO_LOG_FGRN("DEBUG") : "DEBUG";
  case MICRO_LOG_LEVEL_INFO:
    return color ? MICRO_LOG_FCYN("INFO") : "INFO";
  case MICRO_LOG_LEVEL_WARN:
    return color ? MICRO_LOG_FYEL("WARN") : "WARN";
  case MICRO_LOG_LEVEL_ERROR:
    return color ? MICRO_LOG_FRED("ERROR") : "ERROR";
  case MICRO_LOG_LEVEL_FATAL:
    return color ? MICRO_LOG_BOLD(MICRO_LOG_FRED("FATAL")) : "FATAL";
  case MICRO_LOG_LEVEL_DISABLED:
    return "DISABLED";
  default:
    break;
  }
  return "UNKNOWN";
}

void _micro_log_write_impl(MicroLog *micro_log,
                           enum MicroLogLevel level,
                           const char* file,
                           int line,
                           const char *fmt, ...)
{
  if (micro_log == NULL) return;
  if (level < micro_log->log_level
      || micro_log->log_level == MICRO_LOG_LEVEL_DISABLED)
    return;
  
  #ifdef MICRO_LOG_MULTITHREADED
  if (pthread_mutex_lock(&micro_log->write_mutex) != 0)
  {
    fprintf(stderr, "Logger error pthread_mutex_lock\n");
    return;
  }
  #endif

#define __COLOR(x) color ? MICRO_LOG_FLGRAY(x) : x
#define __COLOR2(x) color ? MICRO_LOG_BOLD(x) : x
#ifdef MICRO_LOG_SOCKETS
#define __PRINTF_SOCKET(...) assert(false); // TODO
#define __VPRINTF_SOCKET(...) assert(false); // TODO
#else
#define __PRINTF_SOCKET(...) ;
#define __VPRINTF_SOCKET(fmt, args) ;
#endif
#define __PRINTF(...)                                   \
  do {                                                  \
    if (micro_log->out_bitfield & MICRO_LOG_OUT_STDOUT) \
      fprintf(stdout, __VA_ARGS__);                     \
    if (micro_log->out_bitfield & MICRO_LOG_OUT_FILE)   \
      fprintf(micro_log->file, __VA_ARGS__);            \
    if (micro_log->out_bitfield & MICRO_LOG_OUT_SOCKET) \
      fprintf(micro_log->file, __VA_ARGS__);            \
    __PRINTF_SOCKET(__VA_ARGS__);                       \
  } while(0)
#define __VPRINTF(fmt, args)                             \
  do {                                                   \
    if (micro_log->out_bitfield & MICRO_LOG_OUT_STDOUT)  \
    {                                                    \
      va_list copy; va_copy(copy, args);                 \
      vfprintf(stdout, fmt, copy);                       \
      va_end(copy);                                      \
    }                                                    \
    if (micro_log->out_bitfield & MICRO_LOG_OUT_FILE)    \
    {                                                    \
      va_list copy; va_copy(copy, args);                 \
      vfprintf(micro_log->file, fmt, copy);              \
      va_end(copy);                                      \
    }                                                    \
    if (micro_log->out_bitfield & MICRO_LOG_OUT_SOCKET)  \
      __VPRINTF_SOCKET(fmt, args);                       \
  } while(0)

  
  // Handle flags
  bool json = false;
  bool color = false;
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_NONE)
  {
    // Skip other flags
    goto do_print;
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_COLOR)
  {
    color = true;
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_JSON)
  {
    json = true;
    color = false;  // json disables color
    __PRINTF("{ ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_DATE)
  {
    if (json) __PRINTF("\"date\": \"");

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    __PRINTF(__COLOR("%d-%02d-%02d"),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_TIME)
  {
    if (json) __PRINTF("\"time\": \"");

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    __PRINTF(__COLOR("%02d:%02d:%02d"),
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_LEVEL)
  {
    if (json) __PRINTF("\"log_level\": \"");

    __PRINTF("%-5s", micro_log_level_string(level, color));
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_PID)
  {
    if (json) __PRINTF("\"pid\": \"");

    __PRINTF(__COLOR("%d"), getpid());
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_TID)
  {
    if (json) __PRINTF("\"tid\": \"");

    __PRINTF(__COLOR("%ld"), pthread_self());
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_FILE)
  {
    if (json) __PRINTF("\"file\": \"");

    __PRINTF(__COLOR("%s"), file);
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }
  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_LINE)
  {
    if (json) __PRINTF("\"line\": \"");

    __PRINTF(__COLOR("%d"), line);
    
    if (json) __PRINTF("\",");
    __PRINTF(" ");
  }

  if (json)
  {
    __PRINTF("\"log\": \"");
  } else if (micro_log->flags_bitfield > 1) {
    __PRINTF(__COLOR2("| "));
  }
  
 do_print:;
  va_list args;
  va_start(args, fmt); 
  __VPRINTF(fmt, args);
  va_end(args);

  if (json)
    __PRINTF("\" }");
  __PRINTF("\n");
  
  
  #ifdef MICRO_LOG_MULTITHREADED
  if (pthread_mutex_unlock(&micro_log->write_mutex) != 0)
  {
    fprintf(stderr, "Logger error pthread_mutex_unlock\n");
    return;
  }
  #endif
  
#undef __COLOR
#undef __COLOR2
#undef __PRINTF
#undef __VPRINTF
#undef __PRINTF_SOCKET
#undef __VPRINTF_SOCKET
  return;
}

#endif // MICRO_LOG_IMPLEMENTATION

//
// Examples
//

#if 0
#define MICRO_LOG_IMPLEMENTATION
#define MICRO_LOG_MULTITHREADED
#include "micro-log.h"

#include <stdio.h>

int main(void)
{
  micro_log_init(MICRO_LOG_FLAG_LEVEL 
                 | MICRO_LOG_FLAG_DATE  
                 | MICRO_LOG_FLAG_TIME);

  micro_log_set_file("out");
  
  micro_log_info("I’d just like to interject for a moment...");
  
  micro_log_close();  
  return 0;
}

#endif // 0

#ifdef __cplusplus
}
#endif

#endif // _MICRO_LOG_H_
