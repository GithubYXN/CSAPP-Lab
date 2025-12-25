#include "csapp.h"
#include <stdio.h>
#include <strings.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct {
  char host[MAXLINE];
  char port[MAXLINE];
  char path[MAXLINE];
} url_t;

void doit(int fd);
int parse_url(url_t *url, char *uri);
void parse_header(rio_t *rp, char *header, url_t *url);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) {
  int listenfd, connfd;
  char host[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, host, MAXLINE, port, MAXLINE, 0);
    printf("Accept connection from (%s, %s)\n", host, port);
    doit(connfd);
    Close(connfd);
  }

  return 0;
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
  parse_header(&rio_c, header, url);

  servfd = Open_clientfd(url->host, url->port);
  Rio_readinitb(&rio_s, servfd);
  Rio_writen(servfd, header, strlen(header));

  int n;
  while ((n = Rio_readlineb(&rio_s, buf, MAXLINE)) != 0) {
    printf("Accept %d bytes from server\n", n);
    Rio_writen(fd, buf, n);
  }
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
