#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_PORT 65535
#define PATH_MAX 512
#include "threadpool.h"
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
void exit_usage(int, char**);
int get_positive_num(char*);
int  getSubString(char *, char *,int, int);
int responde(void*);
void get_space_location(char* str, int* res);
void send_error(int code, int fd, char* path);
int path_exists(const char*);
int dir_exist(char*);
int is_file(char*);
int have_index_html(char*);
char* make_dir_html(char*);
int get_num_of_digit(int);
int have_execute_permission(char*);
int have_execute_permission_recursive(char*);
int have_read_permission(char*);
char* make_headers(size_t, char*, char*);
char* parent_path(char*);
char* get_mime_type(char*);
int send_file(int, char*);
int send_dynamic_html(int, char*);
void left_shift_string(char*, char*, int);
int main(int argc, char** argv){
    //signal(SIGPIPE, SIG_IGN);
    //checking arguments
    int max_req, port, pool_size, req_counter=0;
    if(argc!=4)
        exit_usage(argc,argv);

    port = get_positive_num(argv[1]);
    if(port<1 || port>MAX_PORT)
        exit_usage(argc,argv);

    pool_size = get_positive_num(argv[2]);
    if(pool_size<1 || pool_size>MAXT_IN_POOL)
        exit_usage(argc, argv);

    max_req = get_positive_num(argv[3]);
    if(max_req<1)
        exit_usage(argc, argv);

    threadpool* tp = create_threadpool(pool_size);
    if(!tp)
        exit(1);
    int welcome;
    if ((welcome = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        destroy_threadpool(tp);
        perror("socket");
        exit(1);
    }
    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    srv.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(welcome, (struct sockaddr*) &srv, sizeof(srv)) < 0) {
        perror("bind");
        //close(welcome);
        destroy_threadpool(tp);
        exit(1);
    }
    if(listen(welcome, 1) < 0) {// set Q of 1
        perror("listen");
        //close(welcome);
        destroy_threadpool(tp);
        exit(1);
    }
    struct sockaddr_in cli;
    int newfd;
    int cli_len = sizeof(cli);
    while(req_counter<max_req){
        newfd = accept(welcome, (struct sockaddr*) &cli, &cli_len);
        if(newfd < 0) {
            perror("accept");
            exit(1);
        }
        int* fd = (int*) malloc(sizeof(int));
        if(!fd){
            close(welcome);
            perror("malloc");
            exit(1);
        }
        *fd = newfd;
        dispatch(tp, (int(*)(void *))responde, (void*)fd);
        req_counter++;
        //printf("job counter: %d of %d\n",req_counter,max_req);
    }
    destroy_threadpool(tp);
}
void exit_usage(int argc, char** argv){
    printf("Usage: ");
    for(int i=0;i<argc;i++)
        printf("%s ",argv[i]);
    exit(1);
}
int get_positive_num(char* str){
    for(int i=0;i<strlen(str);i++) {
        if (!isdigit(str[i]))
            return -1;
    }
    int val;
    sscanf(str,"%d",&val);
    return val;
}
int  getSubString(char *source, char *target,int from, int to){
    int length=0;
    int i=0,j=0;
    //get length
    while(source[i++]!='\0')
        length++;
    if(from<0 || from>length)
        return 1;
    if(to>length)
        return 1;
    for(i=from,j=0;i<=to;i++,j++)
        target[j]=source[i];
    //assign NULL at the end of string
    target[j]='\0';
    return 0;
}
int responde(void* p){
    int fd = *((int*)p);
    //printf("respond to fd: %d\n",fd);
    //respond
    int nread=0, readed=0;
    char* req=(char*)malloc(2*sizeof(char));
    if(!req){
        perror("malloc");
        close(fd);
        free(p);
        return -1;
    }
    while(1) {
        nread = read(fd, req + readed, 1);
        if(nread==0)
            break;
        if (readed>1 && req[readed-1]=='\n' && req[readed-2]=='\r')
            break;
        readed += nread;
        req = realloc(req, sizeof(char) * (readed + 1));
        if(!req){
            perror("realloc");
            break;
        }
    }
    req[readed]='\0';
    char* protocol = (char*)malloc(readed*sizeof(char));
    if(!protocol){
        free(req);
        close(fd);
        free(p);
        return -1;
    }
    char* relPath = (char*)malloc(readed*sizeof(char));
    if(!relPath){
        free(protocol);
        free(req);
        close(fd);
        free(p);
        return -1;
    }
    char* method = (char*)malloc(readed*sizeof(char));
    if(!method){
        free(protocol);
        free(req);
        free(relPath);
        close(fd);
        free(p);
        return -1;
    }

    int space_index[2];
    get_space_location(req, space_index);
    if(space_index[0]!=3 || space_index[1]>strlen(req)-2 || space_index[0]==space_index[1]-1){
        send_error(400, fd, NULL);
        free(protocol);
        free(method);
        free(relPath);
        free(req);
        close(fd);
        free(p);
        return 0;
    }

    getSubString(req, method, 0, space_index[0]-1);
    getSubString(req, relPath, space_index[0]+1, space_index[1]-1);
    getSubString(req, protocol, space_index[1]+1, strlen(req));
    char path[PATH_MAX];
    if (!getcwd(path, sizeof(path))){
        perror("getcwd");
        free(protocol);
        free(method);
        free(relPath);
        free(req);
        close(fd);
        free(p);
        return -1;
    }
    strcat(path, relPath);
    free(relPath);
    //printf("path: %s\n", path);
    if(!(strcmp(protocol,"HTTP/1.0") || (strcmp(protocol,"HTTP/1.1")))){
        send_error(400, fd, NULL);
        free(protocol);
        free(method);
        free(req);
        close(fd);
        free(p);
        return 0;
    }

    if(strcmp(method, "GET")){
        send_error(501, fd, NULL);
        free(protocol);
        free(method);
        free(req);
        close(fd);
        free(p);
        return 0;
    }
    free(protocol);
    free(method);
    free(req);
    if(!path_exists(path)){
        send_error(404, fd, NULL);
        close(fd);
        free(p);
        return 0;
    }
    if(is_file(path)){
        char* parent_folder = parent_path(path);
        if(have_read_permission(path) && have_execute_permission_recursive(parent_folder)) {
            free(parent_folder);
            send_file(fd, path);
            close(fd);
            free(p);
            return 0;
        }
        else {
            free(parent_folder);
            send_error(403, fd, NULL);
            close(fd);
            free(p);
            return 0;
        }
    }
    else{
        if(path[strlen(path)-1]!='/'){
            char newPath[PATH_MAX],cwd[PATH_MAX];
            if(!getcwd(cwd,sizeof(cwd))) {
                perror("getcwd");
                close(fd);
                free(p);
                return -1;
            }
            left_shift_string(path,newPath,strlen(cwd));
            send_error(302, fd, newPath);
            close(fd);
            free(p);
            return 0;
        }
        else{
            if(!have_execute_permission_recursive(path)){
                send_error(403, fd, NULL);
                close(fd);
                free(p);
                return 0;
            }
            else{
                if(have_index_html(path)){
                    char* index_path =(char*)malloc(sizeof(char)*(strlen(path)+strlen("index.html")+1));
                    if(!index_path){
                        close(fd);
                        free(p);
                        return -1;
                    }
                    strcpy(index_path,path);
                    strcat(index_path,"index.html");
                    if(have_read_permission(index_path)){
                        send_file(fd,index_path);
                        free(index_path);
                        close(fd);
                        free(p);
                        return 0;
                    }
                    else{
                        send_error(403, fd, NULL);
                        free(index_path);
                        close(fd);
                        free(p);
                        return 0;
                    }
                }
                else{
                    send_dynamic_html(fd, path);
                    close(fd);
                    free(p);
                    return 0;
                }
            }
        }
    }
    close(fd);
    free(p);
    return 0;
}
void get_space_location(char* str, int* res){
    int first_space=-1, second_space=-1, space_counter=0;
    for(int i=0;i< strlen(str);i++){
        if(str[i]==' '){
            space_counter++;
            if(space_counter>2){
                first_space=-1, second_space=-1;
                break;
            }
            if(first_space<0)
                first_space=i;
            else
                second_space=i;
        }
    }
    res[0]=first_space;
    res[1]=second_space;
}
void send_error(int code, int fd, char* path){
    char res[2048];
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    int len = 0;
    if(code==400) {
        strcpy(res, "HTTP/1.1 400 Bad Request\r\nServer: webserver/1.0\r\nDate: ");
        strcat(res, timebuf);
        strcat(res, "\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n");
        strcat(res,
               "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\r\nBad Request.\r\n</BODY></HTML>");
    }
    else if(code==501){
        strcpy(res, "HTTP/1.1 501 Not supported\r\nServer: webserver/1.0\r\nDate: ");
        strcat(res, timebuf);
        strcat(res, "\r\nContent-Type: text/html\r\nContent-Length: 124\r\nConnection: close\r\n\r\n");
        strcat(res,
               "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\nMethod is not supported.\r\n</BODY></HTML>");
    }
    else if(code==403){
        strcpy(res, "HTTP/1.1 403 Forbidden\r\nServer: webserver/1.0\r\nDate: ");
        strcat(res, timebuf);
        strcat(res, "\r\nContent-Type: text/html\r\nContent-Length: 109\r\nConnection: close\r\n\r\n");
        strcat(res,
               "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>");
    }
    else if(code==404){
        strcpy(res, "HTTP/1.1 404 Not Found\r\nServer: webserver/1.0\r\nDate: ");
        strcat(res, timebuf);
        strcat(res, "\r\nContent-Type: text/html\r\nContent-Length: 110\r\nConnection: close\r\n\r\n");
        strcat(res,
               "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile Not Found.\r\n</BODY></HTML>");
    }
    else if(code==500){
        strcpy(res, "HTTP/1.1 500 Internal Server Error\r\nServer: webserver/1.0\r\nDate: ");
        strcat(res, timebuf);
        strcat(res, "\r\nContent-Type: text/html\r\nContent-Length: 109\r\nConnection: close\r\n\r\n");
        strcat(res,
               "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>");
    }
    else if(code==302){
        strcpy(res, "HTTP/1.1 302 Found\r\nServer: webserver/1.0\r\nDate: ");
        strcat(res, timebuf);
        strcat(res, "\r\nLocation: ");
        strcat(res , path);
        strcat(res, "/\r\n");
        strcat(res, "Content-Type: text/html\r\nContent-Length: 121\r\nConnection: close\r\n\r\n");
        strcat(res,
               "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n<BODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>");
    }
    else{
        return;
    }
    int nbytes;
    if((nbytes = write(fd, res, strlen(res))) < 0) {
        perror("write");
        return;
    }
}
int dir_exist(char* path){
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return 1;
    }
    else if (ENOENT == errno) {
        return 0;
    }
    else {
        //perror("opendir");
        return 0;
    }
}
int path_exists(const char *fname){
    FILE *file;
    if ((file = fopen(fname, "r"))){
        fclose(file);
        return 1;
    }
    return 0;
}
int is_file(char* path){
    if(path_exists(path) && dir_exist(path))
        return 0;
    return 1;
}
char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}
int have_index_html(char* path){
    char* new_path = (char*)malloc((strlen(path)+11)*sizeof(char));
    strcpy(new_path,path);
    strcat(new_path, "index.html");
    if(path_exists(new_path)){
        free(new_path);
        return 1;
    }
    free(new_path);
    return 0;
}
char* make_headers(size_t content_len, char* file_type, char* last_modified){
    char* headers = (char*)malloc(1024*sizeof(char));
    if(!headers)
        return NULL;
    time_t now;
    char timebuf[128];
    char* html;
    int free_html=0;
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    strcpy(headers,"HTTP/1.1 200 OK\r\nServer: webserver/1.0\r\nDate: ");
    strcat(headers, timebuf);
    char* content_len_str = (char*)malloc(sizeof(char)*(get_num_of_digit(content_len)+1));
    if(!content_len_str)
        return NULL;
    sprintf(content_len_str, "%ld", content_len);
    if(file_type) {
        strcat(headers, "\r\nContent-Type: ");
        strcat(headers, file_type);
    }
    strcat(headers,"\r\nContent-Length: ");
    strcat(headers, content_len_str);
    free(content_len_str);
    strcat(headers, "\r\nLast-Modified: ");
    strcat(headers, last_modified);
    strcat(headers, "\r\nConnection: close\r\n\r\n");
    return headers;
}
char* make_dir_html(char* absPath){
    int html_size = 2*strlen(absPath)+1024;
    char path[PATH_MAX], cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))){
        perror("getcwd");
        return NULL;
    }
    left_shift_string(absPath, path, strlen(cwd));
    char* html = (char*)malloc(html_size*sizeof(char));
    if(!html)
        return NULL;
    strcpy(html,"<HTML>\n<HEAD><TITLE>Index of ");
    strcat(html, path);
    strcat(html,"</TITLE></HEAD>\n<BODY>\n<H4>Index of ");
    strcat(html, path);
    strcat(html, "</H4>\n<table CELLSPACING=8>\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\n");
    DIR *d;
    struct dirent *dir={0};
    d = opendir(absPath);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            char absPathFile[PATH_MAX], folderPath[PATH_MAX];
            strcpy(absPathFile, absPath);
            //printf("1)absPath= %s\n",absPath);
            //strcat(absPath,"/");
            strcat(absPathFile, dir->d_name);
            struct stat st={0};
            stat(absPathFile, &st);
            strcat(html, "<tr>\n<td><A HREF=");
            //printf("absPath= %s\n",absPath);
            if(is_file(absPathFile))
                strcat(html, dir->d_name);
            else{
                left_shift_string(absPathFile,folderPath,strlen(absPath));
                strcat(html, folderPath);
                strcat(html,"/");
            }
            strcat(html, ">");
            strcat(html, dir->d_name);
            strcat(html, "</A></td><td>");
            char timebuf[128]={};
            strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&st.st_mtime));
            strcat(html, timebuf);
            strcat(html, "</td>\n<td>");
            if(is_file(absPath)) {
                size_t size = st.st_size;
                char size_str[21];
                sprintf(size_str, "%ld", size);
                strcat(html, size_str);
            }
            strcat(html,"</td>\n</tr>");
            html_size+= strlen(absPath)+ strlen(dir->d_name)+128;
            html = (char*) realloc(html,html_size*sizeof(char));
            if(!html){
                closedir(d);
                return NULL;
            }
        }
        //printf("________________________________________\n");
        closedir(d);
        strcat(html, "</table>\n<HR>\n<ADDRESS>webserver/1.0</ADDRESS>\n</BODY></HTML>");
        return html;
    }
    return NULL;
}
int get_num_of_digit(int num){
    int counter = 0;
    while(num>0){
        num/=10;
        counter++;
    }
    return counter;
}
int have_execute_permission(char* path){
    struct stat fs;
    int r;
    r = stat(path,&fs);
    if( r==-1 )
    {
        perror("getstat");
        return -1;
    }
    if(fs.st_mode & S_IXOTH)
        return 1;
    return 0;
}
int have_execute_permission_recursive(char* path){
    if(!strlen(path))
        return 1;
    if(have_execute_permission(path)==-1)
        return -1;
    if(have_execute_permission(path)==0)
        return 0;
    int i;
    char newPath[512];
    strcpy(newPath,path);
    for(i= strlen(newPath);i>=0;i--){
        if(newPath[i]=='/'){
            newPath[i]='\0';
            break;
        }
    }
    return have_execute_permission_recursive(newPath);
}
int have_read_permission(char *path){
    struct stat fs;
    int r;
    r = stat(path,&fs);
    if(r==-1){
        perror("open file");
        return -1;
    }
    if( fs.st_mode & S_IROTH )
        return 1;
    return 0;
}
char* parent_path(char* path){
    char* res = (char*)malloc(sizeof(char)*(1+strlen(path)));
    if(!res)
        return NULL;
    strcpy(res,"");
    if(strlen(path)==0)
        return res;
    strcat(res, path);
    int i;
    for(i= strlen(res);i>=0;i--)
        if(res[i]=='/'){
            res[i]='\0';
            break;
        }
    if(!i)
        strcpy(res,"");
    return res;
}
int send_file(int fd, char* path){
    struct stat fs;
    stat(path, &fs);
    size_t file_size = fs.st_size;
    char last_modified[128];
    strftime(last_modified, sizeof(last_modified), RFC1123FMT, gmtime(&fs.st_mtime));
    char* headers = make_headers(file_size,get_mime_type(path), last_modified);
    int to_send = open(path, O_RDONLY);
    if(to_send<0){
        free(headers);
        return -1;
    }
    write(fd,headers, strlen(headers));
    free(headers);
    unsigned char buff[4096]={};
    while(read(to_send,buff,sizeof(buff))){
        write(fd,buff,sizeof(buff));
    }
    close(to_send);
    return 0;
}
int send_dynamic_html(int fd, char* path){
    char* html = make_dir_html(path);
    size_t file_size = strlen(html);
    time_t now = time(NULL);
    char last_modified[128];
    strftime(last_modified, sizeof(last_modified), RFC1123FMT, gmtime(&now));
    char* headers = make_headers(file_size,get_mime_type(path), last_modified);
    write(fd,headers, strlen(headers));
    write(fd, html, strlen(html));
    free(headers);
    free(html);
    return 0;
}
void left_shift_string(char* str,char* dest, int shift){
    if(shift> strlen(str))
        return;
    strcpy(dest,str);
    int i = shift;
    for(;i< strlen(dest);i++)
        dest[i-shift] = dest[i];
    dest[i-shift]='\0';
}