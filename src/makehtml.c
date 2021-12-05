#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>

#define BUFF_SIZE 30

// storage 디렉토리를 바탕으로 다운로드 웹 페이지를 만듬

int main(int argc, char * argv[]){

  FILE *fp;

  FILE * pp;
  char buff[BUFF_SIZE];
  int file_num = 0;
  int i;

  printf("HTML을 생성합니다.\n");

  // printf("argc : %d\n", argc);
  // printf("argv : %s  %s \n", argv[0], argv[1]);

  pp = popen("cd storage;ls -p | grep -v '/$'", "r");

  fp = fopen("index.html", "w");

  if(fp == NULL){
  perror("mh fopen : ");
  exit(1);
  }

  fputs("<!doctype html>\n",fp);
  fputs("<html>\n",fp);
    fputs("<head>\n",fp);
      fprintf(fp, "<title>%s</title>\n", "test page");
      fprintf(fp, "<link rel=\"stylesheet\" href=\"style.css\">\n");
    fputs("</head>\n",fp);

    fputs("<body>\n",fp);

      fputs("<h1>BROWSER FILE DOWNLOAD</h1>\n",fp);

      fputs("<div class=\"centerbox\">\n",fp);

        fputs("<table class=\"type01\">\n",fp);
        i = 1;
        while(fgets(buff, BUFF_SIZE, pp)){
          fputs("<tr>\n",fp);
            fprintf(fp, "<th scope=\"row\">%d</th>\n", i++);
            fprintf(fp, "<td><a href=\"storage/%s\" download>%s</a></td>\n", buff, buff);
          fputs("</tr>\n",fp);
        }
        fputs("</table>\n",fp);

      fputs("</div>\n",fp);

      // fprintf(fp, "<script src=%s></script>\n", "index.js");

    fputs("</body>\n",fp);

  fputs("</html>\n",fp);

  fclose(fp);
  pclose(pp);

  printf("HTML 생성 완료\n");

  return 0;

}
