#pragma once 

#define TYPE_LASTVALUE      0
#define TYPE_COUNT          1
#define TYPE_RPS            2
#define TYPE_SUMM           3

#define T_STRING    0
#define T_INT       1
#define T_FLOAT     2


struct parser_var {
    char name[128];
    char pattern[256];
    unsigned avg_period;
    unsigned interval;
    int type;
};

struct parser {
    char source[1024]; 
    struct parser_var *vars;
    unsigned vars_count;
};

struct daemon_config {
    unsigned port;
    unsigned daemonize;
    struct parser *file_parsers;
    struct parser *cmd_parsers;
};

struct daemon_config parse_config(char *config_filename);
