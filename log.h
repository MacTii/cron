#ifndef SCR_LOGGER_LOG_H
#define SCR_LOGGER_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <time.h>
//#include <stdatomic.h>
#include <pthread.h>

#define LOG_FILENAME "application.log"

enum log_level_t {MIN = 1, STANDARD, MAX};

void handler_signo_1(int signo, siginfo_t *info, void *other);

void handler_signo_2(int signo, siginfo_t *info, void *other);

void handler_signo_3(int signo, siginfo_t *info, void *other);

void dump_state_handler();

void* log_loop(void* arg);

void log_message(enum log_level_t message_level, char *message);

void log_enable_handler();

int set_log(unsigned int sv, const char *s);

void log_level_handler();

int init_logger();

void close_logger();

#endif //SCR_LOGGER_LOG_H
