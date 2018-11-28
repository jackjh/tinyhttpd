#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>

typedef unsigned short u_short;

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void errorDie(const char* s);
int startup(u_short* port);
int getLine(int sockfd, char* buf, int size);
void *acceptRequest(void* ptClient);
void unImplemented(int client);
void notFound(int client);
void serverFile(int client, const char* path);
void executeCgi(int client, const char* path, const char* method, const char* query_string);
void headers(int client, const char* path);
void cat(int client, FILE* fileResource);
void cannotExecute(int client);
void badRequest(int client);

void errorDie(const char* s) {
    perror(s);
    exit(1);
}

void unImplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 method not implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<HTML><HEAD><TITLE>method not implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

// start up a sever
int startup(u_short* port) {
    int httpd = 0;
    struct sockaddr_in name_;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if(httpd == -1) {
        errorDie("socket error");
    }

    memset(&name_, 0, sizeof(name_));
    name_.sin_family = AF_INET;
    name_.sin_port = htons(*port);
    name_.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(httpd, (struct sockaddr*)&name_, sizeof(name_)) < 0) {
        errorDie("bind error");
    }

    if(*port == 0) {
        socklen_t name_len = sizeof(name_);
        if(getsockname(httpd, (struct sockaddr*)&name_, &name_len) == -1) {
            errorDie("getsockname error");
        }
        *port = ntohs(name_.sin_port);
    }
    if(listen(httpd, 5) < 0) {
        errorDie("listen error");
    }
    return httpd;
}

// accept a request from the client
void* acceptRequest(void* ptClient) {
    int client = *(int*)ptClient;
    char buf[1024];
    char method[255];
    char url[255];
    char path[255];
    int numChars;
    size_t i, j;
    struct stat st;

    int cgi = 0;

    char* query_string = NULL;

    // read the first line of http request
    numChars = getLine(client, buf, sizeof(buf));

    // get the method of the http request
    i = 0;
    j = 0;
    while(!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
        method[i] = buf[j];
        i++;
        j++;
    }
    method[i] = '\0';

    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        unImplemented(client);
        return NULL;
    }

    if(strcasecmp(method, "POST") == 0) {
        cgi = 1;
    }

    while(ISspace(buf[j]) && (j < sizeof(buf))) {
        j++;
    }
    
    i = 0;
    while(!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

    if(strcasecmp(method, "GET") == 0) {
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0')) {
            query_string++;
        }
        if(*query_string == '?') {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdocs%s", url);
    if(path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");
    }

    if(stat(path, &st) == -1) {
        while((numChars > 0) && strcmp("\n", buf)) {
            numChars = getLine(client, buf, sizeof(buf));
        }
        notFound(client);
    }
    else {
        if((st.st_mode & S_IFMT) == S_IFDIR) {
            strcat(path, "/index.html");
        }
        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) {
            cgi = 1;
        }

        if(!cgi) {
            serverFile(client, path);
        }
        else {
            executeCgi(client, path, method, query_string);
        }
    }

    return NULL;
}

// get a line from the sockfd
int getLine(int sockfd, char* buf, int size) {
    int i = 0;
    char c = '\0';

    int n;
    while((i < size - 1) && (c != '\n')) {
        n = recv(sockfd, &c, 1, 0);
        if(n > 0) {
            if(c == '\r') {
                n = recv(sockfd, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n'))
                    recv(sockfd, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else {
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

void serverFile(int client, const char* path) {
    //printf("path = %s\n", path);
    FILE* resource = NULL;
    int numChars = 1;
    char buf[1024];

    buf[0] = 'A';
    buf[1] = '\0';

    while((numChars > 0) && strcmp(buf, "\n")) {
        numChars = getLine(client, buf, sizeof(buf));
    }

    resource = fopen(path, "r");
    if(resource == NULL) {
        notFound(client);
    }
    else {
        // send header to client
        headers(client, path);

        // send body data to client
        cat(client, resource);
    }

    fclose(resource);
}

// send the part of header to client
void headers(int client, const char* path) {
    char buf[1024];
    (void)path;

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

// send the part of body to client
void cat(int client, FILE* fileResource) {
    char buf[1024];
    fgets(buf, sizeof(buf), fileResource);

    while(!feof(fileResource)) {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), fileResource);
    }
}

void executeCgi(int client, const char* path, const char* method, const char* query_string) {
    char buf[1024];
    int cgi_input[2];
    int cgi_output[2];
    pid_t p_id;
    int status;
    int numChars = 1;
    int content_length = -1;
    char c;
    int i;

    buf[0] = 'A';
    buf[1] = '\0';
    if(strcasecmp(method, "GET") == 0) {
        while((numChars > 0) && strcmp("\n", buf)) {
            numChars = getLine(client, buf, sizeof(buf));
        }
    }
    else {
        // POST
        numChars = getLine(client, buf, sizeof(buf));
        while((numChars > 0) && strcmp(buf, "\n")) {
            buf[15] = '\0';
            if(strcasecmp(buf, "Content-Length:") == 0) {
                content_length = atoi(&buf[16]);
            }
            numChars = getLine(client, buf, sizeof(buf));
        }
        if(content_length == -1) {
            badRequest(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if(pipe(cgi_input) < 0) {
        cannotExecute(client);
        return;
    }
    if(pipe(cgi_output) < 0) {
        cannotExecute(client);
        return;
    }

    if((p_id = fork()) < 0) {
        cannotExecute(client);
        return;
    }

    if(p_id == 0) {     // child pthread: CGI script
        char method_env[255];
        char query_env[255];
        char contLength_env[255];

        dup2(cgi_input[0], STDIN_FILENO);
        dup2(cgi_output[1], STDOUT_FILENO);
        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(method_env, "REQUEST_METHOD=%s", method);
        putenv(method_env);

        if(strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {
            sprintf(contLength_env, "CONTENT_LENGTH=%d", content_length);
            putenv(contLength_env);
        }
        execl(path, path, NULL);
        exit(0);
    }
    else {      // parent pthread
        close(cgi_output[1]);
        close(cgi_input[0]);

        if(strcasecmp(method, "POST") == 0) {
            for(i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }
        
        while(read(cgi_output[0], &c, 1) > 0) {
            send(client, &c, 1, 0);
        }

        close(cgi_input[1]);
        close(cgi_output[0]);
        waitpid(p_id, &status, 0);
    }
}

void notFound(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    //以下开始是body
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void cannotExecute(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Sever Error\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

void badRequest(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 Bad Request\r\n");
    send(client, buf, strlen(buf), 0);
    
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<P>The browser sent a bad request, ");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, strlen(buf), 0);
}

int main() {
    int server_sock = -1;
    int client_sock = -1;
    u_short port = 0;

    struct sockaddr_in client_name;
    socklen_t client_len = sizeof(client_name);

    pthread_t new_thread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while(1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_len);
        if(client_sock == -1) {
            errorDie("accept error");
        }

        if(pthread_create(&new_thread, NULL, acceptRequest, (void*)&client_sock) != 0) {
            perror("pthread_create error");
        }
    }

    close(server_sock);

    return 0;
}