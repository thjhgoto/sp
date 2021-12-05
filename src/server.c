#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/inotify.h>

#define BUF_SIZE 1024
#define INOTIFY_BUF_SIZE 8192

#define ERROR_404      "<h1>404 Not Found</h1>\n"
#define ERROR_500    "<h1>500 Internal Server Error</h1>\n"
#define STORAGE_PATH "./storage"

typedef struct __HttpRequest HttpRequest;
typedef struct __HttpResponse HttpResponse;

// 요청을 저장할 구조체
struct __HttpRequest{
  // start-line
  char method[10]; // GET, POST, PUT .....
  char uri[256]; // Path
};

// 응답을 저장할 구조체
struct __HttpResponse{
  // start-line
  // version
  int status_code;
  char status_text[50];

  // headers
  char con_type[30];
  int con_len;
};


int parse_http_request(HttpRequest* req, char* buf){
  // error handling
  strcpy(req->method, strtok(buf, " "));
  strcpy(req->uri, strtok(NULL, " "));

  if(req->method == NULL || req->uri == NULL)
    return -1;

  return 0;
}

void init_response(HttpResponse * res, int sc, char* ct, int cl){

  char *ext;
  if(ct != NULL){
    ext = strrchr(ct, '.');
  }

  res->status_code = sc;
  if(sc == 200){
    if (!strcmp(ext, ".html"))
        strcpy(res->con_type, "text/html");
    else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg"))
        strcpy(res->con_type, "image/jpeg");
    else if (!strcmp(ext, ".png"))
        strcpy(res->con_type, "image/png");
    else if (!strcmp(ext, ".css"))
        strcpy(res->con_type, "text/css");
    else if (!strcmp(ext, ".js"))
        strcpy(res->con_type, "text/javascript");
    else strcpy(res->con_type, "text/plain");
    strcpy(res->status_text, "OK");
    res->con_len = cl;
  }else if(sc == 404){
    char * temp = "Not Found";
    strcpy(res->status_text, temp);
    strcpy(res->con_type, "text/html");
    res->con_len = strlen(ERROR_404);
  }
  else{
    char * temp = "Internal Server Error";
    strcpy(res->status_text, temp);
    strcpy(res->con_type, "text/html");
    res->con_len = strlen(ERROR_500);
  }
}

// 에러를 처리하는 함수
void error_handle(HttpResponse* res, HttpRequest* req, int ns, int code){
  char header[BUF_SIZE];
  init_response(res, code, NULL, 0);
  sprintf(header, "HTTP/1.1 %d %s\nContent-Type: %s\nContent-Length: %d\n\n", res->status_code, res->status_text, res->con_type, res->con_len);
  write(ns, header, strlen(header));

  if(code == 404){
    write(ns, ERROR_404, sizeof(ERROR_404));
  }else{
    write(ns, ERROR_500, sizeof(ERROR_500));
  }
}

void http_handler_routine(int ns) {
    char header[BUF_SIZE];
    char buf[BUF_SIZE];
    HttpRequest req;
    HttpResponse res;

    if (read(ns, buf, BUF_SIZE) < 0) {
        perror("read ");
        error_handle(&res, &req, ns, 500);
        return;
    }

    if(parse_http_request(&req, buf) == -1){
      perror("parse http ");
      error_handle(&res, &req, ns, 500);
      return;
    }

    printf("method=%s uri=%s\n", req.method, req.uri);

    char req_uri[BUF_SIZE];
    char *local_uri;
    struct stat st;

    strcpy(req_uri, req.uri);
    if (!strcmp(req_uri, "/")) strcpy(req_uri, "/index.html");

    local_uri = req_uri + 1;

    if (stat(local_uri, &st) < 0) {
        perror("stat ");
        error_handle(&res, &req, ns, 404);
        return;
    }

    int fd = open(local_uri, O_RDONLY);
    if (fd < 0) {
        perror("open ");
        error_handle(&res, &req, ns, 500);
        return;
    }

    init_response(&res, 200, local_uri, st.st_size);
    sprintf(header, "HTTP/1.1 %d %s\nContent-Type: %s\nContent-Length: %d\n\n", res.status_code, res.status_text, res.con_type, res.con_len);
    write(ns, header, strlen(header));

    int n;
    while ((n = read(fd, buf, BUF_SIZE)) > 0){
        write(ns, buf, n);
    }

}

// 특정(공유 중인) 디렉토리의 변경을 감지하고, 변경 감지시 html생성기 실행
void* update_html_routine(void * arg){

  int ifd = inotify_init();
  if( ifd == -1 ){
    printf( "inotify_init error\n" );
    return 0;
  }

  int wd = inotify_add_watch( ifd, STORAGE_PATH, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
  if( wd == -1 ){
    printf( "inotify_add_watch error\n" );
    return 0;
  }

  char buf[INOTIFY_BUF_SIZE];

  while( 1 )
  {

   int len = read( ifd, buf, INOTIFY_BUF_SIZE);
   if( len < 0 ){
    printf( "read error\n" );
    break;
   }

   int i = 0;

   while( i < len ){

     struct inotify_event * psttEvent = (struct inotify_event *)&buf[i];

     // 변경 감지시 fork 후 execv 를 사용하여 html생성기 실행
      if( (psttEvent->mask & IN_CREATE) ||  (psttEvent->mask & IN_DELETE) || (psttEvent->mask & IN_MOVED_TO) || (psttEvent->mask & IN_MOVED_FROM)){
        pid_t pid;
        switch (pid = fork()) {
          case -1:
            perror("fork ");
            exit(1);
            break;
          case 0:{
            char * argv[3];
            argv[0] = "mh";
            argv[1] = "test";
            argv[2] = NULL;
            if(execv("./mh", argv) == -1){
              perror("execv" );
              exit(1);
            }
            exit(0);
            break;
          }
          default:
            break;
        }
      }
     i += sizeof(struct inotify_event) + psttEvent->len;
   }
  }

 inotify_rm_watch(ifd, wd);
 close(ifd);
 pthread_exit(0);
}

int main(int argc, char *argv[]) {

    struct sockaddr_in sin, cli;
    int clen = sizeof(cli);
    int port, pid;
    int sd, ns;

    if (argc < 2) {
        printf("usage : %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);

    if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
      perror("socket ");
      exit(EXIT_FAILURE);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sd, (struct sockaddr *)&sin, sizeof(sin))){
        perror("bind ");
        exit(EXIT_FAILURE);
    }

    if (listen(sd, 10) == -1) {
        perror("listen ");
        exit(EXIT_FAILURE);
    }

    // 공유 디렉토리 이벤트 감지를 위한 쓰레드
    pthread_t update_tid;
    pthread_create(&update_tid, NULL, update_html_routine, NULL);

    // 자식 프로ㅔ스 무시
    signal(SIGCHLD, SIG_IGN);

    printf("%d 포트에서 서버 오픈\n",port);

    while (1) {

        printf("요청을 기다리는 중 ...\n");
        if((ns = accept(sd, (struct sockaddr *)&cli, &clen)) == -1){
          perror("accept ");
          continue;
        }

        // 요청이 오면 fork
        pid = fork();

        if(pid == 0){ // 자식 프로세스에서 http 처리
          close(sd);
          http_handler_routine(ns);
          close(ns);
          exit(EXIT_SUCCESS);
        }else if(pid > 0){ // 부모 프로세스는 다시 요청을 받도록 처리
          close(ns);
        }else{ // err
          perror("fork ");
        }

    }
}
