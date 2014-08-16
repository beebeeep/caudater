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
#include <pthread.h>
#include <linux/limits.h>

#include "caudater.h"
#include "server.h"

void process_metric(struct metric *metric, char *line)
{
    int ovector[30];
    int rc = 0;
    if (line != NULL) {
        rc = pcre_exec(metric->re, metric->re_extra, line, strlen(line), 0, 0, ovector, 30);
    }
    if (rc > 0 || (line == NULL && metric->type == TYPE_RPS) ) {
        switch(metric->type) {
            case TYPE_COUNT: 
                {
                    *((unsigned long *)metric->result) += 1;
                    printf("Count: '%lu'\n", *((unsigned long *)metric->result));
                } break;
            case TYPE_LASTVALUE: 
                {
                    size_t len = ovector[3] - ovector[2];
                    if (len > BUFF_SIZE - 1) {
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
                    if (len > BUFF_SIZE - 1) {
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
                    if (tdiff >= metric->interval) {
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

FILE *try_open(char *filename)
{
    char msg[PATH_MAX+30];
    /* wait until we found readable file specified in config */
    for (;;) {
        if (access(filename, R_OK) != 0) {
            snprintf(msg, PATH_MAX+30, "Error reading '%s'", filename);
            perror(msg);
            struct timespec t = {.tv_sec = 1, .tv_nsec = 0}, r;
            nanosleep(&t, &r);
        } else {
            FILE *f = fopen(filename, "r");
            if (f == NULL) {
                snprintf(msg, PATH_MAX+30, "Error opening '%s'", filename);
                perror(msg);
            } else {
                printf("Opened '%s' for parsing\n", filename);
                return f;
            }
        }
    }
}

void *file_parser (void *arg)
{
    struct parser *parser = (struct parser *) arg; 

    printf("Starting file_parser for for %s with %i metrics\n", parser->source, parser->metrics_count);

    FILE *file = try_open(parser->source);
    char *line;
    size_t bytes;
    int i;

    /* setup inotify under watched file */
    int ifd = inotify_init();
    struct inotify_event event;
    int iwd = inotify_add_watch(ifd, parser->source, IN_MODIFY|IN_DELETE_SELF|IN_MOVE_SELF);
    if (iwd < 0) {
        perror("Cannot add watch");
        exit(-1);
    }

    unsigned min_interval = parser->metrics[0].interval;
    for (i = 0; i < parser->metrics_count; i++) {
        if (parser->metrics[i].interval < min_interval) {
            min_interval = parser->metrics[i].interval;
        }
    }
    /* we will use select on inotify fd to recount rps after specified interval */
    fd_set rfds;
    struct timeval tv, *timeout;
    int ready;

    while(1) {
        if (min_interval != 0) {
            timeout = &tv;
            timeout->tv_sec = min_interval;
            timeout->tv_usec = 0;
        } else {
            timeout = NULL;
        }
        FD_ZERO(&rfds);
        FD_SET(ifd, &rfds);
        ready = select(ifd+1, &rfds, NULL, NULL, timeout);
        if (ready == -1) {
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
            } else if (event.mask & (IN_MOVE_SELF | IN_DELETE_SELF)) {
                /* file was deleted or moved by logrotate or someone else, try to reopen it and add new watch */
                printf("File '%s' was moved or deleted, trying to reopen\n", parser->source);
                inotify_rm_watch(ifd, iwd);
                file = try_open(parser->source);
                iwd = inotify_add_watch(ifd, parser->source, IN_MODIFY|IN_DELETE_SELF|IN_MOVE_SELF);
                if (iwd < 0) {
                    perror("Cannot add watch");
                    exit(-1);
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
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("USAGE: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }
    
    struct daemon_config config = parse_config(argv[1]);

    int i;
    for (i = 0; i < config.parsers_count; i++) {
        if (pthread_create(&config.parsers[i].thread_id, NULL, file_parser, (void *) &config.parsers[i]) != 0) {
            perror("Cannot start thread");
            exit(-1);
        }
    }
    
    start_server(&config);
    exit(0);

    return 0;
}
