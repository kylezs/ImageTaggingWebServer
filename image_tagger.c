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

// Page locations - MAYBE PUT THIS IN A SEPARATE HEADER FILE
#define INTRO_PAGE "./html/1_intro.html"
#define START_PAGE "./html/2_start.html"
#define FIRST_TURN_PAGE "./html/3_first_turn.html"
#define ACCEPTED_PAGE "./html/4_accepted.html"
#define DISCARDED_PAGE "./html/5_discarded.html"
#define ENDGAME_PAGE "./html/6_endgame.html"
#define GAMEOVER_PAGE "./html/7_gameover.html"

//Post patterns
#define QUIT_PATTERN "quit=Quit"
#define QUIT_PATTERN_LEN 9
#define GUESS_PATTERN "keyword="
#define GUESS_PATTERN_LEN 8
#define USER_PATTERN "user="
#define USER_PATTERN_LEN 5

// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
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

static bool handle_simple_get(int sockfd, const char *page);
static bool handle_root_page(int sockfd, METHOD method, char *post_data);
static bool handle_start_page(int sockfd, METHOD method, char *post_data);
static bool handle_quit_page(int sockfd);


static bool handle_http_request(int sockfd)
{
    // initialise before required to print a 404 on server
    int writeN;
    // try to read the request
    char buff[2049];
    bzero(buff, sizeof(buff));
    int n = read(sockfd, buff, 2049);
    printf("\nSockfd being read from: %d\n", sockfd);
    printf("\n%s\n", buff);

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

    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;

    // if URL '/', root page
    if (*curr == ' ') {
        handle_root_page(sockfd, method, post_data);
    }

    // The game playing page
    if (strncmp(curr, "?start=Start", 12) == 0) {
        curr += 12;
        handle_start_page(sockfd, method, post_data);
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

static bool handle_root_page(int sockfd, METHOD method, char *post_data) {
    if (method == GET)
    {
        handle_simple_get(sockfd, INTRO_PAGE);
    }
    else if (method == POST)
    {
        printf("The post data in root page is: %s\n", post_data);
        // Store the username
        if (!strncmp(post_data, QUIT_PATTERN, QUIT_PATTERN_LEN)) {
            handle_quit_page(sockfd);
        } else if (!strncmp(post_data, USER_PATTERN, USER_PATTERN_LEN)) {
            printf("Storing username: next \n");
            handle_simple_get(sockfd, START_PAGE);
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

static bool handle_start_page(int sockfd, METHOD method, char *post_data) {
    if (method == GET)
    {
        handle_simple_get(sockfd, FIRST_TURN_PAGE);
    } else if (method == POST) {
        printf("The post data in start page is: %s\n", post_data);

        // is it quit?
        if (!strncmp(post_data, QUIT_PATTERN, QUIT_PATTERN_LEN)) {
            handle_quit_page(sockfd);
        }
        // is it guess?
        if (!strncmp(post_data, GUESS_PATTERN, GUESS_PATTERN_LEN)) {
            char* guess_word = strstr(post_data, GUESS_PATTERN) + GUESS_PATTERN_LEN;
            // '&' is the beginning of the second POST argument guess=Guess
            char *clean_guess = strtok(guess_word, "&");
            printf("A user guessed this word: %s\n", clean_guess);
            // Check this guess against their list, then if not there, against other user's list
            
        }

    } else {
        // never used, just for completeness
        fprintf(stderr, "no other methods supported");
    }
    return true;
}

static bool handle_quit_page(int sockfd) {
    // may have to delete and reset some shit.
    handle_simple_get(sockfd, ENDGAME_PAGE);
    return true;
}

// Just returns the contents of the html at page
static bool handle_simple_get(int sockfd, const char* page) {
    // get the size of the file

    printf("Handle simple get returning: %s\n", page);
    struct stat st;
    stat(page, &st);
    char buff[2049];
    int n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
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
                else if (!handle_http_request(i))
                {
                    close(i);
                    FD_CLR(i, &masterfds);
                }
            }
    }

    return 0;
}
