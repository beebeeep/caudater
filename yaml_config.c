#ifdef USE_YAML_CONFIG
#include <stdio.h>
#include <yaml.h>
#include <string.h>
#include <pcre.h>

#include "caudater.h"

#define MAX_ELEM_COUNT 1024

/* struct for keeping tree elements parent stack */
typedef struct elem_t {
    void *value;
    struct elem_t *parent;
} elem;

typedef struct kv_elem_t {
    char *key;
    char *value;
    struct kv_elem_t *parent;
} kv_elem;

/* takes element to push and stack head poiner, return new stack head */
elem *push(void *m, elem *stack) 
{
    elem *new_elem = (elem *)malloc(sizeof(elem));
    new_elem->parent = stack;
    new_elem->value = m;
    return new_elem;
}

/* returns value stored on stack head, changes stack head to prev elem */
void *pop(elem **stack)
{
    elem *m = *stack;
    *stack = m->parent;
    void *value = m->value;
    free(m);
    return value;
}

char * get_elem_by_key(kv_elem *tree[], size_t count, char *key)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if(!strcmp(key, tree[i]->key)) {
            return tree[i]->value;
        }
    }
    return NULL;
}

char * get_elem_by_key_parent(kv_elem *tree[], size_t count, char *key, char *parent)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if(tree[i]->parent != NULL && 
                !strcmp(tree[i]->parent->key, parent) &&
                !strcmp(key, tree[i]->key)) {
            return tree[i]->value;
        }
    }
    return NULL;
}

char *alloc_copy(char *str)
{
    if (str == NULL) {
        return NULL;
    }
    char *dest = (char *)malloc(strlen(str)+1);
    strcpy(dest, str);
    return dest;
}


struct daemon_config parse_config(char *config_filename)
{
    FILE *fh = fopen(config_filename, "r");

    if (fh == NULL) {
        perror("Error opening config file");
        exit(-1);
    }
    yaml_parser_t parser;
    yaml_event_t  event;   /* New variable */

    /* Initialize parser */
    if(!yaml_parser_initialize(&parser))
        fputs("Failed to initialize parser!\n", stderr);
    if(fh == NULL)
        fputs("Failed to open file!\n", stderr);

    /* Set input file */
    yaml_parser_set_input_file(&parser, fh);

    kv_elem *top_elem = (kv_elem *)malloc(sizeof(elem));
    top_elem->key = "HEAD"; top_elem->value = NULL; top_elem->parent = NULL; 
    kv_elem *curr_elem = top_elem;

    elem *parent_stack = push(top_elem, NULL);

    kv_elem *tree[MAX_ELEM_COUNT];
    tree[0] = top_elem;
    unsigned elem_count = 1;

    unsigned kv_parity = 0;

    /* START new code */
    do {
        if (!yaml_parser_parse(&parser, &event)) {
            printf("Parser error %d\n", parser.error);
            exit(EXIT_FAILURE);
        }

        switch(event.type)
        { 
            case YAML_NO_EVENT:
            case YAML_STREAM_START_EVENT:
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_SEQUENCE_START_EVENT: 
            case YAML_SEQUENCE_END_EVENT:   
            case YAML_ALIAS_EVENT:  
                break;
            case YAML_MAPPING_START_EVENT:  
                                            kv_parity = 0;
                                            parent_stack = push(curr_elem, parent_stack);
                                            break;
            case YAML_MAPPING_END_EVENT:    
                                            kv_parity = 0;
                                            curr_elem = pop(&parent_stack);
                                            break;
            case YAML_SCALAR_EVENT: 
                                            if(kv_parity++ % 2 == 0) {      /* this is a key */
                                                kv_elem *new_elem = (kv_elem *)malloc(sizeof(kv_elem));
                                                new_elem->key = malloc(event.data.scalar.length);
                                                new_elem->value = NULL;
                                                memcpy(new_elem->key, event.data.scalar.value, event.data.scalar.length+1);
                                                new_elem->parent = parent_stack->value;
                                                tree[elem_count++] = new_elem; 
                                                curr_elem = new_elem; 
                                            } else {    /* this is a value */
                                                curr_elem->value = malloc(event.data.scalar.length);
                                                memcpy(curr_elem->value, event.data.scalar.value, event.data.scalar.length+1);
                                            }


                                            break;
        }
        if(event.type != YAML_STREAM_END_EVENT)
            yaml_event_delete(&event);
    } while(event.type != YAML_STREAM_END_EVENT);
    yaml_event_delete(&event);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);
    fclose(fh);

    size_t i;
    struct daemon_config cfg;
    cfg.parsers_count = 0;
    cfg.parsers = NULL;
    struct parser *current_parser = NULL;
    struct metric *current_metric = NULL;

    char *port = get_elem_by_key_parent(tree, elem_count,  "port", "general");
    if (port == NULL) {
        printf("Error parsing config: port not found\n");
        exit(-1);
    } else {
        cfg.port = atoi(port);
    }
    for (i = 0; i < elem_count; i++) { 
        kv_elem *c = tree[i];
        printf("Elem %p:\tkey=%s, value=%s, parent = %s\n", (void *)c, c->key, (c->value != NULL)?c->value:"NULL", (c->parent != NULL)?c->parent->key:"NULL");
        
        if(c->parent == NULL) continue;

        if(!strcmp(c->parent->key, "parsers")) {
            cfg.parsers_count++;
            cfg.parsers = (struct parser *)realloc(cfg.parsers, sizeof(struct parser) * cfg.parsers_count);
            current_parser = &cfg.parsers[cfg.parsers_count-1]; 
            current_parser->source = alloc_copy(c->key);
            current_parser->metrics_count = 0;
            printf("Found parser %s\n", current_parser->source);

            char *parser_type = get_elem_by_key_parent(tree, elem_count, "type", current_parser->source);
            if (!strcmp(parser_type, "file")) {
                current_parser->type = PT_FILE;
            } else if (!strcmp(parser_type, "command")) {
                current_parser->type = PT_CMD;
            }
        }

        if(!strcmp(c->parent->key, "metrics")) {
           current_parser->metrics_count++;
           current_parser->metrics = (struct metric *)realloc(
                   current_parser->metrics, 
                   sizeof(struct metric)*current_parser->metrics_count);
           current_metric = &current_parser->metrics[current_parser->metrics_count-1];
           current_metric->name = alloc_copy(c->key);

           char *pattern = get_elem_by_key_parent(tree, elem_count, "pattern", current_metric->name);
           if(pattern == NULL) {
               printf("Error parsing metric '%s' config: metric should have pattern!", current_metric->name);
               exit(-1);
           }
           current_metric->pattern = alloc_copy(pattern);

           char *interval = get_elem_by_key_parent(tree, elem_count, "interval", current_metric->name); 
           if(interval != NULL) {
               current_metric->interval = atoi(interval);
           }
           char *type = get_elem_by_key_parent(tree, elem_count, "type", current_metric->name);
           if(type == NULL) {
               printf("Error parsing metric '%s' config: metric should have type!", current_metric->name);
               exit(-1);
           }

           if(!strcmp(type, "lastvalue")) {
               current_metric->type = TYPE_LASTVALUE;
               current_metric->acc = NULL;
               current_metric->result = malloc(BUFF_SIZE);
           } else if (!strcmp(type, "rps")) {
               current_metric->type = TYPE_RPS;
               current_metric->acc = malloc(sizeof(double));
               current_metric->result = malloc(sizeof(double));
               *((double *)current_metric->result) = 0.0;
           } else if (!strcmp(type, "sum")) {
               current_metric->type = TYPE_SUM;
               current_metric->acc = malloc(sizeof(double));
               current_metric->result = malloc(sizeof(double));
               *((double *)current_metric->result) = 0.0;
           } else if (!strcmp(type, "count")) {
               current_metric->type = TYPE_COUNT;
               current_metric->acc = NULL;
               current_metric->result = malloc(sizeof(unsigned long));
               *((unsigned long *)current_metric->result) = 0;
           }

           const char *pcre_error;
           int pcre_erroffset;
           current_metric->re = pcre_compile(current_metric->pattern, PCRE_UTF8, &pcre_error, &pcre_erroffset, NULL);    
           if (current_metric->re == NULL) {
               printf("PCRE compilation failed at offset %d: %s\n", pcre_erroffset, pcre_error);
               exit(1);
           }
           current_metric->re_extra = pcre_study(current_metric->re, 0, &pcre_error);
           if (pcre_error != NULL) {
               printf("Errors studying pattern: %s\n", pcre_error);
               exit(1);
           }
           current_metric->last_updated = 0;

        }
    }
            
    return cfg;
}

#endif
