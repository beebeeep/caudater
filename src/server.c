#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "caudater.h"

struct responder_param {
  int fd;
  pthread_t thread_id;
  struct daemon_config *cfg;
};

void *responder(void *p) 
{
  struct responder_param *param = (struct responder_param *)p;
  unsigned i, metric; 
  char *buff = (char *)malloc(BUFF_SIZE);
  for (i = 0; i < param->cfg->parsers_count; i++) {
    struct parser *parser = &param->cfg->parsers[i];
    for (metric = 0; metric < parser->metrics_count; metric++) {
      struct metric *m  = &parser->metrics[metric];
      if (m->type == TYPE_LASTVALUE) {
        int bytes = snprintf(buff, BUFF_SIZE, "%s=%s\n", m->name, (char *)m->result);
        write(param->fd, buff, bytes);
      } else if (m->type == TYPE_COUNT) {
        int bytes = snprintf(buff, BUFF_SIZE, "%s=%lu\n", m->name, *((unsigned long *)m->result));
        write(param->fd, buff, bytes);
      } else if (m->type == TYPE_RPS) {
        int bytes = snprintf(buff, BUFF_SIZE, "%s=%.2f\n", m->name, *((double *)m->result));
        write(param->fd, buff, bytes);
      } else if (m->type == TYPE_SUM) {
        int bytes = snprintf(buff, BUFF_SIZE, "%s=%.2f\n", m->name, *((double *)m->result));
        write(param->fd, buff, bytes);
      }
    }
  }
  close(param->fd);
  free((void *)param);
  free(buff);
  return NULL;
}

void start_server(struct daemon_config *config)
{
  struct sockaddr_in6 sin;
  memset( &sin, 0, sizeof(sin) );
  sin.sin6_family = AF_INET6;    /* fuck IPv4, we're running at least on dualstack system */
  sin.sin6_addr = in6addr_any; 
  sin.sin6_port = htons( config->port );

  config->server_listenfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (config->server_listenfd < 0) {
    perror("Cannot create socket");
    exit(-1);
  }
  int x = 1;
  if(setsockopt(config->server_listenfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x)) != 0) {
      perror("setsockopt failed");
      exit(-1);
  }
  

  if (bind(config->server_listenfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("Cannot bind socket");
    exit(-1);
  }

  if (listen(config->server_listenfd, 10) < 0) {
    perror("Cannot start listen()");
    exit(-1);
  }

  int connfd;

  pthread_attr_t attr;

  for(;;) {
    connfd = accept(config->server_listenfd, (struct sockaddr *)NULL, NULL);
    if(connfd >= 0) {
      struct responder_param *param = (struct responder_param *)malloc(sizeof(struct responder_param));
      param->fd = connfd;
      param->cfg = config;

      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_create(&param->thread_id, &attr, responder, (void *)param);
      pthread_attr_destroy(&attr);
    }
  }
}

