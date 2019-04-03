/*
This file is the center of the image_tagger server
Created by: Kyle Zsembery, Student Number: 911920
*/

#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
// #include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// IP and port (+1 for filename)
#define EXPECTED_ARGS 3


int main(int argc, char const *argv[]) {
    if (argc != EXPECTED_ARGS) {
        printf("Program execution should contain arguments <server_ip> and <port_number>\n");
        exit(1);
    }
    assert(argc == EXPECTED_ARGS);
    printf("image_tagger server is now running at IP: %s on port %s\n", argv[1], argv[2]);

    return 0;
}
