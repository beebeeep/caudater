#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <pcre.h>

#include "caudater.h"

void copy_setting(config_setting_t *setting, const char *key, void *dst, unsigned type) 
{
  if (type == T_STRING) {
      const char *value;
      int found = config_setting_lookup_string(setting, key, &value);
      if (found == CONFIG_FALSE) {
        fprintf(stderr, "Cannot find setting '%s'\n", key);
        exit(-1);
      }
      strncpy((char *)dst, value, 255);
  } else if (type == T_INT) {
      int found = config_setting_lookup_int(setting, key, (int *)dst);
      if (found == CONFIG_FALSE) {
        fprintf(stderr, "Cannot find setting '%s'\n", key);
        exit(-1);
      }
  } else if (type == T_FLOAT) { 
      int found = config_setting_lookup_float(setting, key, (double *)dst);
      if (found == CONFIG_FALSE) {
        fprintf(stderr, "Cannot find setting '%s'\n", key);
        exit(-1);
      }
  }

}

struct daemon_config parse_config(char *config_filename)
{
    config_t cfg; 
    config_setting_t *setting;
    struct daemon_config config;


    config_init(&cfg); 
    if(! config_read_file(&cfg, config_filename))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        exit(EXIT_FAILURE);
    }

    setting = config_lookup(&cfg, "general.port");
    if (setting == NULL) {
        perror("port not specified!");
    } else {
        config.port = config_setting_get_int(setting);
    }
    setting = config_lookup(&cfg, "general.daemon");
    if (setting == NULL) {
        perror("daemon not specified!");
    } else {
        config.daemonize = config_setting_get_bool(setting);
    }

    setting = config_lookup(&cfg, "files");
    int files_count = config_setting_length(setting);

    struct parser *parsers = (struct parser *)malloc(files_count * sizeof(struct parser));

    int i;
    for (i = 0; i < files_count; i++) {
        config_setting_t *file = config_setting_get_elem(setting, i);
        copy_setting(file, "path", parsers[i].source, T_STRING);
        printf("Processing file %s\n", parsers[i].source);

        config_setting_t *metrics = config_setting_get_member(file, "metrics");
        parsers[i].metrics_count = config_setting_length(metrics);
        parsers[i].metrics = (struct metric *)malloc(sizeof(struct metric) * parsers[i].metrics_count);
        int j;
        for (j = 0; j < parsers[i].metrics_count; j++) {
            struct metric *v = &parsers[i].metrics[j];
            v->interval = 0;
            config_setting_t *var = config_setting_get_elem(metrics, j);
            const char *type;
            copy_setting(var, "name", v->name, T_STRING);
            config_setting_lookup_string(var, "type", &type);

            if(!strcmp(type, "lastvalue")) {
                v->type = TYPE_LASTVALUE;
                copy_setting(var, "pattern", v->pattern, T_STRING);
            } else if (!strcmp(type, "rps")) {
                v->type = TYPE_RPS;
                copy_setting(var, "pattern", v->pattern, T_STRING);
                copy_setting(var, "interval", &v->interval, T_INT);
            } else if (!strcmp(type, "sum")) {
                v->type = TYPE_SUM;
                copy_setting(var, "pattern", v->pattern, T_STRING);
            } else if (!strcmp(type, "count")) {
                v->type = TYPE_COUNT;
                copy_setting(var, "pattern", v->pattern, T_STRING);
            }

            printf("\tprocessing var %s of type %s with pattern '%s'\n", v->name, type, v->pattern);

            const char *pcre_error;
            int pcre_erroffset;
            v->re = pcre_compile(v->pattern, PCRE_UTF16, &pcre_error, &pcre_erroffset, NULL);    
            if (v->re == NULL) {
                printf("PCRE compilation failed at offset %d: %s\n", pcre_erroffset, pcre_error);
                exit(1);
            }
            v->re_extra = pcre_study(v->re, 0, &pcre_error);
            if (pcre_error != NULL) {
                printf("Errors studying pattern: %s\n", pcre_error);
                exit(1);
            }
            v->last_updated = 0;
            switch(v->type) {
                case TYPE_COUNT:
                    v->acc = NULL;
                    v->result = malloc(sizeof(unsigned long));
                    *((unsigned long *)v->result) = 0;
                    break;
                case TYPE_LASTVALUE: 
                    v->acc = NULL;
                    v->result = malloc(BUFF_SIZE);
                    break;
                case TYPE_RPS: 
                    v->acc = malloc(sizeof(double));
                    v->result = malloc(sizeof(double));
                    *((double *)v->result) = 0.0;
                    break;
                case TYPE_SUM: 
                    v->acc = malloc(sizeof(double));
                    v->result = malloc(sizeof(double));
                    *((double *)v->result) = 0.0;
                    break;
            }
        }
    }

    config.file_parsers = parsers;
    config.cmd_parsers = NULL;
    return config;
}
