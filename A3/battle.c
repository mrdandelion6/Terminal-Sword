#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // fork()
#include <errno.h> // perrror()
#include <fcntl.h> // open(), close(), dup2()
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/select.h>

// 55976, 46050
#define PORT 55976  
#define MAXSIZE 4096
#define INITIAL_SET_SIZE 32
#define MAX_MESSAGE_LENGTH 256
#define MAX_USER_LENGTH 50

// prototypes
void accept_client(int soc);
void check_wait();
void handle_read(int fd);
void handle_write(int fd);
void player_init(int fd);
void remove_newlines(char *str);
void join_msg(int fd, char* name);

typedef struct player { // 16-bit aligned
    char user[32]; // 32
    int element; // 32
    int foe; // 32
    char buffer[MAX_MESSAGE_LENGTH]; // 256
} Player; 

typedef struct { // 16-byte alligned structure (size_t is 8 bytes)
    size_t capacity; // 64
    size_t length; // 64
    size_t elem_size; // 64
} Dynamic_Set_Header; // the meta data for a dynamic integer set. we will call an a dynamic integer set "iset" from now on.

#define size_mac(T) sizeof(T)

int* iset_init(const char* T) { // initialize an iset
    int* ptr = NULL;
    size_t size = sizeof(Dynamic_Set_Header) + INITIAL_SET_SIZE * size_mac(T);
    Dynamic_Set_Header* h = (Dynamic_Set_Header*) malloc(size);

    if (h) {
        h->elem_size = size_mac(T);
        h->capacity = INITIAL_SET_SIZE;
        h->length = 0;
        ptr = (int*)(h + 1); // we skip over the meta data, and now point to the beginning of the iset
    }

    return ptr; // return our iset. type of iset is int*
}

Dynamic_Set_Header* iset_header(int* iset) { // returns the header for the iset
    return (Dynamic_Set_Header*)(iset) - 1; // we move back "Dynamic_Set_Header" bits from the front of the set. this gives us the address of the header.
}

size_t iset_size(void* iset) { // returns the size of element
    return iset_header(iset)->elem_size;
}

size_t iset_length(void* iset) { // returns the length of the iset
    return iset_header(iset)->length;
}

size_t iset_capacity(void* iset) { // returns the capacity of the iset
    return iset_header(iset)->capacity;
}

void _iset_change_length(void* iset, int add) { // changes the length of the iset by adding the add value. helper for iset_remove() and iset_add()
    int len = (int) iset_header(iset)->length;
    len += add;
    if (len < 0) {
        fprintf(stderr, "_iset_change_length: attempting to turn iset length into a negative value");
        exit(1);
    }

    iset_header(iset)->length = (size_t) len;
}

#define PLACE_PT(T, h) (T*)(h + 1)

void _iset_change_capacity(void** iset_ptr, int add) { // changes the capacity of the iset by adding the add value. helper for iset_remove() and iset_add()
    int cap = (int) (iset_header(*iset_ptr)->capacity);
    cap += add;
    if (cap < 0) {
        fprintf(stderr, "_iset_change_capacity: attempting to turn iset capacity into a negative value");
        exit(1);
    }

    size_t size = cap * iset_size(*iset_ptr) + sizeof(Dynamic_Set_Header);
    int* iset = *iset_ptr;
    Dynamic_Set_Header* h = iset_header(iset); // first we get back the header
    h = (Dynamic_Set_Header*) realloc(h, size); // now we make more space

    if (iset_size(*iset_ptr) == 1) {
        *iset_ptr = (int*)(h + 1);
    } else {
        *iset_ptr = (Player**)(h + 1);
    }
     // and lastly we change the place iset points to
    iset_header(*iset_ptr)->capacity = (size_t) cap;
}

int _iset_index_of(int* iset, int val) { // returns the index of a value in an iset, returns -1 if not present. this is a helper for iset_remove()
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

void iset_remove(int** iset_ptr, int val) { // removes the value from the iset
    // rmk: only to be called by a true integer set.
    if (val < 0) {
        fprintf(stderr, "iset_remove: attempting to remove a negative integer from the iset\n");
        exit(1);
    }

    int* iset = *iset_ptr;
    size_t len = iset_length(iset);
    if (len == 0) {
        fprintf(stderr, "iset_remove: attempting to remove an integer from an empty iset\n");
        exit(1);
    }

    int index = _iset_index_of(iset, val); 
    if (index == -1) {
        fprintf(stderr, "iset_remove: attempting to remove an integer not in the iset\n");
        exit(1);
    }

    size_t cap = iset_capacity(iset);
    int shrink = 0; // flag for whether we will shrink down the iset or not.
    if ( (cap - (len - 1) >= 32) && len != 1) { 
        shrink = 1;
    }

    iset[index] = iset[len - 1]; // replace the value we are removing with the last item
    _iset_change_length(iset, -1);

    if (shrink) { // we shrink down the iset if we need to
        int diff = len - 1 - cap;
        _iset_change_capacity( (void**)iset_ptr, diff);
    }
}

void pset_remove(Player*** pset_ptr, int index) { // remove player at a given index
    Player** pset = *pset_ptr;
    size_t len = iset_length(pset);
    if (len == 0) {
        fprintf(stderr, "pset_remove: attempting to remove a player from an empty iset\n");
        exit(1);
    }

    if (index < 0) {
        fprintf(stderr, "pset_remove: attempting to access negative index.");
        exit(1);
    }

    size_t cap = iset_capacity(pset);
    int shrink = 0; // flag for whether we will shrink down the iset or not.
    if ( (cap - (len - 1) >= 32) && len != 1) { 
        shrink = 1;
    }
    pset[index] = pset[len - 1]; // replace the value we are removing with the last item
    _iset_change_length(pset, -1);

    if (shrink) { // we shrink down the iset if we need to
        int diff = len - 1 - cap;
        _iset_change_capacity( (void**) pset_ptr, diff);
    }    
}

int iset_deque(int** iset_ptr) { // remove the first element in the iset and shift all elemenets back
    int* iset = *iset_ptr;
    size_t len = iset_length(iset);
    if (len == 0) {
        fprintf(stderr, "iset_remove: attempting to remove an integer from an empty iset\n");
        exit(1);
    }

    size_t cap = iset_capacity(iset);
    int shrink = 0; // flag for whether we will shrink down the iset or not.
    if ( (cap - (len - 1) >= 32) && len != 1) { 
        shrink = 1;
    }

    int val = iset[0]; // get topmost value
    for (int i = 0; i < len - 1; i++) {
        iset[i] = iset[i + 1];
    }

    _iset_change_length(iset, -1);

    if (shrink) { // we shrink down the iset if we need to
        int diff = len - 1 - cap;
        _iset_change_capacity( (void**) iset_ptr, diff);
    }

    return val;
}

void iset_addnew(void** iset_ptr, int val, Player* player) { // adds the value to the iset
    // important prerequisite: we are assuming that the value we seek to add is not already in the set
    // we make this assumption because it saves us time from checking, and the nature of this program is only expected to add unique integers in the first place.
    if (val < 0) {
        fprintf(stderr, "iset_addnew: attempting to add a negative integer to the iset\n");
        exit(1);
    }

    size_t len = iset_length(*iset_ptr);
    size_t cap = iset_capacity(*iset_ptr);
    _iset_change_length(*iset_ptr, 1);

    if (len + 1 > cap) {
        _iset_change_capacity(iset_ptr, 32); // allocate space for 32 more integers on the iset
    }

    if (player == NULL) {
        int* iset = *iset_ptr;
        iset[len] = val;
    } else {
        Player** pset = *iset_ptr;
        pset[len] = player;
    }
}

char* elements[] = {"fire", "water", "wind", "blood"};
char* reg_moves[] = {"flame slash", "water cut", "wind slice", "blood stab"};
char* spec_moves[] = {"Wolf-Rayet star WR 102", "15,750 psi!!", "Let us not burthen our remembrance with / A heaviness thats gone.", "holy grail."};

int* clients;
int* waiting_clients;
int* cooldown_list;
Player** players;

int main() {

    clients = iset_init("int");
    waiting_clients = iset_init("int");
    cooldown_list = iset_init("int");
    players = (Player**) iset_init("Player*");

    int soc = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    if((setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }

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

    // use select() to avoid blocking with accept()
    fd_set readfds;
    FD_ZERO (&readfds);
    FD_SET(soc, &readfds); // add the server socket into read_fds() which will listen for clients to connect
    int max_fd = soc;

    while (1) {
        printf("pending select\n");
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) != 1) {
            perror("select");
            exit(1);
        }
        printf("selected\n");

        if (FD_ISSET(soc, &readfds)) {
            accept_client(soc);
        }

        for (int i = 0; i < iset_length(clients); i++) {
            int fd = clients[i];

            if (FD_ISSET(fd, &readfds)) {
                // printf("reading from client\n");
                handle_read(fd);
            }
            // now reset readfds to contain everything
            FD_SET(fd, &readfds); 

            if (fd > max_fd) {
                max_fd = fd;
            }
            printf("%d\n", fd);
        }
    }
    return 0;
}

void accept_client(int soc) {
    printf("accepted a client\n");
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    __u_int client_len = sizeof(struct sockaddr_in);
    
    int client_soc = accept(soc, (struct sockaddr*) &client_addr, &client_len);
    if (client_soc == -1) {
        perror("accept");
        exit(1);
    }

    iset_addnew((void**) &clients, client_soc, NULL);
    iset_addnew((void**) &waiting_clients, client_soc, NULL);

    player_init(client_soc);
    
    if (iset_length(waiting_clients) > 1) {
        check_wait();
    }
}


void player_init(int fd) {
    if (fd >= iset_length(players)) {
        _iset_change_capacity( (void**) &players, 32);
    }
    Player player;
    player.element = -1;
    strcpy(player.user, "\0");
    players[fd] = &player;
    write(fd, "what is your name young one?\r\n", 31);
}

void check_wait() {
    int fd1 = waiting_clients[0];
    int fd2 = waiting_clients[1];
    Player* p1 = players[fd1];
    Player* p2 = players[fd2];
    if (p1->foe != fd2) { 
        // bababooey
    }
}

void handle_read(int fd) {
    Player* pl = players[fd];
    ssize_t bytes_read = read(fd, pl->buffer, MAX_USER_LENGTH);
    printf("%s\n", pl->buffer);
    if (bytes_read == -1) {
        perror("handle_read");
        exit(1); 
    } 
    
    else if (bytes_read == 0) {
        /* client left, kill_client(int fd) */
    }

    else { // process the buffer data
        pl->buffer[bytes_read] = '\0';
        char* newline_ptr = strchr(pl->buffer, '\n');

        if (newline_ptr != NULL) { // user sent a message.
            if (strcmp(pl->user, "\0") == 0) { // user stated their name
                strcpy(pl->user, pl->buffer);
                join_msg(fd, pl->user);
                printf("%s joined\n", pl->user);
            }

            else if (pl->element == -1) { // user stated their element
                int elem = atoi(pl->buffer);
                if (1 <= elem && elem <= 4) { // valid elemnt
                    pl->element = elem - 1;
                }

                else {
                    write(fd, "not a valid element number\r\n", 29);
                }
            }
        }
    }


}

void remove_newlines(char *str) {
    char* ptr = strchr(str, '\n'); // find first newline
    if (ptr != NULL) {
        *ptr = '\0'; // end from there
    }
}

void join_msg(int fd, char* name) {
    for (int i = 0; i < iset_length(clients); i++) {
        if (clients[i] != fd) {

            char message[MAX_MESSAGE_LENGTH];
            sprintf(message, "%s has joined\r\n", name);

            write(fd, name, MAX_MESSAGE_LENGTH);
        }
    }
}