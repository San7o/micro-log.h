//////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
//
// micro-log.h
// -----------
//
// Header-only, configurable, thread safe logging framework in C99.
//
// ```
// 2025-09-21 22:32:36 INFO   | Logger initialized
// 2025-09-21 22:32:36 INFO   | I’d just like to interject for a moment...
// 2025-09-21 22:32:36 INFO   | Closing logger
// ```
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
//  - read settings from file (see the file `settings`)
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
// `micro_log_init`.
//
// ```
// micro_log_init();
// ```
//
// If using `micro_log_init2`, you can specify a pointer to a logger.
//
// Remember to close the logger after you are done with
// `micro_log_close()`.
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
// You can also read some settings from a file. Check out the file
// `settings` for additional information.
//
//
// Code
// ----
//
// The official git repository of micro-log.h is hosted at:
//
//     https://github.com/San7o/micro-log.h
//
// This is part of a bigger collection of header-only C99 libraries
// called "micro-headers", contributions are welcome:
//
//     https://github.com/San7o/micro-headers
//
//
// TODO
// ----
//
// - windows support
//

#ifndef _MICRO_LOG_H_
#define _MICRO_LOG_H_

#define MICRO_LOG_MAJOR 0
#define MICRO_LOG_MINOR 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L // dprintf, vdprintf (UNIX)
#endif
  
//
// Configuration
//
  
// Config: Enable thread safety by defining MICRO_LOG_MULTITHREADED
// Note: This is optional since it adds a (tiny) performance setback
// since printing must be regulated via a mutex.
//
//#define MICRO_LOG_MULTITHREADED

// Config: Enable logging on operating system sockets
// Note: Optional since this may not be needed by most applications
//
//#define MICRO_LOG_SOCKETS

//
// Errors
//

// Do not expect the error numbers to be the same in the future, use
// the macro
#define MICRO_LOG_OK                         0
#define MICRO_LOG_ERROR_LOGGER_NULL          1
#define MICRO_LOG_ERROR_UNIMPLEMENTED        2
#define MICRO_LOG_ERROR_MUTEX_LOCK           3
#define MICRO_LOG_ERROR_MUTEX_UNLOCK         4
#define MICRO_LOG_ERROR_CLOSE_FILE           5
#define MICRO_LOG_ERROR_CLOSE_INET_SOCK      6
#define MICRO_LOG_ERROR_CLOSE_UNIX_SOCK      7
#define MICRO_LOG_ERROR_OPEN_FILE            8
#define MICRO_LOG_ERROR_INET_ADDR_NULL       9
#define MICRO_LOG_ERROR_OPEN_INET_SOCK       10
#define MICRO_LOG_ERROR_INET_CONNECT         11
#define MICRO_LOG_ERROR_PRINTF_SOCK_INET     12
#define MICRO_LOG_ERROR_VPRINTF_SOCK_INET    13
#define MICRO_LOG_ERROR_PRINTF_SOCK_UNIX     14
#define MICRO_LOG_ERROR_VPRINTF_SOCK_UNIX    15
#define MICRO_LOG_ERROR_PRINTF_STDOUT        16
#define MICRO_LOG_ERROR_PRINTF_FILE          17
#define MICRO_LOG_ERROR_VPRINTF_STDOUT       18
#define MICRO_LOG_ERROR_VPRINTF_FILE         19
#define MICRO_LOG_ERROR_INET_ADDR            21
#define MICRO_LOG_ERROR_SOCK_PATH_NULL       22
#define MICRO_LOG_ERROR_OPEN_UNIX_SOCK       23
#define MICRO_LOG_ERROR_UNIX_CONNECT         24
#define MICRO_LOG_ERROR_FLUSH_STDOUT         25
#define MICRO_LOG_ERROR_FLUSH_FILE           26
#define MICRO_LOG_ERROR_FROM_FILE_NULL       27
#define MICRO_LOG_ERROR_INVALID_FILE_SETTING 28
#define MICRO_LOG_ERROR_UNKNOWN_LEVEL        29
#define MICRO_LOG_ERROR_UNKNOWN_FLAG         30
#define MICRO_LOG_ERROR_INVALID_INET_ADDR    31
#define MICRO_LOG_ERROR_INVALID_PORT         32
#define MICRO_LOG_ERROR_INVALID_PROTOCOL     33
#define _MICRO_LOG_ERROR_MAX                 34

//
// Macros
//

#ifdef MICRO_LOG_MULTITHREADED
  #ifdef _WIN32
    #error "TODO: Multithreading support on windows is not yet supported"
  #endif
  #include <pthread.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
  
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
#define _MICRO_LOG_OUT_MAX      (1 << 4)

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

#define micro_log_trace2(micro_log, ...)                          \
  micro_log_write2(micro_log, MICRO_LOG_LEVEL_TRACE, __VA_ARGS__)

#define micro_log_debug2(micro_log, ...)                          \
  micro_log_write2(micro_log, MICRO_LOG_LEVEL_DEBUG, __VA_ARGS__)

#define micro_log_info2(micro_log, ...)                           \
  micro_log_write2(micro_log, MICRO_LOG_LEVEL_INFO, __VA_ARGS__)

#define micro_log_warn2(micro_log, ...)                           \
  micro_log_write2(micro_log, MICRO_LOG_LEVEL_WARN, __VA_ARGS__)

#define micro_log_error2(micro_log, ...)                          \
  micro_log_write2(micro_log, MICRO_LOG_LEVEL_ERROR, __VA_ARGS__)

#define micro_log_fatal2(micro_log, ...)                          \
  micro_log_write2(micro_log, MICRO_LOG_LEVEL_FATAL, __VA_ARGS__)

//
// Types and functions
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
// Global logger functions
//
//
// The Global logger
//
// This is a global logger that can be used with the function below.
// You can also create other loggers, check out the other functions
// in the `Local logger functions` group.
extern MicroLog micro_log_global;
  
// Initialize the global logger
//
// Notes: remember to close it when you are done
micro_log_error micro_log_init(void);

// Read settings from file
//
// Check out the file named `settings` for additional information
// Notes: this expects the global logger to be already initialzied
micro_log_error micro_log_from_file(char *filename);

// Close the global logger
micro_log_error micro_log_close(void);

// Flush all current output streams in the global logger
micro_log_error micro_log_flush(void);

// Set output flags to global logger
//
// These are additional information to the log output, such as the
// date, line, file position etc. You can also set the output to be
// colored or to be serialized in some format like json.
//
// Check out the MICRO_LOG_FLAG_ values to see all the flags available
micro_log_error micro_log_set_flags(long unsigned int flags_bitfield);

// Set the log level to global logger
//
// Only the logs with an higher level of the current log level will be
// printed.
//
// Check out the MicroLogLevel enum for a list of the supported levels.
micro_log_error micro_log_set_level(enum MicroLogLevel level);

// Set the output streams to the global logger
//
// You can toggle some output streams. Check out the MICRO_LOG_OUT_
// values. Remember that if you manually enable the
// MICRO_LOG_OUT_FILE, the logger will try to write to a file so you
// should have already opened it with `miro_log_set_file`. Likewise
// for the other options. By default, when you add a new output
// stream, this will be automatically registered.
micro_log_error micro_log_set_out(int out_flags);

// Set output file of the global logger
//
// The logger will write logs to [filename]. The logger will create
// the file if it does not exist.
micro_log_error micro_log_set_file(char* filename);

#ifdef MICRO_LOG_SOCKETS

// Set output internet socket of the global logger
//
// Note: You need to have defined MICRO_LOG_SOCKETS before including
// this header in torder to use this function
micro_log_error
micro_log_set_socket_inet(char* addr,
                          int port,
                          enum MicroLogProto protocol);

#if defined(__unix__) || defined(__unix)

// Set output unix socket of the global logger
//
// Note: You need to have defined MICRO_LOG_SOCKETS before including
// this header, and be in an unix system to use this function.
micro_log_error micro_log_set_socket_unix(char* path);

#endif // __unix__

#endif // MICRO_LOG_SOCKETS

//
// Local logger functions
//
// These are equivalent to the global ones but they take an additional
// MicroLog first argument, which is the logger what will be modified.

micro_log_error micro_log_init2(MicroLog *micro_log);
// micro_log_from_file2 expects [micro_log] to be already initialzied
micro_log_error
micro_log_from_file2(MicroLog *micro_log, char *filename);
micro_log_error micro_log_close2(MicroLog *micro_log);
micro_log_error micro_log_flush2(MicroLog *micro_log);
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

// Get a string of a certain log level, with an optional color
const char*
micro_log_level_string(enum MicroLogLevel level, bool color);

// The central log function
//
// All other functions call this one for printing. It handles all the
// flags and calls `_micro_log_print_outputs`.
micro_log_error
_micro_log_write_impl(MicroLog *micro_log,
                      enum MicroLogLevel level,
                      const char* file,
                      int line,
                      const char *fmt, ...);

// Wraps `_micro_log_print_outputs_args`
micro_log_error _micro_log_print_outputs(MicroLog *micro_log,
                                         const char* fmt, ...);

// Print formatted string in multiple outputs.
micro_log_error
_micro_log_print_outputs_args(MicroLog *micro_log,
                              const char* fmt,
                              va_list args);
//
// Implementation
//

#ifdef MICRO_LOG_IMPLEMENTATION

#define _GNU_SOURCE
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef MICRO_LOG_SOCKETS
  #ifdef _WIN32
    // Windows
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define close_socket(s) closesocket(s)
    #pragma comment(lib, "ws2_32.lib")  // link against Winsock library
    #define dprintf(...) assert(false); // TODO
    #define vdprintf(...) assert(false); // TODO
    #error "TODO Support for windows internet sockets"
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

micro_log_error micro_log_init(void)
{
  return micro_log_init2(&micro_log_global);
}

micro_log_error micro_log_from_file(char *filename)
{
  return micro_log_from_file2(&micro_log_global, filename);
}

micro_log_error micro_log_close(void)
{
  return micro_log_close2(&micro_log_global);
}

micro_log_error micro_log_flush(void)
{
  return micro_log_flush2(&micro_log_global);
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

micro_log_error micro_log_init2(MicroLog *micro_log)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  *micro_log = (MicroLog){
    .log_level = MICRO_LOG_LEVEL_TRACE,
    .flags_bitfield = 0,
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

int _micro_log_get_spaces(char* str, int max)
{
  int spaces = 0;
  while((*(str + spaces) == ' ' || *(str + spaces) == '\t') && spaces < max)
    spaces++;
  return spaces;
}

int _micro_log_get_word_len(char* str, int max)
{
  int letters = 0;
  while(*(str+letters) != ' '
        && *(str+letters) != '\n'
        && *(str+letters) != '\t'
        && letters < max)
    letters++;
  return letters;
}

micro_log_error
micro_log_from_file2(MicroLog *micro_log, char *filename)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;
  if (filename == NULL)
    return MICRO_LOG_ERROR_FROM_FILE_NULL;

  micro_log_error error = MICRO_LOG_OK;
  
  FILE* file = fopen(filename, "r");
  if (file == NULL)
  {
    perror("Error opening file");
    error = MICRO_LOG_ERROR_OPEN_FILE;
    goto done;
  }

  // Example file:
  //
  // level: debug
  // flags: level date time tid pid
  // file: output.txt
  // inet: 127.0.0.1 5000 TCP
  // unix: /tmp/a-socket
  // # A comment
  //
  char *line = NULL;
  size_t len;
  int spaces;
  while(getline(&line, &len, file) >= 0)
  {
    if (strncmp(line, "#", 1) == 0)
    {
      goto next;
    }
    if (strncmp(line, "\n", 1) == 0)
    {
      goto next;
    }
    if (strncmp(line, "level:", 6) == 0)
    {
      spaces = _micro_log_get_spaces(line + 6, len - 6);
      
      if (strncmp(line + 6 + spaces, "trace", 5) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_TRACE);
      }
      else if (strncmp(line + 6 + spaces, "debug", 5) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_DEBUG);
      }
      else if (strncmp(line + 6 + spaces, "info", 4) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_INFO);
      }
      else if (strncmp(line + 6 + spaces, "warn", 4) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_WARN);
      }
      else if (strncmp(line + 6 + spaces, "error", 5) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_ERROR);
      }
      else if (strncmp(line + 6 + spaces, "fatal", 5) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_FATAL);
      }
      else if (strncmp(line + 6 + spaces, "disabled", 8) == 0)
      {
        micro_log_set_level2(micro_log, MICRO_LOG_LEVEL_DISABLED);
      } else {
        error = MICRO_LOG_ERROR_UNKNOWN_LEVEL;
        free(line);
        goto done;
      }
    }
    else if (strncmp(line, "flags:", 6) == 0)
    {
      int pos = 6;
      long unsigned int flags = 0;
      pos += _micro_log_get_spaces(line + 6, len - 6);
      while (pos <= len)
      {
        if (strncmp(line + pos, "level", 5) == 0)
        {
          flags |= MICRO_LOG_FLAG_LEVEL;
          pos += 5;
        }
        else if (strncmp(line + pos, "date", 4) == 0)
        {
          flags |= MICRO_LOG_FLAG_DATE;
          pos += 4;
        }
        else if (strncmp(line + pos, "time", 4) == 0)
        {
          flags |= MICRO_LOG_FLAG_TIME;
          pos += 4;
        }
        else if (strncmp(line + pos, "pid", 3) == 0)
        {
          flags |= MICRO_LOG_FLAG_PID;
          pos += 3;
        }
        else if (strncmp(line + pos, "tid", 3) == 0)
        {
          flags |= MICRO_LOG_FLAG_TID;
          pos += 3;
        }
        else if (strncmp(line + pos, "json", 4) == 0)
        {
          flags |= MICRO_LOG_FLAG_JSON;
          pos += 4;
        }
        else if (strncmp(line + pos, "color", 5) == 0)
        {
          flags |= MICRO_LOG_FLAG_COLOR;
          pos += 5;
        }
        else if (strncmp(line + pos, "file", 4) == 0)
        {
          flags |= MICRO_LOG_FLAG_FILE;
          pos += 4;
        }
        else if (strncmp(line + pos, "line", 4) == 0)
        {
          flags |= MICRO_LOG_FLAG_LINE;
          pos += 4;
        }
        else {
          error = MICRO_LOG_ERROR_UNKNOWN_FLAG;
          free(line);
          goto done;
        }

        if (*(line + pos) == '\n') break;
        pos += _micro_log_get_spaces(line + pos, len - pos);
      }
      error = micro_log_set_flags2(micro_log, flags);
      if (error != MICRO_LOG_OK) { free(line); goto done; }
    }
    else if (strncmp(line, "file:", 5) == 0)
    {
      int pos = 5;
      pos += _micro_log_get_spaces(line + pos, len - 6);
      int file_str_len = _micro_log_get_word_len(line + pos, len - pos);
      if (pos + file_str_len < len)
      {
        line[pos + file_str_len] = '\0';
      }
      
      error = micro_log_set_file2(micro_log, line + pos);
      if (error != MICRO_LOG_OK) { free(line); goto done; }
    }
    #ifdef MICRO_LOG_SOCKETS
    else if (strncmp(line, "inet:", 5) == 0)
    {
      char* addr;
      int port;
      enum MicroLogProto proto;
      
      int pos = 6;
      pos += _micro_log_get_spaces(line + 6, len - 6);
      int addr_len = _micro_log_get_word_len(line + pos, len - pos);
      if (addr_len == 0)
      {
        error = MICRO_LOG_ERROR_INVALID_INET_ADDR;
        free(line);
        goto done;
      }

      addr = malloc(addr_len + 1);
      strncpy(addr, line + pos, addr_len);
      addr[addr_len] = '\0';
      
      pos += addr_len;

      pos += _micro_log_get_spaces(line + pos, len - pos);
      int port_len = _micro_log_get_word_len(line + pos, len - pos);
      if (port_len == 0)
      {
        error = MICRO_LOG_ERROR_INVALID_PORT;
        free(line); free(addr);
        goto done;
      }

      char *port_str = malloc(port_len + 1);
      strncpy(port_str, line + pos, port_len);
      port_str[port_len] = '\0';

      port = atoi(port_str);
      if (port_len == 0)
      {
        error = MICRO_LOG_ERROR_INVALID_PORT;
        free(port_str); free(line); free(addr);
        goto done;
      }
      
      free(port_str);
      pos += port_len;

      pos += _micro_log_get_spaces(line + pos, len - pos);
      int proto_len = _micro_log_get_word_len(line + pos, len - pos);
      if (proto_len == 0)
      {
        error = MICRO_LOG_ERROR_INVALID_PROTOCOL;
        free(line); free(addr);
        goto done;
      }

      if (strncmp(line + pos, "tcp", 3) == 0)
      {
        proto = MICRO_LOG_PROTO_TCP;
      }
      else if (strncmp(line + pos, "udp", 3) == 0)
      {
        proto = MICRO_LOG_PROTO_UDP;
      }
      else {
        error = MICRO_LOG_ERROR_INVALID_PROTOCOL;
        free(line); free(addr);
        goto done;
      }
      
      error = micro_log_set_socket_inet(addr, port, proto);
      if (error != MICRO_LOG_OK) { free(line); free(addr); goto done; }
      
      free(addr);
    }
    #if defined(__unix__) || defined(__unix)
    else if (strncmp(line, "unix:", 5) == 0)
    {
      spaces = _micro_log_get_spaces(line + 5, len - 6);
      error = micro_log_set_socket_unix2(micro_log, line + 5 + spaces);
      if (error != MICRO_LOG_OK) { free(line); goto done; }
    }
    #endif // __unix__
    #endif // MICRO_LOG_SOCKETS
    else {
      free(line);
      error = MICRO_LOG_ERROR_INVALID_FILE_SETTING;
      goto done;
    }

    next:
    free(line);
    line = NULL;
  }
  
  if (fclose(file) != 0)
  {
    error = MICRO_LOG_ERROR_CLOSE_FILE;
    goto done;
  }
  
 done:

  if (error == MICRO_LOG_OK)
    micro_log_trace("Initialized logger from file \"%s\"", filename);
  return error;
}

micro_log_error micro_log_close2(MicroLog *micro_log)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  micro_log_error error = MICRO_LOG_OK;

  micro_log_info2(micro_log, "Closing logger");
  
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

_Static_assert(_MICRO_LOG_OUT_MAX == (1 << 4),
               "Updated MICRO_LOG_OUT_MAX, maybe should also update micro_log_flush2");
micro_log_error micro_log_flush2(MicroLog *micro_log)
{
  if (micro_log == NULL)
    return MICRO_LOG_ERROR_LOGGER_NULL;

  micro_log_error error = MICRO_LOG_OK;

  if (micro_log->out_bitfield & MICRO_LOG_OUT_STDOUT)
  {
    if (fflush(stdout) != 0)
    {
      perror("Error flushing stdout");
      error = MICRO_LOG_ERROR_FLUSH_STDOUT;
      goto done;
    }
  }
  if (micro_log->out_bitfield & MICRO_LOG_OUT_FILE)
  {
    if (fflush(micro_log->file) != 0)
    {
      perror("Error flushing file");
      error = MICRO_LOG_ERROR_FLUSH_FILE;
      goto done;
    }
  }

 done:
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

  if (level > _MICRO_LOG_LEVEL_MAX)
    return MICRO_LOG_ERROR_UNKNOWN_LEVEL;
  
  micro_log_error error = MICRO_LOG_OK;

  __MICRO_LOG_LOCK(micro_log);
  
  micro_log->log_level = level;

  goto done;
 done:
  __MICRO_LOG_UNLOCK(micro_log);

  if (error == MICRO_LOG_OK)
    micro_log_trace2(micro_log, "Set log level to %s",
                     micro_log_level_string(level, false));
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

  if (error == MICRO_LOG_OK)
    micro_log_trace("Set output file to \"%s\"", filename);
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

  if (error == MICRO_LOG_OK)
    micro_log_trace2(micro_log,
                     "Set output to inet socket at address \"%s\" port %d",
                     addr, port);
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

  if (error == MICRO_LOG_OK)
    micro_log_trace2(micro_log,
                     "Set output to unix socket \"%s\"", path);
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
  }
  else if (micro_log->flags_bitfield > 1)
  {
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

_Static_assert(_MICRO_LOG_OUT_MAX == (1 << 4),
               "Updated MICRO_LOG_OUT, should also update _micro_log_print_outputs_args");
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
  micro_log_init();
  micro_log_set_flags(MICRO_LOG_FLAG_LEVEL 
                      | MICRO_LOG_FLAG_DATE  
                      | MICRO_LOG_FLAG_TIME)
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
