#pragma once 

#include <pcre.h>
#include <pthread.h>

#define TYPE_LASTVALUE      0
#define TYPE_COUNT          1
#define TYPE_RPS            2
#define TYPE_SUM            3

#define T_STRING    0
#define T_INT       1
#define T_FLOAT     2

#define PT_FILE 0
#define PT_CMD 1 

#define BUFF_SIZE 4096

struct metric {
    char name[128];             /* variable name */
    int type;                   /* variable type */
    char pattern[256];          /* regexp for input filtering */
    pcre *re;
    pcre_extra *re_extra;
    unsigned interval;          /* time window for counting rps */
    void *acc;                  /* accumulator (for counting rps etc) */
    void *result;               /* here stores current value */
    time_t last_updated;
};

struct parser {
    char source[1024]; 
    int type;
    pthread_t thread_id;
    struct metric *metrics;
    unsigned metrics_count;
};

struct daemon_config {
    unsigned port;
    unsigned daemonize;
    unsigned parsers_count;
    struct parser *parsers;
};

struct daemon_config parse_config(char *config_filename);
