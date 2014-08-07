#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <pcre.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <errno.h>

#include "caudater.h"

void process_metric(struct metric *metric, char *line)
{
    int ovector[30];
    int rc = 0;
    if(line != NULL) {
        rc = pcre_exec(metric->re, metric->re_extra, line, strlen(line), 0, 0, ovector, 30);
    }
    if(rc > 0 || (line == NULL && metric->type == TYPE_RPS) ) {
        switch(metric->type) {
            case TYPE_COUNT: 
                {
                    *((unsigned long *)metric->result) += 1;
                    printf("Count: '%lu'\n", *((unsigned long *)metric->result));
                } break;
            case TYPE_LASTVALUE: 
                {
                    size_t len = ovector[3] - ovector[2];
                    if(len > BUFF_SIZE - 1) {
                        len = BUFF_SIZE - 1;
                    }
                    memcpy(metric->result, line + ovector[2], len);
                    ((char *)metric->result)[len] = '\0';
                    printf("Last value: '%s'\n", (char *)metric->result);
                } break;
            case TYPE_SUM: 
                {
                    char match[BUFF_SIZE];
                    size_t len = ovector[3] - ovector[2];
                    if(len > BUFF_SIZE - 1) {
                        len = BUFF_SIZE - 1;
                    }
                    memcpy(match, line + ovector[2], len);
                    match[len] = '\0';
                    errno = 0;
                    double result = strtod(match, NULL);
                    if (errno == 0) {
                        *((double *)metric->result) += result;
                    } else {
                        printf("error\n");
                    }
                    printf("Summ: '%f'\n", *((double *)metric->result));
                } break;
            case TYPE_RPS: 
                {
                    time_t tdiff = time(NULL) - metric->last_updated;
                    if(tdiff >= metric->interval) {
                        *((double *)metric->result) = (*((double *)metric->acc))/tdiff;
                        *((double *)metric->acc) = 0.0;
                        metric->last_updated = time(NULL);
                        printf("RPS: %f\n", *((double *)metric->result));
                    } else {
                        *((double *)metric->acc) += 1.0;
                    }
                } break;
        }
    } 
}
void file_parser (struct parser *parser)
{
    printf("Starting file_parser for for %s with %i metrics\n", parser->source, parser->metrics_count);
    if (access(parser->source, R_OK) != 0) {
        char msg[1024];
        snprintf(msg, 1024, "Error reading '%s'", parser->source);
        perror(msg);
        exit(-1);
    }

    FILE *file = fopen(parser->source, "r");
    char *line;
    size_t bytes;
    int i;

    /* setup inotify under watched file */
    int ifd = inotify_init();
    struct inotify_event event;
    int iwd = inotify_add_watch(ifd, parser->source, IN_MODIFY);
    if (iwd < 0) {
        perror("Cannot add watch");
        exit(-1);
    }

    unsigned min_interval = parser->metrics[0].interval;
    for (i = 0; i < parser->metrics_count; i++) {
        if(parser->metrics[i].interval < min_interval) {
            min_interval = parser->metrics[i].interval;
        }
    }
    /* we will use select on inotify fd to recount rps after specified interval */
    fd_set rfds;
    struct timeval tv, *timeout;
    int ready;

    while(1) {
        if(min_interval != 0) {
            timeout = &tv;
            timeout->tv_sec = min_interval;
            timeout->tv_usec = 0;
        } else {
            timeout = NULL;
        }
        FD_ZERO(&rfds);
        FD_SET(ifd, &rfds);
        ready = select(ifd+1, &rfds, NULL, NULL, timeout);
        if(ready == -1) {
            perror("Error in select()");
            exit(-1);
        } else if (FD_ISSET(ifd, &rfds)) {
            read(ifd, &event, sizeof(event));
            if (event.mask & IN_MODIFY) {
                /* got some data, read all lines and process all metrics for them 
                 * XXX let's think than file we reading is line-buferred and we 
                 * XXX don't lock for too much time waiting last endline
                 */
                while(getline(&line, &bytes, file) != -1) {
                    for (i = 0; i < parser->metrics_count; i++) {
                        process_metric(&parser->metrics[i], line);
                    }
                }
            }
        } else if (ready == 0){
            /* no data within selected interval, firing DRY processing for counting RPS*/
            printf("Timeout, dry run\n");
            for (i = 0; i < parser->metrics_count; i++) {
                process_metric(&parser->metrics[i], NULL);
            }
        }
    }


}
int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("USAGE: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }
    
    struct daemon_config config = parse_config(argv[1]);

    file_parser(&config.file_parsers[0]);

    exit(0);

    return 0;
}
