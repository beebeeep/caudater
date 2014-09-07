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
#include <signal.h>
#include <sys/socket.h>
#include <curl/curl.h>

#include "caudater.h"
#include "server.h"

struct daemon_config config;
sigset_t set;

void *sig_handler(void *arg)
{
    sigset_t *set = arg;
    int sig;

    for(;;) { 
        sigwait(set, &sig);
        if(sig == SIGPIPE) {
#ifdef DEBUG
            printf("Signal handling thread got SIGPIPE, ignoring\n");
#endif
        } else {
#ifdef DEBUG
            printf("Signal handling thread got signal %d\n", sig);
#endif
            break;
        }
    }

    //close(config.server_listenfd);
    shutdown(config.server_listenfd, SHUT_RDWR);
    exit(0);
}

void zero_metric(struct metric *metric)
{

    /* а что еще надо обнулять? rps пусть будут непрерывные, 
     * avgcount тоже 
     */
    if(metric->type == TYPE_SUM) { 
        *((double *)metric->result) = 0.0;
    } else if(metric->type == TYPE_COUNT) { 
        *((unsigned long *)metric->result) = 0;
    }
}

void process_metric(struct metric *metric, char *line)
{
    int ovector[30];
    int rc = 0, ignore_rc = -1;
    if (line != NULL) {
        rc = pcre_exec(metric->re, metric->re_extra, line, strlen(line), 0, 0, ovector, 30);
        if(metric->ignore_re != NULL) {
            ignore_rc = pcre_exec(metric->ignore_re, metric->ignore_re_extra, line, strlen(line), 0, 0, ovector, 30);
        }
    }
    if ((rc > 0 && ignore_rc <= 0)  || metric->type == TYPE_RPS) {
        switch(metric->type) {
            case TYPE_COUNT: 
                {
                    *((unsigned long *)metric->result) += 1;
#ifdef DEBUG
                    printf("%s count: '%lu'\n", metric->name, *((unsigned long *)metric->result));
#endif
                } break;
            case TYPE_LASTVALUE: 
                {
                    size_t len = ovector[3] - ovector[2];
                    if (len > BUFF_SIZE - 1) {
                        len = BUFF_SIZE - 1;
                    }
                    memcpy(metric->result, line + ovector[2], len);
                    ((char *)metric->result)[len] = '\0';
#ifdef DEBUG
                    printf("%s last value: '%s'\n", metric->name, (char *)metric->result);
#endif
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
#ifdef DEBUG
                    printf("%s summ: '%f'\n", metric->name, *((double *)metric->result));
#endif
                } break;
            case TYPE_AVGCOUNT:
            case TYPE_RPS: 
                {
                    /* if there is regex match */
                    if (rc > 0 && ignore_rc <= 0) {
                        //*((double *)metric->acc) += 1.0;
                        moving_avg *t = (moving_avg *)metric->acc;
                        t->values[t->current] += 1;
                    }

                    time_t tdiff = time(NULL) - metric->last_updated;
                    if (tdiff >= 1) {
                        /* *((double *)metric->result) = (*((double *)metric->acc))/tdiff;
                        *((double *)metric->acc) = 0.0; 
                        */ 

                        moving_avg *t = (moving_avg *)metric->acc;
                        t->current = (t->current + 1) % metric->interval;
                        t->values[t->current] = 0;

                        unsigned long sum = 0;
                        size_t i;
                        for (i = 0; i < metric->interval; i++) {
                            sum += t->values[i];
                        }

                        if (metric->type == TYPE_RPS) {
                            *((double *)metric->result) = (double) sum / metric->interval;
#ifdef DEBUG
                            printf("%s RPS: %f\n", metric->name, *((double *)metric->result));
#endif
                        } else if (metric->type == TYPE_AVGCOUNT) {
                            *((unsigned long *)metric->result) = sum;
#ifdef DEBUG
                            printf("%s AVGCOUNT: %lu\n", metric->name, *((unsigned long *)metric->result));
#endif
                        }

                        metric->last_updated = time(NULL);
                    }
                } break;
        }
    } 
}

void process_all_metrics(struct parser *parser, char *line)
{
    size_t i;
    for (i = 0; i < parser->metrics_count; i++) {
        process_metric(&(parser->metrics[i]), line);
    }
}

FILE *try_open(char *filename)
{
    char msg[PATH_MAX+30];
    /* wait until we found readable file specified in config */
    for (;;) {
        if (access(filename, R_OK) != 0) {
            /* TODO придумать чего тут лучше сделать - может, помирать? */
            snprintf(msg, PATH_MAX+30, "Error reading '%s'", filename);
            perror(msg);
            struct timespec t = {.tv_sec = 300, .tv_nsec = 0}, r;
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

static size_t process_http_data(void *data, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    char *str = (char *)data;
    char *line = data; 
    struct http_cb_data *r = (struct http_cb_data*)userp;
    size_t i = 0;
    for(i = 0; i < realsize - 1; i++){
        if ( str[i] == '\n' || str[i] ==  '\r') {
            str[i] = '\0';
            if(strlen(r->str) > 0) {
                /* we have unprocessed string part from previous callback, 
                 * prepend received data with it
                 */
                r->str = realloc(r->str, strlen(r->str) + strlen(line) + 1);
                strcat(r->str, line);
                process_all_metrics(r->parser, r->str);

                r->str[0] = '\0';
            } else {
                process_all_metrics(r->parser, line);
            }
            line = &str[i + 1];
        }
    }

    /* save all remaining data will for processing in next callback or later */
    size_t remain_len = realsize - (line - str);
    r->str = realloc(r->str, remain_len + 1);
    strncpy(r->str, line, remain_len);
    r->str[remain_len] = '\0';
    return realsize;
}

void *http_parser (void *arg)
{
    struct parser *parser = (struct parser *) arg; 

#ifdef DEBUG
    printf("Starting http_parser for for %s with %lu metrics", parser->source, parser->metrics_count);
#endif
    unsigned min_interval = parser->metrics[0].interval, i;
    for (i = 0; i < parser->metrics_count; i++) {
        if (parser->metrics[i].interval < min_interval) {
            min_interval = parser->metrics[i].interval;
        }
    }
#ifdef DEBUG
    printf(" and interval %i\n", min_interval);
#endif

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);

    struct http_cb_data r;
    r.str = malloc(1);
    r.parser = parser;

    for(;;) {
        for (i = 0; i < parser->metrics_count; i++) {
            zero_metric(&parser->metrics[i]);
        }
#ifdef DEBUG
        printf("Fetching URL '%s'\n", parser->source);
#endif

        curl_handle = curl_easy_init();
        curl_easy_setopt(curl_handle, CURLOPT_URL, parser->source);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, process_http_data);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&r);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, parser->timeout);
        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            fprintf(stderr, "Cannot fetch %s: %s\n",
                    parser->source, curl_easy_strerror(res));
        } else {
            process_all_metrics(parser, r.str);
        }

        curl_easy_cleanup(curl_handle);

        struct timespec t = {.tv_sec = min_interval, .tv_nsec = 0}, r;
        nanosleep(&t, &r);
    }
    free(r.str);
    curl_global_cleanup();
}

void *cmd_parser (void *arg)
{
    struct parser *parser = (struct parser *) arg; 

#ifdef DEBUG
    printf("Starting cmd_parser for for %s with %lu metrics", parser->source, parser->metrics_count);
#endif

    FILE *cmd_fd;
    size_t bytes;
    char *line = NULL;

    unsigned min_interval = parser->metrics[0].interval, i;
    for (i = 0; i < parser->metrics_count; i++) {
        if (parser->metrics[i].interval < min_interval) {
            min_interval = parser->metrics[i].interval;
        }
    }
#ifdef DEBUG
    printf(" and interval %i\n", min_interval);
#endif

    for(;;) {
        for (i = 0; i < parser->metrics_count; i++) {
            zero_metric(&parser->metrics[i]);
        }
#ifdef DEBUG
        printf("Running command '%s'\n", parser->source);
#endif
        cmd_fd = popen(parser->source, "r");
        if(cmd_fd == NULL) {
            perror("popen() failed");
            return NULL;
        }

        while(getline(&line, &bytes, cmd_fd) != -1) {
#ifdef DEBUG
            printf("Got line '%s'\n", line);
#endif
            process_all_metrics(parser, line);
        }
        pclose(cmd_fd);

        struct timespec t = {.tv_sec = min_interval, .tv_nsec = 0}, r;
        nanosleep(&t, &r);
    }

}

void *file_parser (void *arg)
{
    struct parser *parser = (struct parser *) arg; 

#ifdef DEBUG
    printf("Starting file_parser for for %s with %lu metrics\n", parser->source, parser->metrics_count);
#endif

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

    /*
    unsigned min_interval = parser->metrics[0].interval;
    for (i = 0; i < parser->metrics_count; i++) {
        if (parser->metrics[i].interval < min_interval) {
            min_interval = parser->metrics[i].interval;
        }
    }
    */

    /* we will use select on inotify fd to recount rps after specified interval */
    fd_set rfds;
    struct timeval tv, *timeout;
    int ready;

    while(1) {
        /*
        if (min_interval != 0) {
            timeout = &tv;
            timeout->tv_sec = min_interval;
            timeout->tv_usec = 0;
        } else {
            timeout = NULL;
        }*/

        /* we update rps and avgcount every second, counting average in
         * specified window
         */
        timeout = &tv;
        timeout->tv_sec = 1;
        timeout->tv_usec = 0;

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
                    process_all_metrics(parser, line);
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
                for (i = 0; i < parser->metrics_count; i++) {
                    zero_metric(&parser->metrics[i]);
                }
            }
        } else if (ready == 0){
            /* no data within selected interval, firing DRY processing for counting RPS*/
#ifdef DEBUG
            printf("Timeout, dry run\n");
#endif
            process_all_metrics(parser, NULL);
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
    
    config = parse_config(argv[1]);

    int r;
    pthread_t sig_thread;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGPIPE);
    r = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (r != 0) {
       perror("Cannot set sigmask");
       exit(-1);
    }
    if (pthread_create(&sig_thread, NULL, &sig_handler, (void *) &set) != 0) {
        perror("Cannot start thread");
        exit(-1);
    }

    int i;
    for (i = 0; i < config.parsers_count; i++) {
        void *(*parser_worker) (void *);
        if (config.parsers[i].type == PT_FILE) {
            parser_worker = file_parser;
        } else if (config.parsers[i].type == PT_CMD) {
            parser_worker = cmd_parser;
        } else if (config.parsers[i].type == PT_HTTP) {
            parser_worker = http_parser;
        } else {
            printf ("Unknown parser type!\n");
            exit(-1);
        }
      pthread_attr_init(&config.parsers[i].thread_attr);
      pthread_attr_setdetachstate(&config.parsers[i].thread_attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&config.parsers[i].thread_id, &config.parsers[i].thread_attr, parser_worker, (void *) &config.parsers[i]) != 0) {
            perror("Cannot start thread");
            exit(-1);
        }
    }

    
    start_server(&config);
    exit(0);

    return 0;
}
