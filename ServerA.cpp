#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>


#define MYPORT "30126" // the port client will be connecting to
#define HOST "localhost"

using namespace std;

struct Graph {
    map<int, set<int>> graphMap;
};

map<string, Graph> constructMap();
int find_friends(map<string, Graph>, int , string);


int main(int argc, char *argv[]) {

    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(HOST, MYPORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can -- From Beej
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("ServerA UDP socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("ServerA bind");
            continue;
        }
        break; // if we get here, we must have connected successfully
    }

    if (p == NULL) {
        // looped off the end of the list with no successful bind
        fprintf(stderr, "failed to bind socket€n");
        exit(2);
    }

    freeaddrinfo(servinfo); // all done with this structure
    printf("The Server A is up and running using UDP on port <%s>.\n", MYPORT);

    // construct data from input data.txt to our dataMap
    map<string, Graph> dataMap = constructMap();
    string countryName = "";
    int first = 0;

    // form country list in order to send it back to main server
    for(map<string, Graph>::iterator st = dataMap.begin(); st != dataMap.end(); st++){
        if(first == 0) {
            countryName = st->first;
            first = 1;
        }else{
            countryName += " " + st->first;
        }
    }

    char boot[512];
    struct sockaddr_in addr;
    socklen_t fromlen = sizeof addr;
    // receive message from main server asking the country list
    if (recvfrom(sockfd, boot, sizeof boot, 0, (sockaddr*)&addr, &fromlen) == -1) {
        perror("ServerA receive");
        exit(1);
    }
    // sent back the country list
    if (sendto(sockfd, countryName.c_str(), strlen(countryName.c_str()) + 1, 0, (sockaddr*)&addr, sizeof addr) == -1) {
        perror("ServerA response");
        exit(1);
    }
    printf("The server A has sent a country list to Main Server\n");

    // keep receiving the queries from main server and sending the result back to main server.
    while(1){
        char buf[512];

        if (recvfrom(sockfd, buf, sizeof buf, 0, (sockaddr*)&addr, &fromlen) == -1) {
            perror("ServerA receive");
            exit(1);
        }
        char *tokens;
        int user_id;
        tokens = strtok(buf, " ");
        user_id = atoi(tokens);
        string country = strtok(NULL, " ");

        printf("The server A has received request for finding possible friends of User<%d> in <%s>\n", user_id, country.c_str());

        int friends = find_friends(dataMap, user_id, country);
        if(friends == -2){
            printf("User<%d> does not show up in <%s>\n", user_id, country.c_str());
            printf("The server A has sent \"User<%d> not found\" to Main Server\n", user_id);
        }else{
            if(friends == -3){
                printf("The server A is searching possible friends for User<%d>...\nHere are the results: User<NONE>\n", user_id);
            }else{
                printf("The server A is searching possible friends for User<%d>...\nHere are the results: User<%d>\n", user_id, friends);
            }
            
            cout << "The Server A has sent the result(s) to Main Server." << endl;;
        }
        string result = to_string(friends) + " ";
        if (sendto(sockfd, result.c_str(), strlen(result.c_str()) + 1, 0, (sockaddr*)&addr, sizeof addr) == -1) {
            perror("ServerA response");
            exit(1);
        }

    }
    return 0;
}

// function that construct the map for input data
map<string, Graph> constructMap() {
    map<string, Graph> result;
    ifstream infile;
    infile.open("./data1.txt");

    int key = 0;
    map<int, set<int>> graphMap;
    string line;
    string country;

    while(getline(infile, line)) {
        if(line.size() < 1){
            continue;
        }
        istringstream ss(line);
        string temp;
        ss >> temp;
        if(isalpha(line[0])){
            if(key == 1) {
                Graph g = {graphMap};
                result[country] = g;
            }
            key = 1;
            country = temp;
            graphMap.clear();
        }else{
            int node = stoi(temp);
            set<int> set;
            int friends;
            while(ss >> friends){
                set.insert(friends);
            }
            graphMap[node] = set;
        }
    }
    Graph g = {graphMap};
    result[country] = g;
    infile.close();
    return result;
}

// function that handle the query from main server
int find_friends(map<string, Graph> dataMap, const int user_id, const string country){
    map<int, set<int>> graphMap;
    graphMap = dataMap[country].graphMap;
    if(graphMap.find(user_id) == graphMap.end()){
        return -2; //not this user int the country;
    }
    set<int> user_friend = graphMap[user_id];
    if(user_friend.size() + 1 == graphMap.size()){
        return -3; //return NONE
    }
    int commonNeighbor = 0;
    int friend_id = -3;
    int degree = 0;
    for(map<int, set<int>>::iterator it = graphMap.begin(); it != graphMap.end(); it++){
        if(it->first == user_id){
            continue;
        }
        int temp_id = it->first;
        set<int> friend_set = it->second;
        if(user_friend.find(temp_id) != user_friend.end()){
            continue;
        }
        int shares = 0;
        int temp_degree = friend_set.size();
        for(set<int>::iterator st = friend_set.begin(); st != friend_set.end(); st++){
            if(user_friend.find(*st) != user_friend.end()) {
                shares++;
            }
        }
        if(shares > commonNeighbor){
            friend_id = temp_id;
            commonNeighbor = shares;
        }else if(shares != 0 && shares == commonNeighbor){
            if(friend_id > temp_id){
                friend_id = temp_id;
            }
        }else if(shares == 0){
            if(commonNeighbor != 0){
                continue;
            }
            if(friend_id == -3 || temp_degree > degree){
                friend_id = temp_id;
                degree = temp_degree;
            }else if(temp_degree == degree){
                if(friend_id > temp_id){
                    friend_id = temp_id;
                }
            }
        }
    }
    return friend_id;
}