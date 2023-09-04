#include "log.h"

static sig_atomic_t init_state = 0;

volatile static sig_atomic_t flag_signo_1 = 0;
volatile static sig_atomic_t flag_signo_2 = 0;
volatile static sig_atomic_t flag_signo_3 = 0;

volatile static sig_atomic_t logger_enabled = 1;
volatile static sig_atomic_t logger_severity_lvl = MIN;

static pthread_mutex_t logger_file_mutex;
static pthread_mutex_t logger_dump_file_mutex;

static int sig_ch;

void handler_signo_1(int signo, siginfo_t *info, void *other) { // dump_state_handler
    flag_signo_1 = 1;
    sig_ch = info->si_value.sival_int;
}

void handler_signo_2(int signo, siginfo_t *info, void *other) { // log_enable_handler
    flag_signo_2 = 1;
    sig_ch = info->si_value.sival_int;
}

void handler_signo_3(int signo, siginfo_t *info, void *other) { // log_level_handler
    flag_signo_3 = 1;
    sig_ch = info->si_value.sival_int;
}

void dump_state_handler() {
    if (!logger_enabled) {
        return;
    }

    char dump_file_name[64];
    time_t curr_time = time(NULL);
    struct tm *time_info = localtime(&curr_time);
    strftime(dump_file_name, sizeof(dump_file_name), "dump-%Y-%m-%d.log", time_info);

    // open dump file
    FILE *dump_file = fopen(dump_file_name, "a");
    if (dump_file == NULL) {
        fprintf(stderr, "Error opening dump file\n");
        return;
    }

    pthread_mutex_lock(&logger_dump_file_mutex);

    // write current application state to dump file
    fprintf(dump_file, "NO DATA AVAILABLE!\n");

    pthread_mutex_unlock(&logger_file_mutex);

    // write the current state of the application here
    fclose(dump_file);
}

void log_enable_handler() {
    logger_enabled = !logger_enabled;
    if (logger_enabled) {
        log_message(logger_severity_lvl, "Logging disabled");
    } else {
        log_message(logger_severity_lvl, "Logging enabled");
    }
}

void log_level_handler() {
    int sv_lvl = sig_ch;
    if (sv_lvl < MIN || sv_lvl > MAX) {
        fprintf(stderr, "Invalid option\n");
        return;
    }
    logger_severity_lvl = sv_lvl;
    log_message(logger_severity_lvl, "Log level changed");
}

struct tm *datetime_struct() {
    // time_t ltime;
    time_t ltime = time(NULL);
    return localtime(&ltime);
}

int set_log(unsigned int sv, const char *s) {
    if (logger_severity_lvl < sv)
        return 1;

    else if (!logger_enabled) // stop logging and application when log_enabled is false
        return 2;

    FILE *f = fopen(LOG_FILENAME, "a");
    if (f == NULL) {
        perror("Error writing to log file");
        return 1;
    }

    fprintf(f, "%s", s);
    switch (sv) {
        case 1: {
            int hour = datetime_struct()->tm_hour;
            int min = datetime_struct()->tm_min;
            int sec = datetime_struct()->tm_sec;
            fprintf(f, "\nCurrent time: %02d:%02d:%02d %s\n\n",
                    hour < 12 ? hour : hour - 12, min, sec,
                    hour < 12 ? "am" : "pm");
            break;
        }
        case 2: {
            int day = datetime_struct()->tm_mday;
            int month = datetime_struct()->tm_mon + 1;
            int year = datetime_struct()->tm_year + 1900;
            fprintf(f, "\nCurrent date: %02d/%02d/%d\n\n", day, month, year);
            break;
        }
        case 3: {
            fprintf(f, "\nFull timestamp: %s\n", asctime(datetime_struct()));
            break;
        }
        default:
            fprintf(stderr, "Invalid severity level\n");
            break;
    }

    fclose(f);
    return 0;
}

void log_message(enum log_level_t message_level, char *message) {
    if (!logger_enabled) {
        return;
    }

    if (message_level < logger_severity_lvl) {
        return;
    }

    FILE *f = fopen(LOG_FILENAME, "a");
    if (f == NULL) {
        perror("Problem with file");
        return;
    }

    pthread_mutex_lock(&logger_file_mutex);

    // save log message
    fprintf(f, "%s\n", message);

    pthread_mutex_unlock(&logger_file_mutex);

    fclose(f);
}

void *log_loop(void *arg) {
    while (1) {
        if (flag_signo_1) {
            dump_state_handler();
        }
        if (flag_signo_2) {
            log_enable_handler();
        }
        if (flag_signo_3) {
            log_level_handler();
        }
        sleep(1);
    }
    return NULL;
}

int init_logger() {
    if(init_state++ != 0)
        return -1;

    // application code here
    log_message(logger_severity_lvl, "Application started");

    if (pthread_mutex_init(&logger_file_mutex, NULL) != 0) {
        fprintf(stderr, "Error initializing log file mutex\n");
        return -1;
    }

    if (pthread_mutex_init(&logger_dump_file_mutex, NULL) != 0) {
        fprintf(stderr, "Error initializing dump file mutex\n");
        return -1;
    }

    log_message(logger_severity_lvl, "Logger initialized");

    sig_ch = 0;

    struct sigaction act;

    sigset_t set;
    sigfillset(&set);

    act.sa_sigaction = handler_signo_1; // dump_state_handler
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;
    sigaction(35, &act, NULL); // 10

    act.sa_sigaction = handler_signo_2; // log_enable_handler
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR2, &act, NULL); // 12

    act.sa_sigaction = handler_signo_3; // log_level_handler
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGTERM, &act, NULL); // 15

    pthread_t tid;
    pthread_create(&tid, NULL, log_loop, NULL);

    return 0;
}

void close_logger() {
    log_message(logger_severity_lvl, "Logger closed");
    pthread_mutex_destroy(&logger_file_mutex);
    pthread_mutex_destroy(&logger_dump_file_mutex);
}