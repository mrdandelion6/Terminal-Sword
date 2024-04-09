#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // fork()
#include <errno.h> // perrror()
#include <fcntl.h> // open(), close(), dup2()
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/select.h>

#define PORT 55976 
#define MAXSIZE 4096

char* elements[] = {"fire", "water", "wind", "blood"};
char* reg_moves[] = {"flame slash", "water cut", "wind slice", "blood stab"};
char* spec_moves[] = {"Wolf-Rayet star WR 102", '15,750 psi!!', "Let us not burthen our remembrance with / A heaviness thats gone.", "holy grail."};

struct player {
    int element;
};

int main() {
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr; 
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    memset(&(server_addr.sin_zero), 0, 8);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if ( bind(soc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        exit(1);
    }
    
    if (listen(soc, 5) == -1) {
        perror("listen");
        exit(1);
    }

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    __u_int client_len = sizeof(struct sockaddr_in);

    int client_soc = accept(soc, (struct sockaddr*) &client_addr, &client_len);
    if (client_soc == -1) {
        perror("accept");
        exit(1);
    }

    return 0;
}