#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // fork()
#include <errno.h> // perrror()
#include <fcntl.h> // open(), close(), dup2()
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/select.h>
#include <time.h> // for rand

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
void initiate_battle(int fd1, int fd2);
void player_set_stats(int fd);
void special_move(int val, char* moves);
void norm_attack(int fd1, int fd2);
void power_attack(int fd1, int fd2);
void taunt(int fd1, int fd2);
void check_win(int fd1, int fd2);

typedef struct player { // 16-bit aligned
    char user[32]; // 32
    int element; // 32
    int foe; // 32
    char buffer[MAX_MESSAGE_LENGTH]; // 256
    char msg[MAX_MESSAGE_LENGTH]; // 256
    int hp;
    int normal_dmg;
    int special_dmg;
    float special_chance;
    int special_count;
    int turn; // players have -1 turn if they arent in game, 0 if they are in game but not their turn, and 1 if they are in game and its their turn.
} Player; 

// IMPORTANT CONSTANTS
char* elements[] = {"fire", "water", "wind", "blood"};
char* choose_msg[] = {"The air is getting colder around you...\n", "Try not to drown in your own power.. literally\n", "Let the wind be your guide\n", "Impending doom approaches...\n"};
char* reg_moves[] = {"flame slash", "water cut", "wind slice", "blood stab"};
char* spec_moves[] = {"Wolf-Rayet star WR 102", "15,750 psi!!", "Let us not burthen our remembrance with / A heaviness thats gone.", "holy grail."};

fd_set readfds;
int* clients;
int* waiting_clients;
Player** players;

typedef struct { // 16-byte alligned structure (size_t is 8 bytes)
    size_t capacity; // 64
    size_t length; // 64
    size_t elem_size; // 64
} Dynamic_Set_Header; // the meta data for a dynamic integer set. we will call an a dynamic integer set "iset" from now on.


int* iset_init(size_t e_size) { // initialize an iset
    int* ptr = NULL;
    size_t size = sizeof(Dynamic_Set_Header) + INITIAL_SET_SIZE * e_size;
    Dynamic_Set_Header* h = (Dynamic_Set_Header*) malloc(size);

    if (h) {
        h->elem_size = e_size;
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

void iset_remove(void** iset_ptr, int val, int ind) { // removes the value from the iset by index if index is given.

    void* iset = *iset_ptr;
    size_t len = iset_length(iset);
    if (len == 0) {
        fprintf(stderr, "iset_remove: attempting to remove a value from an empty iset\n");
        exit(1);
    }

    int index;
    if (ind != -1) {
        index = ind;
    }

    else { // if index is -1, then it's an integer set
        index = _iset_index_of( (int*) iset, val ); 
        if (index == -1) {
            fprintf(stderr, "iset_remove: attempting to remove a value not in the iset\n");
            exit(1);
        }
    }

    size_t cap = iset_capacity(iset);
    size_t size = iset_size(iset);

    printf("size is %lu\n", size);
    if (size == sizeof(int)) { // for iset

        ((int*) iset)[index] = ((int*) iset)[len -1]; // replace the value we are removing with the last item
        _iset_change_length(iset, -1);
        
        if ( (cap - (len - 1) >= 32) && len != 1) {  
            int diff = len - 1 - cap;
            _iset_change_capacity( (void**) iset_ptr, diff);
        }
    }
    
    
    else { // for player set
        free( ( (Player**) iset )[index] ); // free the memory for player
        ( (Player**) iset )[index] = NULL;
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
    size_t cap = iset_capacity(iset);

    if (len == 0) {
        return -1;
    }

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

    clients = iset_init(sizeof(int));
    waiting_clients = iset_init(sizeof(int));
    players = (Player**) iset_init(sizeof(Player*)); // cast to pset

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

        for (int i = 0; i < iset_length(clients); i++) {
            int fd = clients[i];
            // now reset readfds to contain everything
            
            if (FD_ISSET(fd, &readfds)) {
                printf("reading from client %d\n", fd);
                handle_read(fd);
            }
        }

        for (int i = 0; i < iset_length(clients); i++) { // second loop for this
            int fd = clients[i];
            printf("adding %d\n", fd);
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

    Player* player = malloc(sizeof(Player)); // allocate memory dynamically
    if (player == NULL) {
        perror("malloc");
        exit(1);
    }
    player->element = -1;
    strcpy(player->user, "\0");
    strcpy(player->msg, "\0");
    player->foe = -1;
    player->turn = -1;

    iset_addnew((void**) &players, -1, player, fd); // store the pointer to the dynamically allocated memory

    int write_check = write_with_size(fd, "What is your name young one?\r\n");
    check_write(fd, write_check);
}

void handle_read(int fd) {
    Player* pl = players[fd];
    printf("name is: %s\n", pl->user);
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

        if (pl->turn == 1) { // handle input immediately when its the player's turn
            printf("yoo!!?!?!\n");
            if ( (strcmp(pl->buffer, "a")) == 0 ) {
                norm_attack(fd, pl->foe);
            }

            else if ( (strcmp(pl->buffer, "p")) == 0 ) {
                power_attack(fd, pl->foe);
            }

            else if ( (strcmp(pl->buffer, "s")) == 0 ) {
                taunt(fd, pl->foe);
            }
            
        }

        else {
            char* newline_ptr = strchr(pl->buffer, '\n');

            if (newline_ptr != NULL) { // user sent a message.
                remove_newlines(pl->msg);

                if (strcmp(pl->user, "\0") == 0) { // user stated their name
                    strcpy(pl->user, pl->msg);
                    join_msg(fd, pl->user);
                    printf("%s joined\n", pl->user); // server side message for testing
                    int write_check = write_with_size(fd, "Choose your element (affects stats).\n(1): fire\n(2): water\n(3): air\n(4): blood\r\n");
                    check_write(fd, write_check);
                }

                else if (pl->element == -1) { // user stated their element
                    printf("yoo: %s\n", pl->user);
                    int elem = atoi(pl->msg);
                    if (1 <= elem && elem <= 4) { // valid elemnt
                        pl->element = elem - 1;
                        printf("%s chosen\n", elements[pl->element]); // server side message for testing
                        int write_check = write_with_size(fd, choose_msg[pl->element]);
                        check_write(fd, write_check);
                        write_check = write_with_size(fd, "Waiting for an opponent..\r\n");
                        check_write(fd, write_check);
                        iset_addnew((void**) &waiting_clients, fd, NULL, -1);
                        player_set_stats(fd);
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
            int msg_size = sizeof(name) + 14;
            char message[msg_size];
            strcpy(message, "**");
            strcat(message, name);
            strcat(message, " has joined**\r\n");

            write_with_size(clients[i], message);
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

void auto_win(int fd) {
    printf("player ran away\n");
    int write_check = write_with_size(fd, "Your opponent has forfeit. You win!\r\n");
    check_write(fd, write_check);

    printf("no issue here 1\n");
    player_set_stats(fd);
    printf("no issue here 2\n");
    iset_addnew((void**) &waiting_clients, fd, NULL, -1); // add back to list of waiting clients.

    printf("no issue here 3\n");
    write_check = write_with_size(fd, "Waiting for an opponent..\r\n");
    check_write(fd, write_check);
    printf("no issue here 4\n");
}

void kill_client(int fd) {
    printf("killing client %d\n", fd);
    Player* pl = players[fd];
    int foe = pl->foe;

    iset_remove( (void**) &clients, fd, -1 ); // remove client with value fd
    iset_remove( (void**) &players, -1, fd ); // remove the player at index fd
    iset_ordered_remove( &waiting_clients, -1, fd);

    printf("got here 1\n");
    if (pl->turn >= 0) { // then fd was in the middle of a game!
        auto_win(foe);
    }

    close(fd); // close socket on serv side
    FD_CLR(fd, &readfds);
    printf("got here 2\n");
}

int not_foes(int fd1, int fd2) {
    Player* p1 = players[fd1];
    Player* p2 = players[fd2];
    return (p1->foe != fd2) || (p2->foe != fd1);
}

void check_wait() {
    int fd1 = waiting_clients[0];
    int fd2;
    int wait_len = iset_length(waiting_clients);

    for (int i = 1; i < wait_len; i++) {
        fd2 = waiting_clients[i];
        if (not_foes(fd1, fd2)) {
            iset_ordered_remove(&waiting_clients, 0, -1); // remove first element
            iset_ordered_remove(&waiting_clients, i, -1); // remove matching client
            initiate_battle(fd1, fd2);
            return;
        }
    }

    if (wait_len > 2) {
        fprintf(stderr, "check_wait: 3+ clients but no matches occuring\n");
        exit(1);
    }
}

void player_set_stats(int fd) {
    printf("socket is %d\n", fd);
    Player* pl = players[fd];
    int elem = pl->element;
    printf("whata??\n");

    pl->turn = -1;

    switch (elem) {
        case 0: // fire
            pl->hp = 100;
            pl->normal_dmg = 20;
            pl->special_dmg = 70;
            pl->special_chance = 0.8;
            pl->special_count = 1;
            break;
        case 1: // water
            pl->hp = 150;
            pl->normal_dmg = 15;
            pl->special_dmg = 30;
            pl->special_chance = 0.5;
            pl->special_count = 10;
            break;
        case 2: // air
            pl->hp = 90;
            pl->normal_dmg = 20;
            pl->special_dmg = 50;
            pl->special_chance = 0.6;
            pl->special_count = 2;
            break; 
        case 3: // blood
            pl->hp = 120;
            pl->normal_dmg = 5;
            pl->special_dmg = 999;
            pl->special_chance = 0.1;
            pl->special_count = -1; // infinite
            break;
        default:
            fprintf(stderr, "player_set_stats: invalid element set\n");
            exit(1);
            break;
    }
}

void initiate_battle(int fd1, int fd2) {
    Player* p1 = players[fd1];
    Player* p2 = players[fd2];
    p1->foe = fd2;
    p2->foe = fd1;
    int first;
    int second;
    int write_val;

    srand(time(NULL));
    int random_number = rand();

    if (random_number <= RAND_MAX / 2) { 
        first = fd1;
        second = fd2;
    }

    else {
        first = fd2;
        second = fd1;
    }

    p1 = players[first];
    p2 = players[second];

    char message[MAX_MESSAGE_LENGTH];
    // char message2[MAX_MESSAGE_LENGTH];
    char moves[10];

    // send to first player
    strcpy(message, "You engage ");
    strcat(message, p2->user);
    strcat(message, "!\r\n");
    write_val = write_with_size(first, message);
    check_write(first, write_val);

    special_move(p1->special_count, moves);
    sprintf(message, "You have %d hp.\nYou have %s MP. \r\n", p1->hp, moves);
    write_val = write_with_size(first, message);
    check_write(first, write_val);

    sprintf(message, "\n%s's hp: %d \r\n\n", p2->user, p2->hp);
    write_val = write_with_size(first, message);
    check_write(first, write_val);

    strcpy(message, "(a)ttack\n(p)ower move\n(s)peak something\r\n");
    write_val = write_with_size(first, message);
    check_write(first, write_val);
    p1->turn = 1;
    

    // send to second player
    strcpy(message, "You engage ");
    strcat(message, p1->user);
    strcat(message, "!\r\n");
    write_val = write_with_size(second, message);
    check_write(second, write_val);

    special_move(p2->special_count, moves);
    sprintf(message, "You have %d hp.\nYou have %s MP.\r\n", p2->hp, moves);
    write_val = write_with_size(second, message);
    check_write(second, write_val);

    sprintf(message, "\n%s's hp: %d \r\n", p1->user, p1->hp);
    write_val = write_with_size(second, message);
    check_write(second, write_val);

    sprintf(message, "Waiting for %s to strike...\n", p1->user);
    write_val = write_with_size(second, message);
    check_write(second, write_val);
    p2->turn = 0;
    
}

void attack_prompts(int fd1, int fd2) { // send the attack prompts, given that fd1's turn
    usleep(700000);
    Player* p1 = players[fd1];

    char message[MAX_MESSAGE_LENGTH];
    int write_val;

    strcpy(message, "(a)ttack\n(p)ower move\n(s)peak something\r\n");
    write_val = write_with_size(fd1, message);
    check_write(fd1, write_val);

    sprintf(message, "Waiting for %s to strike...\n", p1->user);
    write_val = write_with_size(fd2, message);
    check_write(fd2, write_val);
}

void special_move(int val, char* moves) {
    if (val == -1) {
        strcpy(moves, "inf");
    }
    else {
        char msg[5];
        sprintf(msg, "%d", val);
        strcpy(moves, msg);
    }
}

void norm_attack(int fd1, int fd2) {
    Player* p1 = players[fd1];
    Player* p2 = players[fd2];

    // write messages
    char message[MAX_MESSAGE_LENGTH];
    int write_check;

    // write to fd1
    sprintf(message, "\nYou: '%s'\r\n", reg_moves[p1->element]);
    write_check = write_with_size(fd1, message);
    check_write(fd1, write_check);
    sprintf(message, "\nYou hit %s for %d damage!\r\n\n", p2->user, p1->normal_dmg);
    write_check = write_with_size(fd1, message);
    check_write(fd1, write_check);
    

    // write to fd2
    sprintf(message, "%s: '%s'", p1->user, reg_moves[p1->element]);
    write_check = write_with_size(fd2, message);
    check_write(fd2, write_check);
    sprintf(message, "\n\n%s hits you for %d damage!\r\r\n", p1->user, p1->normal_dmg);
    write_check = write_with_size(fd2, message);
    check_write(fd2, write_check);

    p2->hp -= p1->normal_dmg;

    p1->turn =  0;
    check_win(fd1, fd2);
}

int hit(float chance) {
    float random_num = (float)rand() / RAND_MAX;
    return random_num < chance;
}


void power_attack(int fd1, int fd2) {
    Player* p1 = players[fd1];
    Player* p2 = players[fd2];
    
    // write messages
    char message[MAX_MESSAGE_LENGTH];
    int write_check;

    if (p1->special_count == 0) {
        sprintf(message, "You have no power moves left!\r\n");
        write_check = write_with_size(fd1, message);
        check_write(fd1, write_check);
        return;
    }

    int attack_hit = hit(p1->special_chance);

    // write to fd1
    sprintf(message, "\nYou: '%s'\r\n", spec_moves[p1->element]);
    write_check = write_with_size(fd1, message);
    check_write(fd1, write_check);

    // write to fd2
    sprintf(message, "%s: '%s'", p1->user, spec_moves[p1->element]);
    write_check = write_with_size(fd2, message);
    check_write(fd2, write_check);
    
    if (attack_hit) {
        sprintf(message, "\nYou hit %s for %d damage!\r\n\n", p2->user, p1->special_dmg);
        write_check = write_with_size(fd1, message);
        check_write(fd1, write_check);

        sprintf(message, "\n\n%s hits you for %d damage!\r\r\n", p1->user, p1->special_dmg);
        write_check = write_with_size(fd2, message);
        check_write(fd2, write_check);

        p2->hp -= p1->special_dmg;
    }
    
    else {
        sprintf(message, "\nYou missed..\r\n\n");
        write_check = write_with_size(fd1, message);
        check_write(fd1, write_check);

        sprintf(message, "\n\n%s missed..\r\r\n", p1->user);
        write_check = write_with_size(fd2, message);
        check_write(fd2, write_check);
    }

    if (p1->special_count != -1) {
        p1->special_count--;
    }
    p1->turn =  0;
    check_win(fd1, fd2);
}
void taunt(int fd1, int fd2) {


}
void check_win(int fd1, int fd2) { // return 1 if fd1 beat fd2, else return 0. one-way check, not bidirectional!
    char message[MAX_MESSAGE_LENGTH];
    int write_check;
    Player* p1 = players[fd1];
    Player* p2 = players[fd2];

    int win = players[fd2]->hp <= 0;
    if (!win) { // didnt win
        char s_move[10];

        special_move(p1->special_count, s_move);
        sprintf(message, "Your HP: %d\nYour MP: %s\n\n%s's HP: %d\r\n", p1->hp, s_move, p2->user, p2->hp);
        write_check = write_with_size(fd1, message);
        check_write(fd1, write_check);

        special_move(p2->special_count, s_move);
        sprintf(message, "Your HP: %d\nYour MP: %s\n\n%s's HP: %d\r\n", p2->hp, s_move, p1->user, p1->hp);
        write_check = write_with_size(fd2, message);
        check_write(fd2, write_check);

        p2->turn = 1;
        attack_prompts(fd2, fd1);
    }

    else { // won
        sprintf(message, "You defeated %s!\r\n\n", p2->user);
        write_check = write_with_size(fd1, message);
        check_write(fd1, write_check);
        sprintf(message, "Waiting for new match...\n");
        write_check = write_with_size(fd1, message);
        check_write(fd1, write_check);

        sprintf(message, "You lost.\r\n\n");
        write_check = write_with_size(fd2, message);
        check_write(fd2, write_check);
        sprintf(message, "Waiting for new match...\n");
        write_check = write_with_size(fd2, message);
        check_write(fd2, write_check);

        p1->turn = -1;
        p2->turn = -1;

        player_set_stats(fd1);
        iset_addnew((void**) &waiting_clients, fd1, NULL, -1);
        player_set_stats(fd2);
        iset_addnew((void**) &waiting_clients, fd2, NULL, -1);
    }
}