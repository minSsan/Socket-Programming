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

#define BACKLOG 10 /* pending connections queue의 길이 */
#define BUFFER_SIZE 1024

#define HEADER_FORMAT "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n" /* status code | status text | 리소스 크기 | content type 순으로 입력 */
#define NOTFOUND_CONTENT "<h1>404: NOT FOUND</h1>"
#define SERVER_ERR_CONTENT "<h1>500: Internal Server Error</h1>"

char content_type[50]; // content-type value
char header[BUFFER_SIZE]; // header

// * 참고: https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
// ? find_contentType 호출 후에 content-type 값이 content_type 변수에 저장된다.
void find_contentType(char* uri) {
    char* extension = strrchr(uri, '.'); // strrchr: https://www.ibm.com/docs/ko/i/7.3?topic=functions-strrchr-locate-last-occurrence-character-in-string

    if (strcmp(extension, ".html") == 0) {
        strcpy(content_type, "text/html");
    } else if (strcmp(extension, ".png") == 0) {
        strcpy(content_type, "image/png");
    } else if (strcmp(extension, ".gif") == 0) {
        strcpy(content_type, "image/gif");
    } else if (strcmp(extension, ".jpeg") == 0 || strcmp(extension, ".jpg") == 0) {
        strcpy(content_type, "image/jpeg");
    } else if (strcmp(extension, ".pdf") == 0) {
        strcpy(content_type, "application/pdf");
    } else if (strcmp(extension, ".mp3") == 0) {
        strcpy(content_type, "audio/mpeg"); // 다운로드: https://stackoverflow.com/questions/12017694/content-type-for-mp3-download-response
    } else {
        strcpy(content_type, "text/plain");
    }
}

// * 참고: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
// ? make_header 호출 후에 헤더 내용이 header 변수에 저장된다.
void make_header(int status_code, long content_length, char* uri) {
    /* 헤더를 완성하기 위해 먼저 uri에 해당되는 content-type을 찾는다. */
    find_contentType(uri);

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
    sprintf(header, HEADER_FORMAT, status_code, status_text, content_length, content_type);
}

// ? status_code에 대응되는 에러 response를 클라이언트에게 보낸다.
void send_error(int fd, int status_code) {
    switch (status_code)
    {
        case 404:
            make_header(status_code, sizeof(NOTFOUND_CONTENT), "text/html");
            write(fd, header, strlen(header));
            write(fd, NOTFOUND_CONTENT, sizeof(NOTFOUND_CONTENT));
            break;

        case 500:
        default:
            make_header(500, sizeof(SERVER_ERR_CONTENT), "text/html");
            write(fd, header, strlen(header));
            write(fd, SERVER_ERR_CONTENT, sizeof(SERVER_ERR_CONTENT));
            break;
    }
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
        perror("[ERROR] bind error:");
        exit(1);
    }

    /* 클라이언트의 연결 요청(connect)을 listen */
    listen(socket_fd, BACKLOG);

    client_len = sizeof(struct sockaddr_in);
    // *************************************
    
    /* 클라이언트의 연결 요청을 수신하면 accept하여 통신 소켓 생성 -> 반복 */
    while (1) {
        printf("[INFO] listenning...\n");
        new_socket_fd = accept(socket_fd, (struct sockaddr *) &client_address, &client_len);
        if (new_socket_fd < 0) {
            perror("[ERROR] accept error:");
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
            perror("[ERROR] read error:");
            // 500 error
            send_error(new_socket_fd, 500);
            close(new_socket_fd);
            continue;
        }
        printf("[INFO] message is\n%s\n", buffer);


        // ************* method와 file 파싱 *************
        // ex. GET / HTTP/1.1 (파일 경로를 지정하지 않은 경우)
        char* method = strtok(buffer, " "); // 메소드 명 -> "GET"
        char* uri = strtok(NULL, " "); // / HTTP/1.1을 공백 기준 tok = 요청 파일 -> "/"

        if (method == NULL || uri == NULL) {
            // TODO: 400 error
        }

        printf("[INFO] handling method: %s, uri: %s\n", method, uri);
        /* uri text를 저장하기 위한 char 배열 */
        char uri_str[BUFFER_SIZE];
        strcpy(uri_str, uri);

        // 파일 경로 없이 (IP + Port)만 명시한 경우 -> index.html로 대체
        if (strcmp(uri_str, "/") == 0) {
            // 기본 html 파일을 띄운다.
            strcpy(uri_str, "/index.html");
        }
        // ****************************************


        // *************** File 조회 ***************
        // 파일 정보 조회를 실패한 경우
        if (stat(uri_str+1, &sb) < 0) {
            perror("[ERROR] No File Matching with URI:");
            // 404 error
            send_error(new_socket_fd, 404);
            close(new_socket_fd);
            continue;
        }
        // ****************************************


        // *************** File 열기 ***************
        int fd;
        if ((fd = open(uri_str+1, O_RDONLY)) < 0) {
            perror("[ERROR] Failed To Open File:");
            // 500 error
            send_error(new_socket_fd, 404);
            close(new_socket_fd);
            continue;
        }
        // ****************************************


        // // *********** Find Content Type ***********
        // find_contentType(uri_str+1);
        // // *****************************************


        // ************* Make Header *************
        make_header(200, sb.st_size, uri_str+1);
        // ***************************************


        // ************ Send Response ************
        /* send header */
        write(new_socket_fd, header, strlen(header));

        /* send body */
        int read_n;
        char file_buffer[BUFFER_SIZE];
        bzero((char *) &file_buffer, sizeof(file_buffer));
        while ((read_n = read(fd, file_buffer, BUFFER_SIZE)) > 0) {
            write(new_socket_fd, file_buffer, read_n);
        }
        // ***************************************

        /* 읽어온 파일을 닫는다 */
        close(fd);
        /* 현재 소켓은 닫는다 */
        close(new_socket_fd);
    }

    return 0;
}

