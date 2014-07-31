#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libconfig.h>


#define TYPE_LASTVALUE 0
#define TYPE_COUNTLINES 1
#define TYPE_AVGCOUNT 2


struct parser_var {
    char name[128];
    char pattern[256];
    unsigned avg_period;
    unsigned interval;
    int type;
};

struct parser {
    char filename[1024]; 
    struct parser_var *vars;
    unsigned vars_count;
};

int bind_port;
int daemonize;
struct parser *parsers;

void copy_setting(config_setting_t *setting, const char *key, char dst[]) 
{
  const char *value;
  int found = config_setting_lookup_string(setting, key, &value);
  if (found == CONFIG_FALSE) {
    fprintf(stderr, "Cannot find setting '%s'\n", key);
    exit(-1);
  }
  strncpy(dst, value, sizeof(dst));
}

int main(int argc, char **argv)
{
    /* используются свои типы. */
    config_t cfg; 
    config_setting_t *setting;
    const char *str;

    config_init(&cfg); /* обязательная инициализация */

    /* Читаем файл. Если ошибка, то завершаем работу */
    if(! config_read_file(&cfg, "config.conf"))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return(EXIT_FAILURE);
    }

    setting = config_lookup(&cfg, "general.port");
    if (setting == NULL) {
        perror("port not specified!");
    } else {
        bind_port = config_setting_get_int(setting);
    }
    setting = config_lookup(&cfg, "general.daemon");
    if (setting == NULL) {
        perror("daemon not specified!");
    } else {
        daemonize = config_setting_get_bool(setting);
    }

    setting = config_lookup(&cfg, "files");
    int files_count = config_setting_length(setting);
    parsers = (struct parser *)malloc(files_count * sizeof(struct parser));
    int i;
    for (i = 0; i < files_count; i++) {
        config_setting_t *file = config_setting_get_elem(setting, i);
        copy_setting(file, "path", parsers[i].filename);
        printf("Processing file %s\n", parsers[i].filename);

        config_setting_t *vars = config_setting_get_member(file, "vars");
        parsers[i].vars_count = config_setting_length(vars);
        parsers[i].vars = (struct parser_var *)malloc(sizeof(struct parser_var) * parsers[i].vars_count);
        int j;
        for (j = 0; j < parsers[i].vars_count; j++) {
            struct parser_var *v = &parsers[i].vars[j];
            config_setting_t *var = config_setting_get_elem(vars, j);
            const char *pattern, *type;
            copy_setting(var, "name", v->name);
            config_setting_lookup_string(var, "type", &type);
            printf("\tprocessing var %s of type %s\n", v->name, type);
            if(!strcmp(type, "lastvalue")) {
                v->type = TYPE_LASTVALUE;
            } else if (!strcmp(type, "countlines")) {
                v->type = TYPE_COUNTLINES;
                copy_setting(var, "pattern", v->pattern);
            } else if (!strcmp(type, "avgcount")) {
                v->type = TYPE_AVGCOUNT;
                copy_setting(var, "pattern", v->pattern);
            }
        }
    }
    return 0;
}
