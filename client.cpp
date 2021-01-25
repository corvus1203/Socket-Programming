#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>


#define PORT "33126" // the port client will be connecting to
#define MAXDATASIZE 512 // max number of bytes we can get at once
#define HOST "localhost"

using namespace std;

// get sockaddr, IPv4 or IPv6: --- From Beej
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr); }

int main(int argc, char *argv[]) {
    int sockfd = 0, numbytes = 0;

    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 1) {
        fprintf(stderr,"usage: wrong input\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(HOST, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    printf("The client is up and running.\n");
    
    while(1){

        // loop through all the results and connect to the first we can --- From Beej
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
                                 p->ai_protocol)) == -1) {
                perror("client: socket");
                continue;
            }
            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                perror("client: connect");
                continue;
            }
            break;
        }
        if (p == NULL) {
            fprintf(stderr, "client: failed to connect\n");
            return 2;
        }
        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

        // acquire the query from user input
        cout << "Please enter the User ID: ";
        string user_id;
        cin >> user_id;
        cout << "Please enter the Country Name: ";
        string country_name;
        cin >> country_name;

        string input = user_id + " " + country_name;

        // sending the query to main server.
        if((numbytes = send(sockfd, input.c_str(), strlen(input.c_str()) + 1, 0)) == -1) {
            perror("send");
            exit(1);
        }
        printf("The client has sent User<%s> and <%s> to Main Server using TCP\n", user_id.c_str(), country_name.c_str());

        // receive the result from main server and interprete it.
        char buf[512];
        if ((numbytes = recv(sockfd, buf, sizeof buf, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        char *tokens;
        int result = 0;
        tokens = strtok(buf, " ");
        result = atoi(tokens);
        // output will like "0 35" or "-1 -1", or "-2 -2"
        if(result == -1 || result == -2){
            if(result == -1){
                printf("<%s> not found\n", country_name.c_str());
                //close(sockfd);
                //exit(1);
            }else if(result == -2){
                printf("User<%s> not found\n", user_id.c_str());
                //close(sockfd);
                //exit(1);
            }
        }else{
            string ans = strtok(NULL, " ");
            printf("The client has received results from Main Server:\nUser<%s> is possible friend(s) of User<%s> in %s\n", ans.c_str(), user_id.c_str(), country_name.c_str());
        }
        printf("\n");
        cout << "----- Start a new request -----" << endl;
        close(sockfd);
    }
    freeaddrinfo(servinfo); // all done with this structure
    //close(sockfd);
    return 0;
}
