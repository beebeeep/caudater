#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>

#include "caudater.h"

void copy_setting(config_setting_t *setting, const char *key, void *dst, unsigned type) 
{
  const char *value;
  int found = config_setting_lookup_string(setting, key, &value);
  if (found == CONFIG_FALSE) {
    fprintf(stderr, "Cannot find setting '%s'\n", key);
    exit(-1);
  }
  if (type == T_STRING) {
      strncpy((char *)dst, value, 255);
  } else if (type == T_INT) {
      *(int *)dst = atoi(value);
  } else if (type == T_FLOAT) { 
      *(float *)dst = atof(value);
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

        config_setting_t *vars = config_setting_get_member(file, "vars");
        parsers[i].vars_count = config_setting_length(vars);
        parsers[i].vars = (struct parser_var *)malloc(sizeof(struct parser_var) * parsers[i].vars_count);
        int j;
        for (j = 0; j < parsers[i].vars_count; j++) {
            struct parser_var *v = &parsers[i].vars[j];
            config_setting_t *var = config_setting_get_elem(vars, j);
            const char *type;
            copy_setting(var, "name", v->name, T_STRING);
            config_setting_lookup_string(var, "type", &type);
            printf("\tprocessing var %s of type %s\n", v->name, type);

            if(!strcmp(type, "lastvalue")) {
                v->type = TYPE_LASTVALUE;
                copy_setting(var, "pattern", v->pattern, T_STRING);
            } else if (!strcmp(type, "rps")) {
                v->type = TYPE_RPS;
                copy_setting(var, "pattern", v->pattern, T_STRING);
                copy_setting(var, "interval", &v->interval, T_INT);
            } else if (!strcmp(type, "summ")) {
                v->type = TYPE_SUMM;
                copy_setting(var, "pattern", v->pattern, T_STRING);
            } else if (!strcmp(type, "count")) {
                v->type = TYPE_COUNT;
                copy_setting(var, "pattern", v->pattern, T_STRING);
            }
        }
    }

    config.file_parsers = parsers;
    config.cmd_parsers = NULL;
    return config;
}
