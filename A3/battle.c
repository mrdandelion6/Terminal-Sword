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
void kill_client(int fd);
void check_write(int fd, int return_val);
int write_with_size(int fd, char* s);
void accept_client(int soc);
void check_wait();
void handle_read(int fd);
void player_init(int fd);
void remove_newlines(char *str);
void join_msg(int fd, char* name);

typedef struct player { // 16-bit aligned
    char user[32]; // 32
    int element; // 32
    int foe; // 32
    char buffer[MAX_MESSAGE_LENGTH]; // 256
    char msg[MAX_MESSAGE_LENGTH]; // 256
} Player; 

// IMPORTANT CONSTANTS
char* elements[] = {"fire", "water", "wind", "blood"};
char* choose_msg[] = {"The air is getting colder around you...\n", "Try not to drown in your own power.. literally\n", "Let the wind be your guide\n", "Impending doom approaches...\n"};
char* reg_moves[] = {"flame slash", "water cut", "wind slice", "blood stab"};
char* spec_moves[] = {"Wolf-Rayet star WR 102", "15,750 psi!!", "Let us not burthen our remembrance with / A heaviness thats gone.", "holy grail."};

fd_set readfds;
int* clients;
int* waiting_clients;
int* cooldown_list;
Player** players;

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

int iset_max(int* iset) { // return maximum file descriptor in the iset. used for clients only
    int maxfd = 0;
    for (int i = 0; i < iset_length(iset); i++) {
        if (iset[i] > maxfd) {
            maxfd = iset[i];
        }
    }
    return maxfd;
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

void iset_remove(char** iset_ptr, int val, int ind) { // removes the value from the iset by index if index is given.

    char* iset = *iset_ptr;
    size_t len = iset_length(iset);
    if (len == 0) {
        fprintf(stderr, "iset_remove: attempting to remove a value from an empty iset\n");
        exit(1);
    }

    int index;
    if (ind != -1) {
        index = ind;
    }
    else {
        index = _iset_index_of( (int*) iset, val ); 
        if (index == -1) {
            fprintf(stderr, "iset_remove: attempting to remove a value not in the iset\n");
            exit(1);
        }
    }

    size_t cap = iset_capacity(iset);
    size_t size = iset_size(iset);

    if (size == sizeof(int)) { // for iset
        iset[index * size] = iset[(len - 1) * size]; // replace the value we are removing with the last item
        _iset_change_length(iset, -1);

        if ( (cap - (len - 1) >= 32) && len != 1) {  
            int diff = len - 1 - cap;
            _iset_change_capacity( (void**) iset_ptr, diff);
        }

    }

    else { // for player set
        ( (Player**) iset)[index] = NULL;
        _iset_change_length(iset, -1);
        int maxfd = iset_max(clients);
        if ( (cap - maxfd >= 32) && maxfd != 0) {
            int diff = maxfd - cap;
            _iset_change_capacity( (void**) iset_ptr, diff);
        }
    }

}

int iset_ordered_remove(int** iset_ptr, int ind, int val) { // only for integer sets!! not player set
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

    int index;
    int value;
    if (ind != - 1) { // use index if provided
        index = ind;
        value = iset[ind];
    }
    else { // else use value
        value = val;
        index = _iset_index_of(iset, val);
    }
    
    if (index == -1) {
        value = -1; // value not popped
        printf("client %d was not waiting\n", val);
    }

    else { // shift list back, client was waiting so we pop it
        for (int i = index; i < len - 1; i++) {
            iset[i] = iset[i + 1];
        }
        _iset_change_length(iset, -1);

        if (shrink) { // we shrink down the iset if we need to
            int diff = len - 1 - cap;
            _iset_change_capacity( (void**) iset_ptr, diff);
        }
    }

    return value;
}

int iset_deque(int** iset_ptr) { // remove the first element in the iset and shift all elemenets back
    return iset_ordered_remove(iset_ptr, 0, -1);
}

void iset_addnew(void** iset_ptr, int val, Player* player, int ind) { // adds the value to the iset
    // important prerequisite: we are assuming that the value we seek to add is not already in the set
    // we make this assumption because it saves us time from checking, and the nature of this program is only expected to add unique integers in the first place.
    if (val < 0 && player == NULL) {
        fprintf(stderr, "iset_addnew: attempting to add a negative integer to the iset\n");
        exit(1);
    }

    size_t len = iset_length(*iset_ptr);
    size_t cap = iset_capacity(*iset_ptr);
    _iset_change_length(*iset_ptr, 1);

    if (len + 1 > cap) {
        _iset_change_capacity(iset_ptr, 32); // allocate space for 32 more integers on the iset
        cap += 32;
    }

    if (player == NULL) {
        int* iset = *iset_ptr;
        iset[len] = val;
    } else {

        int maxfd = iset_max(clients);
        while (maxfd > cap) {
            _iset_change_capacity(iset_ptr, 32);
            cap += 32;
        }

        Player** pset = *iset_ptr;
        pset[ind] = player;
    }
}

int main() {

    clients = iset_init("int");
    waiting_clients = iset_init("int");
    cooldown_list = iset_init("int");
    players = (Player**) iset_init("Player*"); // cast to pset

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

        if (FD_ISSET(4, &readfds)) {
            printf("4 is ready\n");
        }

        for (int i = 0; i < iset_length(clients); i++) {
            int fd = clients[i];

            if (FD_ISSET(fd, &readfds)) {
                printf("reading from client %d\n", fd);
                handle_read(fd);
            }
            // now reset readfds to contain everything
            FD_SET(fd, &readfds); 
        }

        if (iset_length(clients) > 0) {
            max_fd = iset_max(clients);
        }
        else {
            max_fd = soc;
        }

        FD_SET(soc, &readfds);
    }
    return 0;
}

void accept_client(int soc) {
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    __u_int client_len = sizeof(struct sockaddr_in);
    
    int client_soc = accept(soc, (struct sockaddr*) &client_addr, &client_len);
    if (client_soc == -1) {
        perror("accept");
        exit(1);
    }

    printf("accepted a client on socket %d\n", client_soc);
    iset_addnew((void**) &clients, client_soc, NULL, -1);
    player_init(client_soc);
}


void player_init(int fd) {
    if (fd >= iset_length(players)) {
        _iset_change_capacity( (void**) &players, 32);
    }
    Player player;
    player.element = -1;
    strcpy(player.user, "\0");
    strcpy(player.msg, "\0");

    iset_addnew((void**) &players, -1, &player, fd);

    int write_check = write_with_size(fd, "What is your name young one?\r\n");
    check_write(fd, write_check);
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
    if (bytes_read == -1) {
        perror("read");
        exit(1); 
    } 
    
    else if (bytes_read == 0) { // client is closed
        kill_client(fd);
    }

    else { // process the buffer data
        pl->buffer[bytes_read] = '\0';
        strcat(pl->msg, pl->buffer);

        char* newline_ptr = strchr(pl->buffer, '\n');

        if (newline_ptr != NULL) { // user sent a message.
            remove_newlines(pl->msg);

            if (strcmp(pl->user, "\0") == 0) { // user stated their name
                strcpy(pl->user, pl->msg);
                join_msg(fd, pl->user);
                printf("%s joined\n", pl->user); // server side message for testing
                int write_check = write_with_size(fd, "Choose your element (cosmetic only).\n(1): fire\n(2): water\n(3): air\n(4): blood\r\n");
                check_write(fd, write_check);
            }

            else if (pl->element == -1) { // user stated their element
                int elem = atoi(pl->msg);
                if (1 <= elem && elem <= 4) { // valid elemnt
                    pl->element = elem - 1;
                    printf("%s chosen\n", elements[pl->element]); // server side message for testing
                    int write_check = write_with_size(fd, choose_msg[pl->element]);
                    check_write(fd, write_check);
                    write_check = write_with_size(fd, "Waiting for an oponent..\r\n");
                    check_write(fd, write_check);
                    iset_addnew((void**) &waiting_clients, fd, NULL, -1);
                    if (iset_length(waiting_clients) >= 2) {
                        check_wait();
                    }
                }
                else {
                    int write_check = write_with_size(fd, "Not a valid element number!\r\n");
                    check_write(fd, write_check);
                }
            }
            strcpy(pl->buffer, "\0");
            strcpy(pl->msg, "\0");
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

int write_with_size(int fd, char* s) {
    return write(fd, s, strlen(s));
}

void check_write(int fd, int return_val) { // function to check the return values of write
    // printf("socket %d returned %d\n", fd, return_val);
    if (return_val < 0) { // error case
        perror("write");
        exit(1);
    }

    else if (return_val == 0) { // client closed socket
        kill_client(fd);
    }

}

void kill_client(int fd) {
    printf("killing client %d\n", fd);

    iset_remove( (char**) &clients, fd, -1 ); // remove client with value fd
    iset_remove( (char**) &players, -1, fd ); // remove the player at index fd
    iset_ordered_remove(&waiting_clients, -1, fd);

    FD_CLR(fd, &readfds);
    close(fd); // close socket on serv side
}