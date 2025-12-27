#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_CACHE_NUM 256

typedef struct {
  char host[MAXLINE];
  char port[MAXLINE];
  char path[MAXLINE];
} url_t;

typedef struct {
  int cnt;
  char host[MAXLINE];
  char path[MAXLINE];
  char cache_obj[MAX_OBJECT_SIZE];
} cache_data_t;

typedef struct {
  int used_size;
  int cache_num;
  cache_data_t cache_data[MAX_CACHE_NUM];
} cache_t;

void doit(int fd);
int parse_url(url_t *url, char *uri);
void parse_header(rio_t *rp, char *header, url_t *url);
void *thread_routine(void *arg);
void init_cache();
int query_cache(rio_t *rp, char *host, char *path);
int insert_cache(char *content, char *host, char *path);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static cache_t *cache;
static sem_t mutex;

int main(int argc, char **argv) {
  int listenfd, *connfd;
  char host[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);
  listenfd = Open_listenfd(argv[1]);
  init_cache();

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, host, MAXLINE, port, MAXLINE, 0);
    printf("Accept connection from (%s, %s)\n", host, port);

    pthread_t tid;
    Pthread_create(&tid, NULL, thread_routine, (void *)connfd);
  }

  return 0;
}

void *thread_routine(void *arg) {
  Pthread_detach(Pthread_self());
  int connfd = *(int *)arg;
  doit(connfd);

  return NULL;
}

void doit(int fd) {
  int servfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char header[MAXLINE];
  rio_t rio_c, rio_s;
  url_t *url = (url_t *)Malloc(sizeof(url_t));

  Rio_readinitb(&rio_c, fd);
  if (!Rio_readlineb(&rio_c, buf, MAXLINE)) {
    return;
  }
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {
    fprintf(stderr, "%s: method not implemented\n", method);
    return;
  }

  if (!parse_url(url, uri)) {
    return;
  }

  if (query_cache(&rio_c, url->host, url->path)) {
    return;
  }

  servfd = Open_clientfd(url->host, url->port);
  Rio_readinitb(&rio_s, servfd);
  parse_header(&rio_c, header, url);
  Rio_writen(servfd, header, strlen(header));

  size_t n;
  char content[MAX_OBJECT_SIZE + 1];
  while ((n = Rio_readlineb(&rio_s, buf, MAXLINE)) != 0) {
    printf("Accept %d bytes from server\n", (int)n);
    Rio_writen(fd, buf, n);
    strncat(content, buf, n);
  }
  insert_cache(content, url->host, url->path);
  Free(url);
  Close(servfd);
}

int parse_url(url_t *url, char *uri) {
  char *host = strstr(uri, "//");
  if (!host) {
    fprintf(stderr, "Not valid http request!\n");
    fprintf(stderr, "%s\n", uri);
    return 0;
  }
  host += 2;
  char *port = strstr(host, ":");
  char *path = strstr(host, "/");

  if (!path) {
    fprintf(stderr, "Not valid http request!\n");
    fprintf(stderr, "%s\n", uri);
    return 0;
  }

  if (port) {
    *port = '\0';
    strcpy(url->host, host);
    *port = ':';
    *path = '\0';
    strcpy(url->port, port + 1);
    *path = '/';
    strcpy(url->path, path);
  } else {
    strcpy(url->port, "80");
    *path = '\0';
    strcpy(url->host, host);
    *path = '/';
    strcpy(url->path, path);
  }
  return 1;
}

void parse_header(rio_t *rp, char *header, url_t *url) {
  char buf[MAXLINE], host[MAXLINE], other[MAXLINE], request_line[MAXLINE];

  while (Rio_readlineb(rp, buf, MAXLINE)) {
    if (strcmp(buf, "\r\n")) {
      break;
    }
    if (strstr(buf, "Host:")) {
      strcpy(host, buf);
    } else if (strstr(buf, "User-Agent:") || strstr(buf, "Connection:") ||
               strstr(buf, "Proxy-Connection:")) {
      continue;
    } else {
      strcat(other, buf);
    }
  }

  if (!strlen(host)) {
    sprintf(host, "Host: %s\r\n", url->host);
  }
  sprintf(request_line, "GET %s HTTP/1.0\r\n", url->path);
  sprintf(header, "%s%s%s%s%s%s\r\n", request_line, host, user_agent_hdr,
          "Connection: close\r\n", "Proxy-Connection: close\r\n", other);
  return;
}

void init_cache() {
  cache = Malloc(sizeof(cache_t));
  memset(cache, 0, sizeof(cache_t));
  Sem_init(&mutex, 0, 1);
}

int query_cache(rio_t *rp, char *host, char *path) {
  P(&mutex);

  int i;
  cache_data_t *cp;
  for (i = 0; i < cache->cache_num; i++) {
    cp = &cache->cache_data[i];
    if ((!strcmp(cp->host, host)) && (!strcmp(cp->path, path))) {
      break;
    }
  }
  if (i >= cache->cache_num) {
    V(&mutex);
    return 0;
  }
  cp->cnt = 0;
  V(&mutex);
  Rio_writen(rp->rio_fd, cp->cache_obj, strlen(cp->cache_obj));
  return 1;
}

int insert_cache(char *content, char *host, char *path) {
  P(&mutex);

  int size = strlen(content);
  if (size > MAX_OBJECT_SIZE) {
    V(&mutex);
    return 0;
  }

  int index = cache->cache_num;
  if (index >= MAX_CACHE_NUM) {
    int cnt, max_cnt = -1;
    for (int i = 0; i < MAX_CACHE_NUM; i++) {
      cnt = cache->cache_data[i].cnt;
      if (cnt > max_cnt) {
        max_cnt = cnt;
        index = i;
      }
    }
  }

  int oldsize = strlen(cache->cache_data[index].cache_obj);
  int newsize = cache->used_size - oldsize + size;
  if (newsize > MAX_CACHE_SIZE) {
    V(&mutex);
    return 0;
  }

  memset(&cache->cache_data[index], 0, sizeof(cache_data_t));
  cache->cache_data[index].cnt = 0;
  strcpy(cache->cache_data[index].host, host);
  strcpy(cache->cache_data[index].path, path);
  strcpy(cache->cache_data[index].cache_obj, content);
  if (index < MAX_CACHE_NUM) {
    cache->cache_num++;
  }
  cache->used_size = newsize;

  V(&mutex);

  return 1;
}
