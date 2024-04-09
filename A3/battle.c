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
#define INITIAL_SET_SIZE 32

char* elements[] = {"fire", "water", "wind", "blood"};
char* reg_moves[] = {"flame slash", "water cut", "wind slice", "blood stab"};
char* spec_moves[] = {"Wolf-Rayet star WR 102", '15,750 psi!!', "Let us not burthen our remembrance with / A heaviness thats gone.", "holy grail."};

struct player {
    int element;
};

typedef struct { // 16-byte alligned structure (size_t is 8 bytes)
    size_t capacity;
    size_t length;
} Dynamic_Set_Header; // the meta data for a dynamic integer set. we will call an a dynamic integer set "iset" from now on.

int* iset_init() { // initialize an iset
    int* ptr = NULL;
    size_t size = sizeof(Dynamic_Set_Header) + INITIAL_SET_SIZE * sizeof(int);
    Dynamic_Set_Header* h = (Dynamic_Set_Header*) malloc(size);

    if (h) {
        h->capacity = INITIAL_SET_SIZE;
        h->length = 0;
        ptr = (int*)(h + 1); // we skip over the meta data, and now point to the beginning of the iset
    }

    memset(ptr, -1, INITIAL_SET_SIZE * sizeof(int)); // set all empty indices to -1

    return ptr; // return our iset. type of iset is int*
}

Dynamic_Set_Header* iset_header(int* iset) { // returns the header for the iset
    return (Dynamic_Set_Header*)(iset) - 1; // we move back "Dynamic_Set_Header" bits from the front of the set. this gives us the address of the header.
}

size_t iset_length(int* iset) { // returns the length of the iset
    return iset_header(iset)->length;
}

size_t iset_length(int* iset) { // returns the capacity of the iset
    return iset_header(iset)->capacity;
}

int _iset_index(int* iset, int val) { // returns the index of a value in an iset, returns -1 if not present. this is a helper for iset_remove()
    size_t len = iset_length(iset);
    int index = -1;
    for (int i = 0; i < len; i++) {
        if (iset[i] == val) {
            index = i;
            break;
        }
    }
    return index;    
}

void iset_remove(int* iset, int val) { // returns the value from the iset
    int index = _iset_index(iset, val); 


}

void iset_addnew(int* iset) {
    // important prerequisite: we are assuming that the value we seek to add is not already in the set
    // we make this assumption because it saves us time from checking, and the nature of this program is only expected to add unique integers in the first place.

    
}

void smart_select(fd_set* rd, fd_set* wr, fd_set* mrd, fd_set* mwr) {
    // call select()

    
}


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

    // use select() to avoid blocking with accept()
    fd_set readfds;
    FD_ZERO (&readfds);
    fd_set writefds;
    FD_ZERO (&writefds);
    fd_set filtered_readfds;
    FD_ZERO (&filtered_readfds);
    fd_set filtered_writefds;
    FD_ZERO (&filtered_writefds);

    while (1) {

        
        int client_soc = accept(soc, (struct sockaddr*) &client_addr, &client_len);
        if (client_soc == -1) {
            perror("accept");
            exit(1);
        }
    }

    return 0;
}