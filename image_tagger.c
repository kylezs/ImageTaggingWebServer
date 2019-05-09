/*
This file is the center of the image_tagger server
Created by: Kyle Zsembery, Student Number: 911920

Credit to lab 6 comp30023 for base of code
*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

#include "hashtbl.h"
#include "list.h"

#define INTRO_PAGE "./html/1_intro.html"
#define START_PAGE "./html/2_start.html"

#define FIRST_TURN 3
#define FIRST_TURN_PAGE "./html/3_first_turn.html"
#define FIRST_TURN_PAGE2 "./html/3_first_turn2.html"

#define ACCEPTED 4
#define ACCEPTED_PAGE "./html/4_accepted.html"
#define ACCEPTED_PAGE2 "./html/4_accepted2.html"

#define DISCARDED 5
#define DISCARDED_PAGE "./html/5_discarded.html"
#define DISCARDED_PAGE2 "./html/5_discarded2.html"

#define ENDGAME_PAGE "./html/6_endgame.html"
#define GAMEOVER_PAGE "./html/7_gameover.html"

#define INVALID_PAGE "INVALID PAGE"

#define START_GET_QUERY "?start=Start"

#define COOKIE_ID_LEN 4

// Extra ids in case of returning browsers
#define MAX_USERS 2
#define MAX_IDS MAX_USERS + 4

//Post patterns
#define QUIT_PATTERN "quit=Quit"
#define QUIT_PATTERN_LEN 9
#define GUESS_PATTERN "keyword="
#define GUESS_PATTERN_LEN 8
#define USER_PATTERN "user="
#define USER_PATTERN_LEN 5

#define NRESERVED_SOCKS 4

#define BUFF_SIZE 2048

// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_200_SET_ID_COOKIE = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\
Set-Cookie: clientId=%d\r\n\r\n";
static char const * const HTTP_307_FORMAT = "HTTP/1.1 307 Temporary Redirect\r\n\
Location: %s\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;

// represents the types of method
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;

// Struct store # of accepts and store the usernames and word lists

// typedef char** word_list;
typedef struct {
    bool user_ready[MAX_USERS];
    HashTable *cookie_user_table;
    List *word_lists[MAX_USERS];
    bool end_game;
    bool game_over;
    int image_no;
    int assigned_ids[MAX_IDS];
    int quit_count;

} game_data_t;

static bool handle_simple_get(int sockfd, const char *page,
    game_data_t *game_data, int client_id);
static bool handle_root_page(int sockfd, METHOD method, char *post_data,
     game_data_t *game_data, int client_id);
static bool handle_start_page(int sockfd, METHOD method, char *post_data,
     game_data_t *game_data, int client_id);
static bool handle_quit_page(int sockfd, game_data_t *game_data, int client_id);

static bool handle_dynamic_get(int sockfd, const char* page,
    game_data_t *game_data, int client_id);

static void ready_game_data(game_data_t *game_data);

static bool http_redirect(int sockfd, char *qstring);

static char *resolve_page_name(int page_type, game_data_t *game_data);

/* handles all http requests */
static bool handle_http_request(int sockfd, game_data_t *game_data)
{
    // initialise before required to print a 404 on server
    int writeN;
    // try to read the request
    char buff[BUFF_SIZE];
    bzero(buff, sizeof(buff));
    int n = read(sockfd, buff, BUFF_SIZE);

    if (n <= 0)
    {
        if (n < 0)
            perror("initial read");
        else
            printf("socket %d close the connection\n", sockfd);
        return false;
    }

    // terminate the string
    buff[n] = 0;

    char * curr = buff;
    char * post_data = NULL;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0)
    {
        curr += 4;
        method = GET;
    }
    else if (strncmp(curr, "POST ", 5) == 0)
    {
        curr += 5;
        method = POST;
        post_data = strstr(buff, "\r\n\r\n") + 4;
    }
    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    // Check if the request contains a cookie
    int client_id=0;
    // get pointer to start of cookie part
    char *cookie = strstr(curr, "Cookie:");
    bool has_username = false;
    if (cookie) {
        cookie += 17; // Cookie: clientId=
        char client_id_str[COOKIE_ID_LEN + 1];
        strncpy(client_id_str, cookie, COOKIE_ID_LEN);
        client_id_str[COOKIE_ID_LEN + 1] = '\0';
        client_id = atoi(client_id_str);
        has_username = hash_table_has(game_data->cookie_user_table,
             client_id_str);
    }

    if (game_data->game_over) {
        return handle_quit_page(sockfd, game_data, client_id);
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;

    // if URL '/', root page
    if (*curr == ' ') {
        return handle_root_page(sockfd, method, post_data, game_data,
             client_id);
    }

    // The game playing page
    else if (strncmp(curr, START_GET_QUERY, 12) == 0) {
        curr += 12;
        if (has_username) {
            return handle_start_page(sockfd, method, post_data, game_data,
                 client_id);
        } else {
            return http_redirect(sockfd, "/");
        }
    }

    // send 404 and log this on the server
    else if ((writeN = write(sockfd, HTTP_404, HTTP_404_LENGTH)) < 0)
    {
        perror("write");
        return false;
    } else {
        printf("404 page not found\n");
    }

    return true;
}

/* Temporary redirects to qstring */
static bool http_redirect(int sockfd, char *qstring) {
    char buff[BUFF_SIZE];
    int n = sprintf(buff, HTTP_307_FORMAT, qstring);

    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }
    return true;
}

/* Ready the game state for a new game between same players */
static void ready_game_data(game_data_t *game_data) {
    game_data->end_game = true;
    free_list(game_data->word_lists[0]);
    free_list(game_data->word_lists[1]);

    game_data->word_lists[0] = new_list();
    game_data->word_lists[1] = new_list();

    game_data->user_ready[0] = false;
    game_data->user_ready[1] = false;
    game_data->image_no++;

    game_data->game_over = false;

    game_data->quit_count = 0;
}

/* handles all actions taken on the / page */
static bool handle_root_page(int sockfd, METHOD method, char *post_data,
     game_data_t *game_data, int client_id) {
    int id = sockfd - NRESERVED_SOCKS;
    if (method == GET)
    {
        char str[5];
        str[5] = '\0';
        sprintf(str, "%d", client_id);

        if (hash_table_has(game_data->cookie_user_table, str)) {
            handle_dynamic_get(sockfd, START_PAGE, game_data, client_id);
        } else {
            handle_simple_get(sockfd, INTRO_PAGE, game_data, client_id);
        }

    } else if (method == POST) {
        if (!strncmp(post_data, QUIT_PATTERN, QUIT_PATTERN_LEN)) {
            game_data->user_ready[id] = false;
            return handle_quit_page(sockfd, game_data, client_id);
        } else if (!strncmp(post_data, USER_PATTERN, USER_PATTERN_LEN)) {
            char* username = strstr(post_data, USER_PATTERN) + USER_PATTERN_LEN;
            char str[5];
            str[5] = '\0';
            sprintf(str, "%d", client_id);
            // NB: If username already set, the set username page won't be shown
            hash_table_put(game_data->cookie_user_table, str, username);

            handle_dynamic_get(sockfd, START_PAGE, game_data, client_id);
        }
        else {
            fprintf(stderr, "Only quit and user patterns supported\n");
        }
    } else {
        // never used, just for completeness
        fprintf(stderr, "no other methods supported");
    }
    return true;
}

/* handles all actions taken on the /?start=Start page */
static bool handle_start_page(int sockfd, METHOD method, char *post_data,
        game_data_t *game_data, int client_id) {
    int id = sockfd - NRESERVED_SOCKS;
    if (method == GET)
    {
        // Ensure game not ended by other player
        game_data->end_game = false;
        printf("User id: %d ready? %d\n", id, game_data->user_ready[id]);
        game_data->user_ready[id] = true;
        handle_simple_get(sockfd, resolve_page_name(FIRST_TURN, game_data),
            game_data, client_id);
    }
    else if (method == POST) {

        // is it quit?
        if (!strncmp(post_data, QUIT_PATTERN, QUIT_PATTERN_LEN)) {
            return handle_quit_page(sockfd, game_data, client_id);
        }
        // is it guess?
        else if (!strncmp(post_data, GUESS_PATTERN, GUESS_PATTERN_LEN)) {
            bool ready = (game_data->user_ready[0] && game_data->user_ready[1]);
            // Check if game ended by other player
            if (game_data->end_game) {
                game_data->user_ready[id] = false;
                handle_simple_get(sockfd, ENDGAME_PAGE, game_data, client_id);
            }

            // are both players ready?
            if (ready) {
                char* guess_word = strstr(post_data, GUESS_PATTERN)
                    + GUESS_PATTERN_LEN;
                // '&' is the beginning of the second POST argument guess=Guess
                char *clean_guess = strtok(guess_word, "&");
                char *new_guess = malloc((sizeof(char) * strlen(clean_guess))
                    + 1);
                strcpy(new_guess, clean_guess);

                // Hacky? Yes. Works? Yes.
                List *this_list = game_data->word_lists[id];
                List *other_list = game_data->word_lists[!id];

                if (list_find(other_list, clean_guess)) {
                    handle_simple_get(sockfd, ENDGAME_PAGE, game_data,
                        client_id);
                    // readies game data for new game
                    ready_game_data(game_data);
                } else if (!list_find(this_list, clean_guess)) {
                    // Don't double up on words
                    list_add_end(this_list, new_guess);
                    handle_dynamic_get(sockfd, resolve_page_name(ACCEPTED,
                         game_data), game_data, client_id);
                } else {
                    handle_dynamic_get(sockfd, resolve_page_name(ACCEPTED,
                         game_data), game_data, client_id);
                }
            } else {
                handle_simple_get(sockfd, resolve_page_name(DISCARDED,
                     game_data), game_data, client_id);
            }
        }
    } else {
        // never used, just for completeness
        fprintf(stderr, "no other methods supported");
    }
    return true;
}

static bool handle_quit_page(int sockfd, game_data_t *game_data, int client_id)
{
    game_data->quit_count++;
    handle_simple_get(sockfd, GAMEOVER_PAGE, game_data, client_id);
    game_data->game_over = true;

    // if both players have quit, we can then ready the game data
    // for the next connecting players
    if (game_data->quit_count >= 2) {
        ready_game_data(game_data);
    }
    game_data->image_no = 1;
    // return false to close TCP connection
    return false;
}

// Used for the play pages and the start page
static bool handle_dynamic_get(int sockfd, const char* page,
        game_data_t *game_data, int client_id) {
    struct stat st;
    stat(page, &st);
    char buff[BUFF_SIZE];
    int n;
    // copied to another buffer using strcpy or strncpy to ensure that it
    // will not be overwritten.
    char added_buff[1000];
    added_buff[0] = '\0';

    long added_length = 0;
    if (!strcmp(page, START_PAGE)) {
        // Get added length of username
        char str_id[5];
        str_id[5] = '\0';
        sprintf(str_id, "%d", client_id);
        char *added_str = hash_table_get(game_data->cookie_user_table, str_id);
        added_length = strlen(added_str);
        strcpy(added_buff, added_str);

    } else {
        // KEYWORD ACCEPTED

        // get list of words entered for this user
        int id = sockfd - NRESERVED_SOCKS;
        List *this_list = game_data->word_lists[id];
        assert(this_list != NULL);
    	Node *node = this_list->head;

        while (node) {
            char* added_data = (char *) node->data;
            strcat(added_buff, (char*) node->data);
            strcat(added_buff, ", ");
            added_length += strlen(added_data) + 2;
            node = node->next;
        }
    }
    // send accurate Content-Length in header, making space for dynamic stuff
    long size = st.st_size + added_length;
    n = sprintf(buff, HTTP_200_FORMAT, size);
    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }
    // read the content of the HTML file, should only be startpage or play pages
    int filefd = open(page, O_RDONLY);
    n = read(filefd, buff, BUFF_SIZE);
    if (n < 0)
    {
        perror("read");
        close(filefd);
        return false;
    }
    close(filefd);

    char retBuff[BUFF_SIZE];
    sprintf(retBuff, buff, added_buff);

    if (write(sockfd, retBuff, size) < 0)
    {
        perror("write");
        return false;
    }
    return true;
}

// Just returns the contents of the html at page
static bool handle_simple_get(int sockfd, const char* page, game_data_t
        *game_data, int client_id) {
    struct stat st;
    stat(page, &st);
    char buff[BUFF_SIZE];
    int n;

    if (!strcmp(page, INTRO_PAGE) && (client_id == 0)) {
        // Randomise a client_id, ensure not already in use
        int new_client_id = 0;
        bool client_id_set = false;
        while (!client_id_set) {
            new_client_id = rand() % (9999 + 1 - 1000) + 1000;
            for (int i = 0; i<MAX_IDS; i++) {
                if (new_client_id == game_data->assigned_ids[i]) {
                    break;
                }
            }
            client_id_set = true;
        }

        n = sprintf(buff, HTTP_200_SET_ID_COOKIE, st.st_size, new_client_id);
    } else {
        n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
    }

    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }

    // send the file
    int filefd = open(page, O_RDONLY);
    do
    {
        n = sendfile(sockfd, filefd, NULL, BUFF_SIZE);
    }
    while (n > 0);

    if (n < 0)
    {
        perror("sendfile");
        close(filefd);
        return false;
    }

    close(filefd);
    return true;
}

static char *resolve_page_name(int page_type, game_data_t *game_data) {
    int image_no = game_data->image_no;
    if (page_type == ACCEPTED) {
        if (image_no == 1) {
            return ACCEPTED_PAGE;
        } else if (image_no == 2) {
            return ACCEPTED_PAGE2;
        }
    } else if (page_type == DISCARDED) {
        if (image_no == 1) {
            return DISCARDED_PAGE;
        } else if (image_no == 2) {
            return DISCARDED_PAGE2;
        }
    } else if (page_type == FIRST_TURN) {
        if (image_no == 1) {
            return FIRST_TURN_PAGE;
        } else if (image_no == 2) {
            return FIRST_TURN_PAGE2;
        }
    } else {
        fprintf(stderr, "No matching page type\n");
    }
    // return INTRO_PAGE;
    printf("Sorry, something went wrong resolving the\
page type: %d with image_no:%d\n", page_type, game_data->image_no);
    return INVALID_PAGE;
}

game_data_t init_game_data() {
    game_data_t game_data;
    game_data.user_ready[0] = false;
    game_data.user_ready[1] = false;

    game_data.word_lists[0] = new_list();
    game_data.word_lists[1] = new_list();

    game_data.cookie_user_table = new_hash_table(5000);

    game_data.end_game = false;

    game_data.game_over = false;

    game_data.image_no = 1;

    for (int i=0; i<MAX_IDS; i++) {
        game_data.assigned_ids[i] = 0;
    }

    game_data.quit_count = 0;
    return game_data;
}

void free_game_data(game_data_t *game_data) {
    free_list(game_data->word_lists[0]);
    free_list(game_data->word_lists[1]);

    free_hash_table(game_data->cookie_user_table);
}

// Runs the server
int main(int argc, char * argv[])
{
    srand(time(NULL));
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s ip port\n", argv[0]);
        return 0;
    }

    // create TCP socket which only accept IPv4
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // reuse the socket if possible
    int const reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // create and initialise address we will listen on
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // if ip parameter is not specified
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // bind address to socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 5);

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);
    // record the maximum socket number
    int maxfd = sockfd;


    game_data_t game_data = init_game_data();

    while (1)
    {
        // monitor file descriptors
        fd_set readfds = masterfds;
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // loop all possible descriptor
        for (int i = 0; i <= maxfd; ++i)
            // determine if the current file descriptor is active
            if (FD_ISSET(i, &readfds))
            {
                // create new socket if there is new incoming connection request
                if (i == sockfd)
                {
                    struct sockaddr_in cliaddr;
                    socklen_t clilen = sizeof(cliaddr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr,
                     &clilen);
                    if (newsockfd < 0)
                        perror("accept");
                    else
                    {
                        // add the socket to the set
                        FD_SET(newsockfd, &masterfds);
                        // update the maximum tracker
                        if (newsockfd > maxfd)
                            maxfd = newsockfd;
                        // print out the IP and the socket number
                        char ip[INET_ADDRSTRLEN];
                        printf(
                            "new connection from %s on socket %d\n",
                            // convert to human readable string
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr,
                                 ip, INET_ADDRSTRLEN),
                            newsockfd
                        );
                    }
                }
                // a request is sent from the client
                else if (!handle_http_request(i, &game_data))
                {
                    close(i);
                    FD_CLR(i, &masterfds);
                }
            }
    }

    free_game_data(&game_data);

    return 0;
}
