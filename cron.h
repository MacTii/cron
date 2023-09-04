#ifndef CRON_H
#define CRON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <mqueue.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <pthread.h>

#define MQ_SEND_TASKS "/MQ_SEND_TASKS"
#define SHM_SERVER_PID "/SHM_SERVER_PID"

typedef struct server_info {
    pid_t pid;
} server_info_t;

enum operation {
    QUERY_ADD,
    QUERY_DELETE,
    QUERY_LIST,
    QUERY_EXIT
};

typedef struct query {
    long id;
    char command[256];
    char command_args[256];

    int is_absolute;
    time_t time;
    time_t interval;
    enum operation op;
    timer_t timer_id;

    char res_mq_name[50];
} query_t;

void timer_thread(union sigval query_union);
void delete_cron_query(long id);

#endif
