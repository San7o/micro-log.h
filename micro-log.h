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
//  - Log to stdout, file, UNIX sockets, and network sockets
//  - Configurable metadata (level, date, time, pid, tid, etc.)
//  - JSON serialization support
//  - Thread-safe logging
//  - optional colored output
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
// one, you just need to use the version '2' of the functions and pass
// the logger as the first parameter. For example
// `micro_log_info(...)` becomes `micro_log_info2(MicroLog logger,
// ...)`.
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
// - read settings from file
// - windows support
//

#ifndef _MICRO_LOG_H_
#define _MICRO_LOG_H_

#define MICRO_LOG_MAJOR 0
#define MICRO_LOG_MINOR 1

#ifdef __cplusplus
extern "C" {
#endif

#define _POSIX_C_SOURCE 200809L // dprintf, vdprintf (UNIX)

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
    #define dprintf(...) assert(false); // TODO
    #define vdprintf(...) assert(false); // TODO
  #else
    // POSIX (Linux, macOS, BSD, etc.)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/un.h>
    #define close_socket(s) close(s)
  #endif // _WIN32
#endif // MICRO_LOG_SOCKETS

//
// Errors
//

// Do not expect the error numbers to be the same in the future, use
// the macro
#define MICRO_LOG_OK                      0
#define MICRO_LOG_ERROR_LOGGER_NULL       1
#define MICRO_LOG_ERROR_UNIMPLEMENTED     2
#define MICRO_LOG_ERROR_MUTEX_LOCK        3
#define MICRO_LOG_ERROR_MUTEX_UNLOCK      4
#define MICRO_LOG_ERROR_CLOSE_FILE        5
#define MICRO_LOG_ERROR_CLOSE_INET_SOCK   6
#define MICRO_LOG_ERROR_CLOSE_UNIX_SOCK   7
#define MICRO_LOG_ERROR_OPEN_FILE         8
#define MICRO_LOG_ERROR_INET_ADDR_NULL    9
#define MICRO_LOG_ERROR_OPEN_INET_SOCK    10
#define MICRO_LOG_ERROR_INET_CONNECT      11
#define MICRO_LOG_ERROR_PRINTF_SOCK_INET  12
#define MICRO_LOG_ERROR_VPRINTF_SOCK_INET 13
#define MICRO_LOG_ERROR_PRINTF_SOCK_UNIX  14
#define MICRO_LOG_ERROR_VPRINTF_SOCK_UNIX 15
#define MICRO_LOG_ERROR_PRINTF_STDOUT     16
#define MICRO_LOG_ERROR_PRINTF_FILE       17
#define MICRO_LOG_ERROR_VPRINTF_STDOUT    18
#define MICRO_LOG_ERROR_VPRINTF_FILE      19
#define MICRO_LOG_ERROR_INET_ADDR         21
#define MICRO_LOG_ERROR_SOCK_PATH_NULL    22
#define MICRO_LOG_ERROR_OPEN_UNIX_SOCK    23
#define MICRO_LOG_ERROR_UNIX_CONNECT      24
#define _MICRO_LOG_ERROR_MAX              25
  
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
#include <string.h>

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

#define MICRO_LOG_OUT_STDOUT    (1 << 0)
#define MICRO_LOG_OUT_FILE      (1 << 1)
#ifdef MICRO_LOG_SOCKETS
#define MICRO_LOG_OUT_SOCK_INET (1 << 2)
#if defined(__unix__) || defined(__unix)
#define MICRO_LOG_OUT_SOCK_UNIX (1 << 3)
#endif // __unix__
#endif // MICRO_LOG_SOCKETS

#define MICRO_LOG_RST  "\x1B[0m"
#define MICRO_LOG_RED(x) "\x1B[31m" x MICRO_LOG_RST
#define MICRO_LOG_GRN(x) "\x1B[32m" x MICRO_LOG_RST
#define MICRO_LOG_YEL(x) "\x1B[33m" x MICRO_LOG_RST
#define MICRO_LOG_BLU(x) "\x1B[34m" x MICRO_LOG_RST
#define MICRO_LOG_MAG(x) "\x1B[35m" x MICRO_LOG_RST
#define MICRO_LOG_CYN(x) "\x1B[36m" x MICRO_LOG_RST
#define MICRO_LOG_WHT(x) "\x1B[37m" x MICRO_LOG_RST
#define MICRO_LOG_BOLD(x) "\x1B[1m" x MICRO_LOG_RST
#define MICRO_LOG_UNDL(x) "\x1B[4m" x MICRO_LOG_RST
#define MICRO_LOG_LGRAY(x) "\x1B[90m" x MICRO_LOG_RST

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
  _micro_log_write_impl(micro_log, log_level, __FILE__, __LINE__, __VA_ARGS__)

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
enum MicroLogProto
{
  MICRO_LOG_PROTO_TCP = 0,
  MICRO_LOG_PROTO_UDP,
  _MICRO_LOG_PROTO_MAX
};
#endif // MICRO_LOG_SOCKETS

typedef int micro_log_error;

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
  int inet_sock_fd;
  #if defined(__unix__) || defined(__unix)
  int unix_sock_fd;
  #endif // __unix__
  #endif // MICRO_LOG_SOCKETS
  #ifdef MICRO_LOG_MULTITHREADED
  // Mutex to protect all writes
  pthread_mutex_t write_mutex;
  #endif // MICRO_LOG_MULTITHREADED
} MicroLog;

//
// Declarations
//

// Global logger
extern MicroLog micro_log_global;

// Global logger functions

micro_log_error micro_log_init(long unsigned int flags_bitfield);
micro_log_error micro_log_close(void);
micro_log_error micro_log_set_flags(long unsigned int flags_bitfield);
micro_log_error micro_log_set_level(enum MicroLogLevel level);
micro_log_error micro_log_set_out(int out_flags);
micro_log_error micro_log_set_file(char* filename);
#ifdef MICRO_LOG_SOCKETS
micro_log_error
micro_log_set_socket_inet(char* addr,
                          int port,
                          enum MicroLogProto protocol);
#if defined(__unix__) || defined(__unix)
micro_log_error micro_log_set_socket_unix(char* path);
#endif // __unix__
#endif // MICRO_LOG_SOCKETS

// Local logger functions

micro_log_error micro_log_init2(MicroLog *micro_log,
                                long unsigned int flags_bitfield);
micro_log_error micro_log_close2(MicroLog *micro_log);
micro_log_error micro_log_set_flags2(MicroLog *micro_log,
                          long unsigned int flags_bitfield);
micro_log_error
micro_log_set_level2(MicroLog *micro_log,
                          enum MicroLogLevel level);
micro_log_error micro_log_set_out2(MicroLog *micro_log, int out_flags);
micro_log_error micro_log_set_file2(MicroLog *micro_log,
                         char* filename);
#ifdef MICRO_LOG_SOCKETS
micro_log_error
micro_log_set_socket_inet2(MicroLog *micro_log,
                           char* addr,
                           int port,
                           enum MicroLogProto protocol);
#if defined(__unix__) || defined(__unix)
micro_log_error
micro_log_set_socket_unix2(MicroLog *micro_log,
                           char* path);
#endif // __unix__
#endif // MICRO_LOG_SOCKETS

// Misc

const char* micro_log_level_string(enum MicroLogLevel level, bool color);

// The central log function
micro_log_error
_micro_log_write_impl(MicroLog *micro_log,
                      enum MicroLogLevel level,
                      const char* file,
                      int line,
                      const char *fmt, ...);
// Print formatted string in multiple outputs
micro_log_error _micro_log_print_outputs(MicroLog *micro_log,
                                         const char* fmt, ...);

micro_log_error
_micro_log_print_outputs_args(MicroLog *micro_log,
                              const char* fmt,
                              va_list args);
//
// Implementation
//

#ifdef MICRO_LOG_IMPLEMENTATION

#ifdef MICRO_LOG_MULTITHREADED

#define __MICRO_LOG_LOCK(__micro_log_ptr)                   \
  do {                                                      \
    if (pthread_mutex_lock(&__micro_log_ptr->write_mutex) != 0) \
    {                                                       \
      error = MICRO_LOG_ERROR_MUTEX_LOCK;                   \
      goto done;                                            \
    }                                                       \
  } while (0)
#define __MICRO_LOG_UNLOCK(__micro_log_ptr)                   \
  do {                                                        \
    if (pthread_mutex_unlock(&__micro_log_ptr->write_mutex) != 0) \
    {                                                         \
      error = MICRO_LOG_ERROR_MUTEX_UNLOCK;                   \
      goto done;                                              \
    }                                                         \
  } while(0)

#else
  
#define __MICRO_LOG_LOCK(__micro_log_ptr) ;
#define __MICRO_LOG_UNLOCK(__micro_log_ptr) ;

#endif // MICRO_LOG_MULTITHREADED

MicroLog micro_log_global;

micro_log_error micro_log_init(long unsigned int flags_bitfield)
{
  return micro_log_init2(&micro_log_global, flags_bitfield);
}

micro_log_error micro_log_close(void)
{
  return micro_log_close2(&micro_log_global);
}

micro_log_error micro_log_set_flags(long unsigned int flags_bitfield)
{
  return micro_log_set_flags2(&micro_log_global, flags_bitfield);
}

micro_log_error micro_log_set_level(enum MicroLogLevel level)
{
  return micro_log_set_level2(&micro_log_global, level);
}

micro_log_error micro_log_set_out(int out_flags)
{
  return micro_log_set_out2(&micro_log_global, out_flags);
}

micro_log_error micro_log_set_file(char* filename)
{
  return micro_log_set_file2(&micro_log_global, filename);
}

#ifdef MICRO_LOG_SOCKETS
micro_log_error
micro_log_set_socket_inet(char* addr,
                          int port,
                          enum MicroLogProto protocol)
{
  return micro_log_set_socket_inet2(&micro_log_global, addr, port, protocol);
}

#if defined(__unix__) || defined(__unix)
micro_log_error micro_log_set_socket_unix(char* path)
{
  return micro_log_set_socket_unix2(&micro_log_global, path);
}
#endif // __unix__

#endif // MICRO_LOG_SOCKETS

micro_log_error micro_log_init2(MicroLog *micro_log, long unsigned int flags_bitfield)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;
  
  *micro_log = (MicroLog){
    .log_level = MICRO_LOG_LEVEL_TRACE,
    .flags_bitfield = flags_bitfield,
    .out_bitfield = MICRO_LOG_OUT_STDOUT,
    .file = NULL,
    #ifdef MICRO_LOG_SOCKETS
    .inet_sock_fd = 0,
    #if defined(__unix__) || defined(__unix)
    .unix_sock_fd = 0,
    #endif // __unix__
    #endif // MICRO_LOG_SOCKETS
  };

  #ifdef MICRO_LOG_MULTITHREADED
  pthread_mutex_init(&micro_log->write_mutex, NULL);
  #endif

  micro_log_write2(micro_log, MICRO_LOG_LEVEL_INFO, "Logger initialized");
  return MICRO_LOG_OK;
}

micro_log_error micro_log_close2(MicroLog *micro_log)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  micro_log_error error = MICRO_LOG_OK;

  micro_log_write2(micro_log, MICRO_LOG_LEVEL_INFO, "Closing logger");
  
  __MICRO_LOG_LOCK(micro_log);
  
  if (micro_log->file != NULL)
  {
    if (fclose(micro_log->file) != 0)
    {
      perror("Error closing file");
      error = MICRO_LOG_ERROR_CLOSE_FILE;
      goto done;
    }
  }

  #ifdef MICRO_LOG_SOCKETS
  if (micro_log->inet_sock_fd > 0)
  {
    if (close_socket(micro_log->inet_sock_fd) < 0)
    {
      perror("Error closeing socket");
      error = MICRO_LOG_ERROR_CLOSE_INET_SOCK;
      goto done;
    }
  }
  #if defined(__unix__) || defined(__unix)
  if (micro_log->unix_sock_fd > 0)
  {
    if (close(micro_log->unix_sock_fd) < 0)
    {
      error = MICRO_LOG_ERROR_CLOSE_UNIX_SOCK;
      goto done;
    }
  }
  #endif // __unix__
  #endif // MICRO_LOG_SOCKETS

 done:
  __MICRO_LOG_UNLOCK(micro_log);
  
  #ifdef MICRO_LOG_MULTITHREADED
  pthread_mutex_destroy(&micro_log->write_mutex);
  #endif
  
  return error;
}

micro_log_error
micro_log_set_flags2(MicroLog *micro_log,
                     long unsigned int flags_bitfield)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;
  
  micro_log_error error = MICRO_LOG_OK;
  
  __MICRO_LOG_LOCK(micro_log);
  
  micro_log->flags_bitfield = flags_bitfield;

  goto done;
 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;
}

micro_log_error
micro_log_set_level2(MicroLog *micro_log,
                     enum MicroLogLevel level)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  micro_log_error error = MICRO_LOG_OK;

  __MICRO_LOG_LOCK(micro_log);
  
  micro_log->log_level = level;

  goto done;
 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;
}

micro_log_error micro_log_set_out2(MicroLog *micro_log, int out_flags)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  micro_log_error error = MICRO_LOG_OK;

  __MICRO_LOG_LOCK(micro_log);
  
  micro_log->out_bitfield = out_flags;

  goto done;
 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;
}

micro_log_error micro_log_set_file2(MicroLog *micro_log,
                                    char* filename)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  micro_log_error error = MICRO_LOG_OK;
  
  __MICRO_LOG_LOCK(micro_log);
  
  if (micro_log->file != NULL)
  {
    if (fclose(micro_log->file) != 0)
    {
      perror("Error closing file");
      error = MICRO_LOG_ERROR_CLOSE_FILE;
      goto done;
    }
  }
  
  FILE *file = fopen(filename, "w+");
  if (file == NULL)
  {
    perror("Error opening file");
    error = MICRO_LOG_ERROR_OPEN_FILE;
    goto done;
  }

  micro_log->out_bitfield |= MICRO_LOG_OUT_FILE;
  micro_log->file = file;

 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;
}

#ifdef MICRO_LOG_SOCKETS

_Static_assert(_MICRO_LOG_PROTO_MAX == 2,
               "Updated MicroLogProto, should also update micro_log_set_socket_inet2");
micro_log_error
micro_log_set_socket_inet2(MicroLog *micro_log,
                           char* addr,
                           int port,
                           enum MicroLogProto protocol)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;
  if (addr == NULL)
    return MICRO_LOG_ERROR_INET_ADDR_NULL;

  micro_log_error error = MICRO_LOG_OK;

  __MICRO_LOG_LOCK(micro_log);

  if (micro_log->inet_sock_fd > 0)
  {
    if (close_socket(micro_log->inet_sock_fd) < 0)
    {
      perror("Error closing inet socket");
      error = MICRO_LOG_ERROR_CLOSE_INET_SOCK;
      goto done;
    }
  }

  switch (protocol)
  {
  case MICRO_LOG_PROTO_TCP:
    micro_log->inet_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    break;
  case MICRO_LOG_PROTO_UDP:
    micro_log->inet_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    break;
  default:
    fprintf(stderr, "Inet Protocol unrecognized");
    goto done;
  }
  if (micro_log->inet_sock_fd < 0)
  {
    perror("Error creating socket");
    error = MICRO_LOG_ERROR_OPEN_INET_SOCK;
    goto done;
  }

  struct sockaddr_in sockaddr_in;
  sockaddr_in.sin_family = AF_INET;
  sockaddr_in.sin_port = htons(port);

  if(inet_pton(AF_INET, addr, &sockaddr_in.sin_addr) <= 0)
  {
    perror("Error setting inet socket addr");
    error = MICRO_LOG_ERROR_INET_ADDR;
    goto done;
  } 
  
  if (connect(micro_log->inet_sock_fd, (struct sockaddr *) &sockaddr_in,
              sizeof(sockaddr_in)) < 0)
  {
    perror("Error connecting to inet socket");
    error = MICRO_LOG_ERROR_INET_CONNECT;
    goto done;
  }

  micro_log->out_bitfield |= MICRO_LOG_OUT_SOCK_INET;
  
 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;
}

#if defined(__unix__) || defined(__unix)
micro_log_error
micro_log_set_socket_unix2(MicroLog* micro_log, char* path)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;
  if (path == NULL)
    return MICRO_LOG_ERROR_SOCK_PATH_NULL;

  micro_log_error error = MICRO_LOG_OK;

  __MICRO_LOG_LOCK(micro_log);

  if (micro_log->unix_sock_fd > 0)
  {
    if (close(micro_log->unix_sock_fd) < 0)
    {
      perror("Error closing unix socket");
      error = MICRO_LOG_ERROR_CLOSE_UNIX_SOCK;
      goto done;
    }
  }

  micro_log->unix_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (micro_log->unix_sock_fd < 0)
  {
    perror("Error creating unix socket");
    error = MICRO_LOG_ERROR_OPEN_UNIX_SOCK;
    goto done;
  }

  struct sockaddr_un sockaddr_un;
  memset(&sockaddr_un, 0, sizeof(sockaddr_un));
  sockaddr_un.sun_family = AF_UNIX;
  strncpy(sockaddr_un.sun_path, path, sizeof(sockaddr_un.sun_path) - 1);
  
  if (connect(micro_log->unix_sock_fd, (struct sockaddr *) &sockaddr_un,
              sizeof(sockaddr_un)) < 0)
  {
    perror("Error connecting to unix socket");
    error = MICRO_LOG_ERROR_UNIX_CONNECT;
    goto done;
  }

  micro_log->out_bitfield |= MICRO_LOG_OUT_SOCK_UNIX;

 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;
}
#endif // __unix__

#endif // MICRO_LOG_SOCKETS

_Static_assert(_MICRO_LOG_LEVEL_MAX == 7,
               "Updated MICRO_LOG_LEVEL, should also update micro_log_level_string");
const char* micro_log_level_string(enum MicroLogLevel level, bool color)
{
  switch(level)
  {
  case MICRO_LOG_LEVEL_TRACE:
    return color ? MICRO_LOG_MAG("TRACE") : "TRACE";
  case MICRO_LOG_LEVEL_DEBUG:
    return color ? MICRO_LOG_GRN("DEBUG") : "DEBUG";
  case MICRO_LOG_LEVEL_INFO:
    return color ? MICRO_LOG_CYN("INFO") : "INFO";
  case MICRO_LOG_LEVEL_WARN:
    return color ? MICRO_LOG_YEL("WARN") : "WARN";
  case MICRO_LOG_LEVEL_ERROR:
    return color ? MICRO_LOG_RED("ERROR") : "ERROR";
  case MICRO_LOG_LEVEL_FATAL:
    return color ? MICRO_LOG_BOLD(MICRO_LOG_RED("FATAL")) : "FATAL";
  case MICRO_LOG_LEVEL_DISABLED:
    return "DISABLED";
  default:
    break;
  }
  return "UNKNOWN";
}

micro_log_error
_micro_log_write_impl(MicroLog *micro_log,
                      enum MicroLogLevel level,
                      const char* file,
                      int line,
                      const char *fmt, ...)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;
  if (level < micro_log->log_level
      || micro_log->log_level == MICRO_LOG_LEVEL_DISABLED)
    return MICRO_LOG_OK;

  micro_log_error error = MICRO_LOG_OK;
  
  __MICRO_LOG_LOCK(micro_log);

#define COLOR(x) color ? MICRO_LOG_LGRAY(x) : x
#define COLOR2(x) color ? MICRO_LOG_BOLD(x) : x
#define CHECK_ERROR() if (error != MICRO_LOG_OK) { goto done; }

  // Handle flags
  bool json = false;
  bool color = false;
  
  if (micro_log->flags_bitfield == MICRO_LOG_FLAG_NONE)
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
    error = _micro_log_print_outputs(micro_log, "{ ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_DATE)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"date\": \"");
      CHECK_ERROR();
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    error = _micro_log_print_outputs(micro_log, COLOR("%d-%02d-%02d"),
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_TIME)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"time\": \"");
      CHECK_ERROR();
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    error = _micro_log_print_outputs(micro_log, COLOR("%02d:%02d:%02d"),
                                     tm.tm_hour, tm.tm_min, tm.tm_sec);
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_LEVEL)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"log_level\": \"");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, "%-5s",
                                     micro_log_level_string(level, color));
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_PID)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"pid\": \"");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, COLOR("%d"), getpid());
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }
    
    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_TID)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"tid\": \"");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, COLOR("%ld"), pthread_self());
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }
    
    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_FILE)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"file\": \"");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, COLOR("%s"), file);
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }
    
    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (micro_log->flags_bitfield & MICRO_LOG_FLAG_LINE)
  {
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\"line\": \"");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, COLOR("%d"), line);
    CHECK_ERROR();
    
    if (json)
    {
      error = _micro_log_print_outputs(micro_log, "\",");
      CHECK_ERROR();
    }

    error = _micro_log_print_outputs(micro_log, " ");
    CHECK_ERROR();
  }

  if (json)
  {
    error = _micro_log_print_outputs(micro_log, "\"log\": \"");
    CHECK_ERROR();
  } else if (micro_log->flags_bitfield > 1) {
    error = _micro_log_print_outputs(micro_log, COLOR2("| "));
    CHECK_ERROR();
  }
  
 do_print:;
  va_list args;
  va_start(args, fmt);
  error = _micro_log_print_outputs_args(micro_log, fmt, args);
  if (error != MICRO_LOG_OK) { va_end(args); goto done; }
  va_end(args);

  if (json)
  {
    error = _micro_log_print_outputs(micro_log, "\" }");
    CHECK_ERROR();
  }
  
  error = _micro_log_print_outputs(micro_log, "\n");
  CHECK_ERROR();
  
 done:
  __MICRO_LOG_UNLOCK(micro_log);
  return error;

#undef COLOR
#undef COLOR2
}
  
micro_log_error
_micro_log_print_outputs(MicroLog *micro_log, const char* fmt, ...)
{
  micro_log_error error;
  va_list args;

  va_start(args, fmt);
  error = _micro_log_print_outputs_args(micro_log, fmt, args);
  va_end(args);
  
  return error;
}

micro_log_error
_micro_log_print_outputs_args(MicroLog *micro_log,
                              const char* fmt,
                              va_list args)
{
  micro_log_error error = MICRO_LOG_OK;

  if (micro_log->out_bitfield & MICRO_LOG_OUT_STDOUT)
  {
    va_list copy;
    va_copy(copy, args);
    if (vfprintf(stdout, fmt, copy) < 0)
    {
      error = MICRO_LOG_ERROR_PRINTF_STDOUT;
      va_end(copy);
      goto done;
    }
    va_end(copy);
  }
  if (micro_log->out_bitfield & MICRO_LOG_OUT_FILE)
  {
    va_list copy;
    va_copy(copy, args);
    if (vfprintf(micro_log->file, fmt, copy) < 0)
    {
      error = MICRO_LOG_ERROR_PRINTF_FILE;
      va_end(copy);
      goto done;
    }
    va_end(copy);
  }
  #ifdef MICRO_LOG_SOCKETS
  if (micro_log->out_bitfield & MICRO_LOG_OUT_SOCK_INET)
  {
    va_list copy; va_copy(copy, args);
    if (vdprintf(micro_log->inet_sock_fd, fmt, copy) < 0)
    {
      error = MICRO_LOG_ERROR_VPRINTF_SOCK_INET;
      va_end(copy);
      goto done;
    }
    va_end(copy); 
  }
  #if defined(__unix__) || defined(__unix)
  if (micro_log->out_bitfield & MICRO_LOG_OUT_SOCK_UNIX)
  {
    va_list copy; va_copy(copy, args);
    if (vdprintf(micro_log->unix_sock_fd, fmt, copy) < 0)
    {
      error = MICRO_LOG_ERROR_VPRINTF_SOCK_INET;
      va_end(copy);
      goto done;
    }
    va_end(copy);
  }
  #endif // __unix__
  #endif // MICRO_LOG_SOCKETS

 done:
  return error;  
}

#undef __MICRO_LOG_LOCK
#undef __MICRO_LOG_UNLOCK

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
