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

#include "words.h"
#include "hashtbl.h"
#include "list.h"

// Page locations - MAYBE PUT THIS IN A SEPARATE HEADER FILE
#define INTRO_PAGE "./html/1_intro.html"
#define START_PAGE "./html/2_start.html"
#define FIRST_TURN_PAGE "./html/3_first_turn.html"
#define ACCEPTED_PAGE "./html/4_accepted.html"
#define DISCARDED_PAGE "./html/5_discarded.html"
#define ENDGAME_PAGE "./html/6_endgame.html"
#define GAMEOVER_PAGE "./html/7_gameover.html"

#define START_GET_QUERY "http://172.26.37.26:7900/?start=Start"


//Post patterns
#define QUIT_PATTERN "quit=Quit"
#define QUIT_PATTERN_LEN 9
#define GUESS_PATTERN "keyword="
#define GUESS_PATTERN_LEN 8
#define USER_PATTERN "user="
#define USER_PATTERN_LEN 5

#define NRESERVED_SOCKS 4

// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_200_SET_ID_COOKIE = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\
Set-Cookie: clientId=%d\r\n\r\n";
static char const * const HTTP_301_FORMAT = "HTTP/1.1 301 Moved Permanently\r\n\
Location: %s\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;

static char const * const HELLO_USER_MSG = "<h3>Hello %s!<h3>";

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
    bool cookie_isset[MAX_USERS];
    HashTable *cookie_user_table;
    List *word_lists[MAX_USERS];
    bool end_game;
    int image_no;

} game_data_t;

static bool handle_simple_get(int sockfd, const char *page, game_data_t *game_data);
static bool handle_root_page(int sockfd, METHOD method, char *post_data, game_data_t *game_data, int client_id);
static bool handle_start_page(int sockfd, METHOD method, char *post_data, game_data_t *game_data);
static bool handle_quit_page(int sockfd, game_data_t *game_data);

static void ready_game_data(game_data_t *game_data);

static bool http_redirect(int sockfd, char *qstring);


static bool handle_http_request(int sockfd, game_data_t *game_data)
{
    // initialise before required to print a 404 on server
    int writeN;
    // try to read the request
    char buff[2049];
    bzero(buff, sizeof(buff));
    int n = read(sockfd, buff, 2049);
    printf("\n\nHTTP Request: \n%s\n", buff);

    int id = sockfd - NRESERVED_SOCKS;

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
    if (cookie) {
        // NB: FIX THIS. IN FIREFOX AN EXTRA FIELD IS ADDED AFTER THE COOKIE FIELD
        cookie += 17; // Cookie: clientId=
        char client_id_str[5];
        strncpy(client_id_str, cookie, 4);
        client_id_str[5] = '\0';
        client_id = atoi(client_id_str);

        printf("the cookie id is: %d\n", client_id);

        game_data->cookie_isset[id] = true;
        // Fetch the username for the user on the connected socket and set it for this connection
        char str[5];
        str[5] = '\0';

        sprintf(str, "%d", client_id);
        if (hash_table_has(game_data->cookie_user_table, str)) {
            char *username = hash_table_get(game_data->cookie_user_table, str);
            printf("The cookie has a username: %s\n", username);
        } else {
            printf("The cookie does not have a username\n");
        }
    } else {
        game_data->cookie_isset[id] = false;
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;

    // if URL '/', root page
    if (*curr == ' ') {
        return handle_root_page(sockfd, method, post_data, game_data, client_id);
    }

    // The game playing page
    else if (strncmp(curr, "?start=Start", 12) == 0) {
        curr += 12;
        return handle_start_page(sockfd, method, post_data, game_data);
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

// Was going to be used, maybe still will be.
static bool http_redirect(int sockfd, char *qstring) {
    char buff[2048];
    int n = sprintf(buff, HTTP_301_FORMAT, qstring);

    printf("Redirect request:\n%s\n", buff);
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

    game_data->user_ready[0] = false;
    game_data->user_ready[1] = false;
    game_data->image_no++;
}

static bool handle_root_page(int sockfd, METHOD method, char *post_data, game_data_t *game_data, int client_id) {
    printf("Received the cookie id in handle_root_page: %d\n", client_id);
    int id = sockfd - NRESERVED_SOCKS;
    if (method == GET)
    {
        // If the user has a cookie AND their username is set, retrieve start page
        if (game_data->cookie_isset[id]) {
            handle_simple_get(sockfd, START_PAGE, game_data);
        } else {
            handle_simple_get(sockfd, INTRO_PAGE, game_data);
        }

    }
    else if (method == POST)
    {
        if (!strncmp(post_data, QUIT_PATTERN, QUIT_PATTERN_LEN)) {
            game_data->user_ready[id] = false;
            return handle_quit_page(sockfd, game_data);
        } else if (!strncmp(post_data, USER_PATTERN, USER_PATTERN_LEN)) {
            char* username = strstr(post_data, USER_PATTERN) + USER_PATTERN_LEN;
            printf("Storing username '%s' in hashtable with cookie key: %d\n", username, client_id);
            char str[5];
            str[5] = '\0';

            sprintf(str, "%d", client_id);
            hash_table_put(game_data->cookie_user_table, str, username);

            handle_simple_get(sockfd, START_PAGE, game_data);
        }
        else {
            printf("You hacker man, get fucked\n");
        }
    }
    else {
        // never used, just for completeness
        fprintf(stderr, "no other methods supported");
    }
    return true;
}

static bool handle_start_page(int sockfd, METHOD method, char *post_data, game_data_t *game_data) {
    int id = sockfd - NRESERVED_SOCKS;
    if (method == GET)
    {
        game_data->end_game = false;
        game_data->user_ready[id] = true;

        // !!!!!!!! HERE FIRST_TURN_PAGE NEEDS MODIFYING WITH IMAGE NO !!!!!!!!!
        handle_simple_get(sockfd, FIRST_TURN_PAGE, game_data);
    } else if (method == POST) {

        // is it quit?
        if (!strncmp(post_data, QUIT_PATTERN, QUIT_PATTERN_LEN)) {
            return handle_quit_page(sockfd, game_data);
        }
        // is it guess?
        else if (!strncmp(post_data, GUESS_PATTERN, GUESS_PATTERN_LEN)) {
            bool ready = (game_data->user_ready[0] && game_data->user_ready[1]);
            // Check if other player ended the game by guessing a word in your list
            if (game_data->end_game) {
                handle_simple_get(sockfd, ENDGAME_PAGE, game_data);
            }
            if (ready) {
                char* guess_word = strstr(post_data, GUESS_PATTERN) + GUESS_PATTERN_LEN;
                // '&' is the beginning of the second POST argument guess=Guess
                char *clean_guess = strtok(guess_word, "&");
                char *new_guess = malloc((sizeof(char) * strlen(clean_guess)) + 1);
                strcpy(new_guess, clean_guess);

                printf("User guessed '%s'\n", clean_guess);
                List *this_list = game_data->word_lists[id];
                List *other_list = game_data->word_lists[!id];

                if (list_find(other_list, clean_guess)) {
                    printf("Found in other users list, game over.\n");
                    handle_simple_get(sockfd, ENDGAME_PAGE, game_data);

                    // readies game data for new game
                    ready_game_data(game_data);

                    printf("New image number: %d\n", game_data->image_no);
                    // end game for other player too.
                } else if (!list_find(this_list, clean_guess)) {
                    // Don't double up on words
                    printf("The word was not in the list so add it.\n");
                    list_add_end(this_list, new_guess);
                    handle_simple_get(sockfd, ACCEPTED_PAGE, game_data);
                } else {
                    handle_simple_get(sockfd, ACCEPTED_PAGE, game_data);
                }
            } else {
                handle_simple_get(sockfd, DISCARDED_PAGE, game_data);
            }
        }
    } else {
        // never used, just for completeness
        fprintf(stderr, "no other methods supported");
    }
    return true;
}

static bool handle_quit_page(int sockfd, game_data_t *game_data) {
    // may have to delete and reset some shit.
    handle_simple_get(sockfd, GAMEOVER_PAGE, game_data);
    // return false to close TCP connection
    return false;
}

// Just returns the contents of the html at page
static bool handle_simple_get(int sockfd, const char* page, game_data_t *game_data) {
    int id = sockfd - NRESERVED_SOCKS;
    struct stat st;
    stat(page, &st);
    char buff[2049];
    int n;

    if (!(game_data->cookie_isset[id])) {
        // Randomise a client_id
        int client_id = rand() % (9999 + 1 - 1000) + 1000;
        n = sprintf(buff, HTTP_200_SET_ID_COOKIE, st.st_size, client_id);
        game_data->cookie_isset[id] = true;
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
        n = sendfile(sockfd, filefd, NULL, 2048);
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

game_data_t init_game_data() {
    game_data_t game_data;
    // memset(game_data.usernames, '\0' ,sizeof(char*) * MAX_USERS);
    game_data.user_ready[0] = false;
    game_data.user_ready[1] = false;

    game_data.cookie_isset[0] = false;
    game_data.cookie_isset[1] = false;

    game_data.word_lists[0] = new_list();
    game_data.word_lists[1] = new_list();

    game_data.cookie_user_table = new_hash_table(5000);

    game_data.end_game = false;

    game_data.image_no = 1;
    return game_data;
}

// Runs the server
int main(int argc, char * argv[])
{
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

    // listen on the socket
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
        printf("The max fd: %d\n", maxfd);
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
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
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
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, ip, INET_ADDRSTRLEN),
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

    // Free everything
    free_list(game_data.word_lists[0]);
    free_list(game_data.word_lists[1]);

    return 0;
}
