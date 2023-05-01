#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* constant */
#define BACKLOG 10 /* pending connections queue의 길이 */
#define BUFFER_SIZE 1024

/* HTTP Response header format */
#define HEADER_FORMAT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n" /* status code | status text | 리소스 크기 | content type 순으로 입력 */

/* error page filenames */
#define NOTFOUND_FILENAME "notfound.html"
#define SERVER_ERR_FILENAME "server_error.html" 
#define BAD_REQUEST_FILENAME "bad_request.html"

/* response header */
char content_type[50];      // content-type value
char header[BUFFER_SIZE];   // header

/* functions */
void set_contentType(char* uri);
void make_header(int status_code, long content_lenght);
void send_error(int fd, int status_code);
void send_HTTP_response(int socket_fd, char* filename);

// 참고: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
// ? set_contentType 호출 후에 content-type 값이 content_type 변수에 저장된다.
void set_contentType(char* uri) {
    /* 확장자 추출 */
    char* extension = strrchr(uri, '.'); // strrchr: https://www.ibm.com/docs/ko/i/7.3?topic=functions-strrchr-locate-last-occurrence-character-in-string

    /* .html file */
    if (strcmp(extension, ".html") == 0) {
        strcpy(content_type, "text/html");
    } 
    /* .png file */
    else if (strcmp(extension, ".png") == 0) {
        strcpy(content_type, "image/png");
    } 
    /* .gif file */
    else if (strcmp(extension, ".gif") == 0) {
        strcpy(content_type, "image/gif");
    } 
    /* .jpeg .jpg file */
    else if (strcmp(extension, ".jpeg") == 0 || strcmp(extension, ".jpg") == 0) {
        strcpy(content_type, "image/jpeg");
    } 
    /* .pdf file */
    else if (strcmp(extension, ".pdf") == 0) {
        strcpy(content_type, "application/pdf");
    } 
    /* .mp3 file */
    else if (strcmp(extension, ".mp3") == 0) {
        strcpy(content_type, "audio/mpeg");
    } 
    /* other */
    else {
        strcpy(content_type, "text/plain");
    }
}

// 참고: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
// ? make_header 호출 후에 헤더 내용이 header 변수에 저장된다.
void make_header(int status_code, long content_lenght) {
    /* status code에 알맞는 status 문구를 결정한다. */
    char status_text[50];

    switch (status_code)
    {
        case 200:
            strcpy(status_text, "OK");
            break;

        case 400:
            strcpy(status_text, "Bad Request");
        
        case 404:
            strcpy(status_text, "Not Found");
            break;

        case 500:
        default:
            strcpy(status_text, "Internal Server Error");
            break;
    }

    /* status code/text, content-type, content-length을 조합하여 헤더를 완성한다. */
    sprintf(header, HEADER_FORMAT, status_code, status_text, content_lenght, content_type);
}

// ? status_code에 대응되는 에러 response를 클라이언트에게 보낸다.
// FILENAME을 잘못 설정한 경우(recursion 발생 가능성)를 대비하여 send_HTTP_response를 사용하지 X
// FILENAME을 잘못 설정하더라도 status code를 담은 헤더를 전송하기 때문에, 브라우저 기본 제공 에러 화면을 띄울 수 있다.
void send_error(int fd, int status_code) {
    struct stat st;
    int file_fd, read_n;
    char file_buff[BUFFER_SIZE];
    
    switch (status_code)
    {
        case 400:
            if (stat(BAD_REQUEST_FILENAME, &st) < 0) {
                perror("[ERROR] Bad 400 Filename");
            }
            if ((file_fd = open(BAD_REQUEST_FILENAME, O_RDONLY)) < 0) {
                perror("[ERROR] Failed To Open 400 Custom File");
            }
            /* 커스텀 에러 화면을 불러오는데 실패한 경우에도 status code를 담은 헤더를 보냄 */
            strcpy(content_type, "text/html");
            make_header(400, st.st_size); 
            write(fd, header, strlen(header));
            while ((read_n = read(file_fd, file_buff, BUFFER_SIZE)) > 0) {
                write(fd, file_buff, read_n);
            } 
            break;

        case 404:
            if (stat(NOTFOUND_FILENAME, &st) < 0) {
                perror("[ERROR] Bad 404 Filename");
            }
            if ((file_fd = open(NOTFOUND_FILENAME, O_RDONLY)) < 0) {
                perror("[ERROR] Failed To Open 404 Custom File");
            }
            /* 커스텀 에러 화면을 불러오는데 실패한 경우에도 status code를 담은 헤더를 보냄 */
            strcpy(content_type, "text/html");
            make_header(404, st.st_size);
            write(fd, header, strlen(header));
            while ((read_n = read(file_fd, file_buff, BUFFER_SIZE)) > 0) {
                write(fd, file_buff, read_n);
            } 
            break;

        case 500:
        default:
            if (stat(SERVER_ERR_FILENAME, &st) < 0) {
                perror("[ERROR] Bad 500 Filename");
            }
            if ((file_fd = open(SERVER_ERR_FILENAME, O_RDONLY)) < 0) {
                perror("[ERROR] Failed To Open 500 Custom File");
            }
            /* 커스텀 에러 화면을 불러오는데 실패한 경우에도 status code를 담은 헤더를 보냄 */
            strcpy(content_type, "text/html");
            make_header(500, st.st_size);
            write(fd, header, strlen(header));
            while ((read_n = read(file_fd, file_buff, BUFFER_SIZE)) > 0) {
                write(fd, file_buff, read_n);
            } 
            break;
    }
}

// ? 지정 소켓을 통해 filename에 대한 HTTP response를 전송하는 함수
void send_HTTP_response(int socket_fd, char* filename) {
    // *************** File 조회 ***************
    /* struct stat
    * 파일 정보를 얻기 위함
    * content-length를 구할 때도 활용 가능
    * 참고: https://www.it-note.kr/173
    */
    struct stat st;
    // 파일 정보 조회를 실패한 경우
    if (stat(filename, &st) < 0) {
        perror("[ERROR] No File Matching With URI: ");
        // 404 error
        send_error(socket_fd, 404);
        return ;
    }
    // ****************************************


    // *************** File 열기 ***************
    int file_fd;
    if ((file_fd = open(filename, O_RDONLY)) < 0) {
        perror("[ERROR] Failed To Open File:");
        // 500 error
        send_error(socket_fd, 500);
        return ;
    }
    // ****************************************


    // *********** Find Content Type ***********
    set_contentType(filename); // content-type value 결정
    // *****************************************


    // ************** Make Header **************
    make_header(200, st.st_size); // header value 결정
    // *****************************************


    // ************* Send Response *************
    /* send header */
    write(socket_fd, header, strlen(header));
        
    /* send body */
    int read_n; /* 읽어들인 byte 수 */
    char file_buffer[BUFFER_SIZE];
    bzero((char *) &file_buffer, sizeof(file_buffer));
    while ((read_n = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        write(socket_fd, file_buffer, read_n);
    }
    // *****************************************

    /* 읽어온 파일을 닫는다 */
    close(file_fd);
}

int main(int argc, char *argv[]) {
    /*
     * socket_fd            연결을 위한 소켓
     * new_socket_fd        데이터 송수신을 위한 소켓
     * port_num             클라이언트와 통신할 포트번호
     * struct sockaddr_in   주소를 저장하기 위한 구조체
     * server_address       서버의 주소를 저장
     * client_address       클라이언트의 주소를 저장
     * buffer               요청 메시지를 저장할 버퍼
     */
    int socket_fd, new_socket_fd; 
    int port_num;
    socklen_t client_len;
    struct sockaddr_in server_address, client_address;
    char buffer[BUFFER_SIZE];

    // ************ 포트 번호 저장 ************
    if (argc < 2) {
        fprintf(stderr, "[ERROR] no port number");
        exit(1);
    }
    port_num = atoi(argv[1]);
    // ************************************


    // ************** 소켓 연결 **************
    /*
     * AF_INET: 주소 형식 - IPv4 (Address Domain is Internet)
     * SOCK_STREAM: TCP
    */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[ERROR] socket error:");
        exit(1);
    }

    /* 서버 주소 정보 등록 */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port_num); // htons: 호스트 바이트 순서 -> 네트워크 바이트 순서로 설정
    
    /* 소켓에 IP + PORT를 bind */
    if (bind(socket_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("[ERROR] bind error");
        exit(1);
    }

    /* 클라이언트의 연결 요청(connect)을 listen */
    listen(socket_fd, BACKLOG);

    client_len = sizeof(struct sockaddr_in);
    // *************************************
    
    /* 클라리언트의 연결 요청을 수신하면 accept하여 통신 소켓 생성 -> 반복 */
    while (1) {
        printf("[INFO] listenning...\n");
        new_socket_fd = accept(socket_fd, (struct sockaddr *) &client_address, &client_len);
        if (new_socket_fd < 0) {
            perror("[ERROR] accept error");
            continue;
        }

        bzero((char *) &buffer, sizeof(buffer)); // 버퍼를 0으로 초기화

        /* struct stat
         * 파일 정보를 얻기 위함
         * content-length를 구할 때도 활용 가능
         * 참고: https://www.it-note.kr/173
         */
        struct stat sb;

        /* read_n: 읽어들인 byte 수 */
        if (read(new_socket_fd, buffer, BUFFER_SIZE) < 0) { // accept한 소켓에서 데이터를 읽고 buffer에 저장
            perror("[ERROR] read error");
            // 500 error
            send_error(new_socket_fd, 500);
            close(new_socket_fd);
            continue;
        }
        printf("[INFO] Client Message is\n%s\n", buffer);

        // ex. GET / HTTP/1.1 (파일 경로를 지정하지 않은 경우)
        char* method = strtok(buffer, " "); // 메소드 명 -> "GET"
        char* uri = strtok(NULL, " "); // / HTTP/1.1을 공백 기준 tok = 요청 파일 -> "/"

        /* Request 형식이 잘못된 경우 */
        if (method == NULL || uri == NULL) {
            perror("[ERROR] Bad Request");
            // 400 error
            send_error(new_socket_fd, 400);
            close(new_socket_fd);
            continue;
        }

        printf("[INFO] method: %s, uri: %s\n", method, uri);
        /* uri text를 저장하기 위한 char 배열 */
        char uri_str[BUFFER_SIZE];
        strcpy(uri_str, uri);

        /* 파일 경로 없이 (IP + Port)만 명시한 경우 -> index.html로 대체 */
        if (strcmp(uri_str, "/") == 0) {
            // 기본 html 파일을 띄운다.
            strcpy(uri_str, "/index.html");
        }

        /* client에게 HTTP response를 보낸다 */
        send_HTTP_response(new_socket_fd, uri_str+1);

        /* 현재 소켓은 닫는다 */
        close(new_socket_fd);
    }

    return 0;
}