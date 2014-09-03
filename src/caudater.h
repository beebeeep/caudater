#pragma once 

#include <pcre.h>
#include <pthread.h>

#define TYPE_LASTVALUE      0
#define TYPE_COUNT          1
#define TYPE_RPS            2
#define TYPE_SUM            3
#define TYPE_AVGCOUNT       4

#define T_STRING    0
#define T_INT       1
#define T_FLOAT     2

#define PT_FILE 0
#define PT_CMD 1 

#define BUFF_SIZE 4096

struct metric {
    char *name;                 /* variable name */
    int type;                   /* variable type */
    char *pattern;              /* regexp for input filtering */
    pcre *re;
    pcre_extra *re_extra;
    unsigned interval;          /* time window for counting rps */
    void *acc;                  /* accumulator (for counting rps etc) */
    void *result;               /* here stores current value */
    time_t last_updated;
};

struct parser {
    char *source; 
    int type;
    pthread_t thread_id;
    pthread_attr_t thread_attr;
    struct metric *metrics;
    unsigned metrics_count;
};

struct daemon_config {
    unsigned port;
    unsigned daemonize;
    unsigned parsers_count;
    int server_listenfd;
    struct parser *parsers;
};

typedef struct moving_avg_t {
  unsigned long *values;
  size_t current;
} moving_avg;

struct daemon_config parse_config(char *config_filename);
