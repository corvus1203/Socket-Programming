#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <map>
#include <set>
#include <string>

#define A_UDP_PORT "30126"
#define B_UDP_PORT "31126"
#define MAIN_UDP_PORT "32126"
#define MAIN_TCP_PORT "33126"
#define HOST "localhost"
#define BACKLOG 10

using namespace std;

void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6 --- From Beej
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void list_country(set<string>, set<string>);

int main(){

    // Setup UDP structure -- From Beej
    int udp_sockfd;
    struct addrinfo udp_hints, *udp_servinfo, *udp_p;
    int udp_rv;

    memset(&udp_hints, 0, sizeof udp_hints);
    udp_hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    udp_hints.ai_socktype = SOCK_DGRAM;
    udp_hints.ai_flags = AI_PASSIVE; // use my IP address

    if ((udp_rv = getaddrinfo(HOST, MAIN_UDP_PORT, &udp_hints, &udp_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s€n", gai_strerror(udp_rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can -- From Beej
    for(udp_p = udp_servinfo; udp_p != NULL; udp_p = udp_p->ai_next) {
        if ((udp_sockfd = socket(udp_p->ai_family, udp_p->ai_socktype,
                                 udp_p->ai_protocol)) == -1) {
            perror("ServerA UDP socket");
            continue;
        }
        if (bind(udp_sockfd, udp_p->ai_addr, udp_p->ai_addrlen) == -1) {
            close(udp_sockfd);
            perror("ServerA bind");
            continue;
        }
        break; // if we get here, we must have connected successfully
    }
    if (udp_p == NULL) {
        // looped off the end of the list with no successful bind
        fprintf(stderr, "failed to bind socket€n");
        exit(2);
    }

    // setup tcp -- From Beej
    int tcp_sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo tcp_hints, *tcp_servinfo, *tcp_p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int tcp_rv;

    memset(&tcp_hints, 0, sizeof tcp_hints);
    tcp_hints.ai_family = AF_UNSPEC;
    tcp_hints.ai_socktype = SOCK_STREAM;
    tcp_hints.ai_flags = AI_PASSIVE; // use my IP

    if ((tcp_rv = getaddrinfo(HOST, MAIN_TCP_PORT, &tcp_hints, &tcp_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(tcp_rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(tcp_p = tcp_servinfo; tcp_p != NULL; tcp_p = tcp_p->ai_next) {
        if ((tcp_sockfd = socket(tcp_p->ai_family, tcp_p->ai_socktype,
                                 tcp_p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(tcp_sockfd, tcp_p->ai_addr, tcp_p->ai_addrlen) == -1) {
            close(tcp_sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(tcp_servinfo); // all done with this structure

    if (tcp_p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(tcp_sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    cout << "The Main server is up and running." << endl;
    map<int, set<string>> countryMap;

    struct sockaddr_in servaddr;
    socklen_t fromlen;

    memset((char*)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    char bufA[1024];
    servaddr.sin_port = htons(atoi(A_UDP_PORT));

    // send the message to server A to ask the country list.
    if (sendto(udp_sockfd, bufA, sizeof bufA, 0, (struct sockaddr*)&servaddr, sizeof servaddr) == -1) {
        perror("Main sednto ServerA");
        exit(1);
    }
    fromlen = sizeof servaddr;
    if (recvfrom(udp_sockfd, bufA, sizeof bufA, 0, (struct sockaddr*)&servaddr, &fromlen) == -1) {
        perror("Main receive from Server A");
        exit(1);
    }
    char *tokens;
    tokens = strtok(bufA, " ");
    set<string> setA;
    while(tokens != NULL){
        setA.insert(string(tokens));
        tokens = strtok(NULL, " ");
    }
    countryMap[0] = setA;
    printf("The Main server has received the country list from server A using UDP over port<%s>\n", MAIN_UDP_PORT);


    servaddr.sin_port = htons(atoi(B_UDP_PORT));
    char bufB[1024];
    // send the message to server B to ask the country list.
    if (sendto(udp_sockfd, bufB, sizeof bufB, 0, (struct sockaddr*)&servaddr, sizeof servaddr) == -1) {
        perror("Main sednto Server B");
        exit(1);
    }
    fromlen = sizeof servaddr;
    if (recvfrom(udp_sockfd, bufB, sizeof bufB, 0, (struct sockaddr*)&servaddr, &fromlen) == -1) {
        perror("Main receive from Server B");
        exit(1);
    }
    tokens = strtok(bufB, " ");
    set<string> setB;
    while(tokens != NULL){
        setB.insert(string(tokens));
        tokens = strtok(NULL, " ");
    }
    countryMap[1] = setB;
    printf("The Main server has received the country list from server B using UDP over port<%s>\n", MAIN_UDP_PORT);
    list_country(setA, setB);

    // keep receiving the queries from the clients and sending the message to server A/B, the receicing the result from server A/B and send it back
    // to the clients.
    while(1){
        sin_size = sizeof their_addr;
        new_fd = accept(tcp_sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);

        // use fork to create child process in order to have multiple connections.
        if(!fork()){
            char buf[1024];
            close(tcp_sockfd); // child doesn't need the listener
            if (recv(new_fd, buf, sizeof buf, 0) == -1){
                perror("send");
                exit(1);
            }
            string user_id;
            string country;
            string result;
            user_id = string(strtok(buf, " "));
            country = string(strtok(NULL, " "));
            printf("The Main server has received the request on User<%s> in <%s> from the client using TCP over port <%s>\n", user_id.c_str(), country.c_str(), MAIN_TCP_PORT);
            if(setA.find(country) == setA.end() && setB.find(country) == setB.end()){
                printf("<%s> does not show up in server A&B\n", country.c_str());
                printf("The Main Server has sent \"Country Name: Not found\" to the client using TCP over port<%s>\n", MAIN_TCP_PORT);
                result = "-1 -1";
            }else{

                printf("<%s> shows up in server A/B\n", country.c_str());
                printf("The Main Server has sent request from User<%s> to server A/B using UDP over port <%s>\n", user_id.c_str(), MAIN_UDP_PORT);

                memset((char*)&servaddr, 0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                if(setA.find(country) != setA.end()){
                    servaddr.sin_port = htons(atoi(A_UDP_PORT));
                }else{
                    servaddr.sin_port = htons(atoi(B_UDP_PORT));
                }
                string query = user_id + " " + country;
                if ((sendto(udp_sockfd, query.c_str(), strlen(query.c_str()) + 1, 0, (struct sockaddr*)&servaddr, sizeof servaddr)) == -1) {
                    perror("Main sednto Server A/B");
                    exit(1);
                }
                char response[1024];
                fromlen = sizeof servaddr;
                if (recvfrom(udp_sockfd, response, sizeof response, 0, (struct sockaddr*)&servaddr, &fromlen) == -1) {
                    perror("Main receive from Server A/B");
                    exit(1);
                }
                int friends = atoi(strtok(response, " "));
                if(friends == -2){
                    if(setA.find(country) != setA.end()){
                        printf("The Main server has received \"User ID: Not found\" from server A\n");
                    }else {
                        printf("The Main server has received \"User ID: Not found\" from server B\n");
                    }
                    printf("The Main Server has sent error to client using TCP over port <%s>\n", MAIN_TCP_PORT);
                    result = "-2 -2";
                }else{
                    if(setA.find(country) != setA.end()){
                        printf("The Main server has received searching result(s) of User<%s> from server A\n", user_id.c_str());
                    }else{
                        printf("The Main server has received searching result(s) of User<%s> from server B\n", user_id.c_str());
                    }
                    printf("The Main Server has sent searching result(s) to client using TCP over port <%s>\n", MAIN_TCP_PORT);
                    if(friends == -3){
                        result = "-3 None";
                    } else{
                        result = "0 " + to_string(friends);
                    }
                }
            }
            if(send(new_fd, result.c_str(), strlen(result.c_str()) + 1, 0) == -1){
                perror("send");
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd); // parent doesn't need this }
    }
    return 0;
}

// function the used to print the country list in server A and B.
void list_country(set<string> A, set<string> B){
    set<string>::iterator itA = A.begin();
    set<string>::iterator itB = B.begin();
    cout << "Server A          | Server B          " << endl;
    while(itA != A.end() || itB != B.end()){
        string ap = "";
        string bp = "";
        if(itA != A.end()){
            ap = *itA;
            itA++;
        }
        if(itB != B.end()){
            bp = *itB;
            itB++;
        }
        printf("%-18s| %s\n", ap.c_str(), bp.c_str());
    }
}
