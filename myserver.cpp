#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <string>
#include <unistd.h>
#define BACKLOG 10 /* pending connections queue의 길이 */

/*
 * part A: 
*/

int main(int argc, char *argv[]) {
    int socket_fd, new_socket_fd; 
    int port_num;
    socklen_t client_len;

    /* 
     * struct sockaddr_in:  주소를 저장하기 위한 구조체
     * server_address:      서버의 주소를 저장
     * client_address:      클라이언트의 주소를 저장
     */
    struct sockaddr_in server_address, client_address;

    char buffer[1024];
    bzero((char *) &buffer, sizeof(buffer)); // 버퍼를 0으로 초기화

    if (argc < 2) {
        fprintf(stderr, "ERROR: no port number");
        exit(1);
    }

    port_num = atoi(argv[1]);

    /*
     * AF_INET: 주소 형식 - IPv4 (Address Domain is Internet)
     * SOCK_STREAM: TCP
    */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR - socket error:");
        exit(1);
    }

    /* 서버 주소 정보 등록 */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    // 포트번호: 호스트 바이트 순서 -> 네트워크 바이트 순서로 통일하기 위함
    server_address.sin_port = htons(port_num);
    
    if (bind(socket_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("ERROR - bind error:");
        exit(1);
    }

    listen(socket_fd, BACKLOG);
    std::cout << "listenning...\n";

    client_len = sizeof(client_address);
    new_socket_fd = accept(socket_fd, (struct sockaddr *) &client_address, &client_len);
    std::cout << "accept!\n";
    if (new_socket_fd < 0) {
        perror("ERROR - accept error:");
        exit(1);
    }

    /* read_n: 읽어들인 byte 수 */
    if (read(new_socket_fd, buffer, 1023) < 0) { // accept한 소켓에서 255 byte 만큼의 데이터를 읽고 buffer에 저장
        perror("ERROR - read error:");
        exit(1);
    }
    std::cout << "message: " << buffer << '\n';

    close(socket_fd);
    close(new_socket_fd);

    return 0;
}