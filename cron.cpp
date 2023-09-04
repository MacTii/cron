#include "cron.h"
#include "log.h"
#include <vector>

static pthread_mutex_t query_mutex;
std::vector<query_t *> query_vector;

void timer_thread(union sigval query_union) {
    pthread_mutex_lock(&query_mutex);
    query_t *query = (query_t *) query_union.sival_ptr;

    pid_t pid;
    char *argv[] = {query->command, query->command_args, nullptr};
    posix_spawn(&pid, query->command, nullptr, nullptr, argv, environ);
    pthread_mutex_unlock(&query_mutex);
    if (query->interval == 0)
        delete_cron_query(query->id);
}

void delete_cron_query(long id) {
    pthread_mutex_lock(&query_mutex);
    for (int i = 0; i < query_vector.size(); i++) {
        timer_delete(query_vector.at(i)->timer_id);
        query_vector.erase(query_vector.begin() + i);
        break;
    }
    pthread_mutex_unlock(&query_mutex);
}

int main(int argc, char **argv) {
    int shm_server_fd = shm_open(SHM_SERVER_PID, O_RDONLY, 0777);
    server_info_t *shm_server_info = (server_info_t *) mmap(NULL, sizeof(server_info_t), PROT_READ, MAP_SHARED,
                                                            shm_server_fd, 0);

    if (shm_server_fd != -1 && shm_server_info != MAP_FAILED && kill(shm_server_info->pid, 0) == 0) {
        // server is running
        mqd_t mqd_server_write = mq_open(MQ_SEND_TASKS, O_WRONLY);

        if (argc > 1 && mqd_server_write != -1) {
            query_t query = {0};
            if (strcmp(argv[1], "add") == 0) {

                query.op = QUERY_ADD;
                if (strcmp(argv[2], "rel") == 0) {

                    if (argc >= 4) {
                        sscanf(argv[3], "%ld", &query.time);

                        int first_i = 4;
                        if (strcmp(argv[4], "-r") == 0) {
                            sscanf(argv[5], "%ld", &query.interval);
                            first_i = 6;
                        }

                        strcat(query.command, argv[first_i++]);

                        for (size_t i = first_i; i < argc; i++) {
                            if (i != first_i)
                                strcat(query.command_args, " ");
                            strcat(query.command_args, argv[i]);
                        }

                        mq_send(mqd_server_write, (char *) &query, sizeof(query_t), 1);
                    }
                } else if (strcmp(argv[2], "abs") == 0) {
                    if (argc >= 5) {
                        struct tm abs_time;
                        sscanf(argv[3], "%d%*c%d%*c%d", &abs_time.tm_hour, &abs_time.tm_min, &abs_time.tm_sec);
                        sscanf(argv[4], "%d%*c%d%*c%d", &abs_time.tm_mday, &abs_time.tm_mon, &abs_time.tm_year);

                        abs_time.tm_mon = abs_time.tm_mon - 1;
                        abs_time.tm_year = abs_time.tm_year - 1900;

                        int first_i = 5;
                        if (strcmp(argv[5], "-r") == 0) {
                            sscanf(argv[6], "%ld", &query.interval);
                            first_i = 7;
                        }

                        strcat(query.command, argv[first_i++]);

                        for (size_t i = first_i; i < argc; i++) {
                            if (i != first_i)
                                strcat(query.command_args, " ");
                            strcat(query.command_args, argv[i]);
                        }

                        query.time = mktime(&abs_time);
                        query.is_absolute = TIMER_ABSTIME;
                        mq_send(mqd_server_write, (char *) &query, sizeof(query_t), 1);
                    }
                }
            } else if (strcmp(argv[1], "delete") == 0) {
                if (argc >= 3) {
                    query.op = QUERY_DELETE;
                    sscanf(argv[2], "%ld", &query.id);
                    mq_send(mqd_server_write, (char *) &query, sizeof(query_t), 0);
                }
            } else if (strcmp(argv[1], "show") == 0) {
                query.op = QUERY_LIST;

                struct mq_attr attr = {0};
                attr.mq_maxmsg = 8;
                attr.mq_msgsize = sizeof(query_t);
                attr.mq_flags = 0;

                sprintf(query.res_mq_name, "/MQ_RECEIVE_TASKS_%d", getpid());
                int mqd_read = mq_open(query.res_mq_name, O_RDONLY | O_CREAT, &attr);

                if (mqd_read == -1) {
                    printf("Failed to open response queue");
                } else {
                    mq_send(mqd_server_write, (char *) &query, sizeof(query_t), 1);

                    while (1) {
                        query_t *response = new query_t();
                        mq_receive(mqd_read, (char *) response, sizeof(query_t), NULL);
                        if (response->timer_id == (timer_t) -1)
                            break;

                        printf("ID: %ld\nCommand: %s\nArguments: %s\n\nTimer: %ld", response->id, response->command,
                               response->command_args, (long) response->timer_id);
                    }
                }
                mq_close(mqd_read);
                mq_unlink(query.res_mq_name);
            }
            else if(strcmp(argv[1], "exit") == 0) {
                query.op = QUERY_EXIT;
                mq_send(mqd_server_write, (char *) &query, sizeof(query_t), 1);
            }
        }

        mq_close(mqd_server_write);
        close(shm_server_fd);
        munmap(shm_server_info, sizeof(server_info_t));

        exit(0);
    }

    // server is not running
    if (shm_server_fd != -1) {
        munmap(shm_server_info, sizeof(server_info_t));
        close(shm_server_fd);
        shm_unlink(SHM_SERVER_PID);
    }

    shm_server_fd = shm_open(SHM_SERVER_PID, O_RDWR | O_CREAT, 0777);

    if (shm_server_fd == -1) {
        printf("Failed to open shm with server PID");
        exit(3);
    }

    ftruncate(shm_server_fd, sizeof(server_info_t));
    shm_server_info = (server_info_t *) mmap(NULL, sizeof(server_info_t), PROT_READ | PROT_WRITE, MAP_SHARED,
                                             shm_server_fd, 0);

    if (shm_server_info == MAP_FAILED) {
        printf("Failed to map server pid");
        exit(3);
    }

    struct mq_attr attr = {0};
    attr.mq_maxmsg = 8;
    attr.mq_msgsize = sizeof(query_t);
    attr.mq_flags = 0;
    mqd_t mq_server_read = mq_open(MQ_SEND_TASKS, O_RDONLY | O_CREAT, 0666, &attr);
    if (mq_server_read == -1) {
        printf("Failed to open queue to send tasks");
        exit(4);
    }

    pthread_mutex_init(&query_mutex, NULL);

    shm_server_info->pid = getpid();

    printf("Server started with PID = %d\n", shm_server_info->pid);
    init_logger();
    int server_running = 1;
    long id_counter = 1;
    while (server_running) {
        query_t *query = new query_t();
        mq_receive(mq_server_read, (char *) query, sizeof(query_t), NULL);

        operation op = query->op;
        if (op == QUERY_EXIT) {
            set_log(STANDARD, "QUERY_EXIT\n");
            server_running = 0;
        } else if (op == QUERY_ADD) {
            set_log(STANDARD, "QUERY_ADD\n");

            timer_t timer_id;

            struct sigevent *timer_event = new sigevent();
            timer_event->sigev_notify = SIGEV_THREAD;
            timer_event->sigev_notify_function = timer_thread;
            timer_event->sigev_value.sival_ptr = query;

            timer_create(CLOCK_REALTIME, timer_event, &timer_id);

            struct itimerspec *timer_time = new itimerspec();
            timer_time->it_value.tv_sec = query->time;
            timer_time->it_value.tv_nsec = 0;
            timer_time->it_interval.tv_sec = query->interval;
            timer_time->it_interval.tv_nsec = 0;

            timer_settime(timer_id, query->is_absolute, timer_time, NULL);

            query->timer_id = timer_id;
            query->id = id_counter++;

            pthread_mutex_lock(&query_mutex);
            query_vector.push_back(query);
            pthread_mutex_unlock(&query_mutex);

            set_log(STANDARD, "Task added\n");
        } else if (op == QUERY_DELETE) {
            set_log(STANDARD, "QUERY_DELETE\n");
            delete_cron_query(query->id);
        } else if (op == QUERY_LIST) {
            set_log(STANDARD, "QUERY_LIST\n");

            pthread_mutex_lock(&query_mutex);

            int mqd_write = mq_open(query->res_mq_name, O_WRONLY, 0666, &attr);
            for (auto job : query_vector) {
                mq_send(mqd_write, (char *) job, sizeof(query_t), 1);
            }
            query_t last_query = {0};
            last_query.timer_id = (timer_t) -1;
            mq_send(mqd_write, (char *) &last_query, sizeof(query_t), 1);
            mq_close(mqd_write);
            pthread_mutex_unlock(&query_mutex);
        }
    }
    pthread_mutex_destroy(&query_mutex);
    mq_close(mq_server_read);
    close(shm_server_fd);

    mq_unlink(MQ_SEND_TASKS);
    shm_unlink(SHM_SERVER_PID);

    exit(0);
}