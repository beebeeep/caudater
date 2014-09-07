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
#define PT_HTTP 2 

#define BUFF_SIZE 4096

struct metric {
    char *name;                 /* variable name */
    int type;                   /* variable type */
    char *pattern;              /* regexp for input filtering */
    char *ignore_pattern;       /* regexp to filter out output of previous regexp */
    pcre *re;
    pcre_extra *re_extra;
    pcre *ignore_re;
    pcre_extra *ignore_re_extra;
    unsigned interval;          /* time window for counting rps */
    void *acc;                  /* accumulator (for counting rps etc) */
    void *result;               /* here stores current value */
    time_t last_updated;
};

struct parser {
    char *source; 
    int type;
    long timeout;
    pthread_t thread_id;
    pthread_attr_t thread_attr;
    struct metric *metrics;
    size_t metrics_count;
};

struct daemon_config {
    unsigned port;
    unsigned daemonize;
    size_t parsers_count;
    int server_listenfd;
    struct parser *parsers;
};

typedef struct moving_avg_t {
  unsigned long *values;
  size_t current;
} moving_avg;

struct http_cb_data {
    char *str;
    struct parser *parser;
};

struct daemon_config parse_config(char *config_filename);
