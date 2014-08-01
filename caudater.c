#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <pcre.h>
#include <string.h>
#include <time.h>

#include "caudater.h"


void process_metric(struct metric *metric, char *line)
{
    int ovector[30];
    int rc;
    rc = pcre_exec(metric->re, metric->re_extra, line, strlen(line), 0, 0, ovector, 30);
    if(rc > 0) {
        printf("%s", line);
    } 
}
void file_parser (struct parser *parser)
{
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

    int ifd = inotify_init();
    struct inotify_event event;
    int iwd = inotify_add_watch(ifd, parser->source, IN_MODIFY);
    if (iwd < 0) {
        perror("Cannot add watch");
        exit(-1);
    }

    while(1) {
        read(ifd, &event, sizeof(event));
        if (event.mask & IN_MODIFY) {
            while(getline(&line, &bytes, file) != -1) {
                for (i = 0; i < parser->metrics_count; i++) {
                    process_metric(&parser->metrics[i], line);
                }
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
