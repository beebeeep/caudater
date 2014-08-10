#include <stdio.h>
#include <yaml.h>

#include "caudater.h"


/********************************
 *
 *
 *
 *
 *
 *
 *  THIS IS FUCKING CRAZY SHIT
 *
 *  TRY TO LOOK EVENT-BASED PARSING
 *
 **************************************/

struct daemon_config parse_config(char *config_filename)
{
    struct daemon_config config;

    FILE *fh = fopen(config_filename, "r");

    yaml_parser_t parser;
    yaml_token_t  token;   /* new variable */

    /* Initialize parser */
    if(!yaml_parser_initialize(&parser))
        fputs("Failed to initialize parser!\n", stderr);
    if(fh == NULL)
        fputs("Failed to open file!\n", stderr);

    /* Set input file */
    yaml_parser_set_input_file(&parser, fh);

    int config_level = CL_TOP;
    /* BEGIN new code */
    do {
        yaml_parser_scan(&parser, &token);
        if (token.type == YAML_KEY_TOKEN) {
            yaml_parser_scan(&parser, &token);
            if(token.type == YAML_SCALAR_TOKEN) {
                if(!strcmp(token.data, "general")) {
                    read_general_settings(&config, &parser);
                } else if (!strcmp(token.data, "parsers")) {
                    read_parsers_settings(&config, &parser);
                } else {
                    fprintf(stderr, "Unknown config option!\n");
                    exit(-1);
                }
            } else {
                fprintf(stderr, "Error parsing config\n");
                exit(-1);
            }
        }                                          
        if(token.type != YAML_STREAM_END_TOKEN)
            yaml_token_delete(&token);
    } while(token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);

void read_general_setting(struct daemon_config *cfg, yaml_parser_t *p)
{
    yaml_token_t t;
    do {
        yaml_parser_scan(&p, &t);
        if (t.type != YAML_BLOCK_MAPPING_START_TOKEN) {
            yaml_token_delete(&t);
            break;
        }
    }

    do {
        yaml_parser_scan(&p, &t);
        if(t.type == YAML_KEY_TOKEN) {
            yaml_token_delete(&t);
            yaml_parser_scan(&p, &t);
            if(t.type == YAML_VALUE_TOKEN) {
                yaml_token_delete(&t);
                yaml_parser_scan(&p, &t);
                if(t.type == YAML_SCALAR_TOKEN) {
                    if (!strcmp(t.data, "port")) {
                        yaml_token_delete(&t);
                        yaml_parser_scan(&p, &t);
                        cfg->port = atoi(t.data);
                    } else if (!strcmp(t.data, "daemonize")) {
                        yaml_token_delete(&t);
                        yaml_parser_scan(&p, &t);
                        cfg->daemonize = (!strcmp(t.data, "true"));
                    }
                }
            }
        }
    } while (t.type != YAML_BLOCK_END_TOKEN);
}

void read_parsers_settings(struct daemon_config *cfg, yaml_parser_t *p)
{
    yaml_token_t t;
    char src[SOURCE_MAX];
    do {
        yaml_parser_scan(&p, &t);
        if (t.type != YAML_BLOCK_MAPPING_START_TOKEN) {
            yaml_token_delete(&t);
            break;
        }
    }

    do { //cycle parser source
        yaml_parser_scan(&p, &t);
        if(t.type == YAML_KEY_TOKEN) {
            yaml_parser_scan(&p, &t);
            if (t.type == YAML_SCALAR_TOKEN) {
                strncpy(src, t.data, SOURCE_MAX);
            }
        }
        yaml_token_delete(&t);
        yaml_parser_scan(&p, &t);            
        if(t.type == YAML_VALUE_TOKEN) {
            yaml_token_delete(&t);
            yaml_parser_scan(&p, &t);
            if(t.type == YAML_BLOCK_MAPPING_START_TOKEN) {
                cfg->parsers_count++; 
                cfg->parsers = (struct parser *)realloc(cfg->parsers, sizeof(struct parser) * cfg->parsers_count);
                read_parser(&cfg[cfg->parsers_count-1], p);
            }
        }
    } while (t.type != YAML_BLOCK_END_TOKEN);
}

void read_parser(struct parser *parser, yaml_parser_t *p) 
{
    yaml_token_t t;
    do { //cycle parser source
        yaml_parser_scan(&p, &t);
        if(t.type == YAML_KEY_TOKEN) {
            yaml_token_delete(&t);
            yaml_parser_scan(&p, &t);
            if (t.type == YAML_SCALAR_TOKEN) {
                if(!strcmp(t.data, "type") {
                        re
            }
        }
        yaml_token_delete(&t);
        yaml_parser_scan(&p, &t);            
        if(t.type == YAML_VALUE_TOKEN) {
            yaml_token_delete(&t)
            yaml_parser_scan(&p, &t);
            if(t.type == YAML_BLOCK_MAPPING_START_TOKEN) {
                cfg->parsers_count++; 
                cfg->parsers = (struct parser *)realloc(cfg->parsers, sizeof(struct parser) * cfg->parsers_count);
                read_parser(&cfg[cfg->parsers_count-1], p);
            }
        }
    } while (t.type != YAML_BLOCK_END_TOKEN);
}




            






}
        switch(token.type)
        {
            /* Stream start/end */
            case YAML_STREAM_START_TOKEN: puts("STREAM START"); break;
            case YAML_STREAM_END_TOKEN:   puts("STREAM END");   break;
                                          /* Token types (read before actual token) */
            case YAML_KEY_TOKEN:  

            case YAML_VALUE_TOKEN: printf("(Value token) "); break;
                                   /* Block delimeters */
            case YAML_BLOCK_SEQUENCE_START_TOKEN: puts("<b>Start Block (Sequence)</b>"); break;
            case YAML_BLOCK_ENTRY_TOKEN:          puts("<b>Start Block (Entry)</b>");    break;
            case YAML_BLOCK_END_TOKEN:            puts("<b>End block</b>");              break;
                                                  /* Data */
            case YAML_BLOCK_MAPPING_START_TOKEN:  puts("[Block mapping]");            break;
            case YAML_SCALAR_TOKEN:  printf("scalar %s \n", token.data.scalar.value); break;
                                     /* Others */
            default:
                                     printf("Got token of type %d\n", token.type);
        }
        if(token.type != YAML_STREAM_END_TOKEN)
            yaml_token_delete(&token);
    } while(token.type != YAML_STREAM_END_TOKEN);
    yaml_token_delete(&token);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);
    fclose(fh);
    return 0;
}

