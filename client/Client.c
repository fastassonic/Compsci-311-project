#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

#define SERV_TCP_PORT 23 /* server's port */

int main(int argc , char *argv[]) {

    int sockfd;
    struct sockaddr_in serv_addr;
    char *serv_host = "localhost";
    struct hostent *host_ptr;

    int port;
    int buff_size = 0;


    //Read the host and the port if the user gives it otherwise sets to default.
    if(argc >= 2)
        serv_host = argv[1];
    if(argc == 3)
        sscanf(argv[2], "%d", &port);
    else
        port = SERV_TCP_PORT;

    //Get the address of the host.
    if((host_ptr = gethostbyname(serv_host)) == NULL) {
        perror("There was an error with gethostbyname()");
        exit(1);
    }

    if(host_ptr->h_addrtype !=  AF_INET) {
        perror("The address type is unkown");
        exit(1);
    }

    //Set the struct to 0 and clear the information first. Then set/fill in the serv_addr struct.
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    //This line specifically sets the address. It does this casting the address which we got from gethostbyname to struct in_addr. T
    //his changes the pointer type from char to in_addr. in_addr has one field s_addr(unint32_t) which is what stores the address. This
    //entire process allows us to access s_addr and copy to serv_addr.sin_addr.s_addr.
    serv_addr.sin_addr.s_addr = ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr);
    serv_addr.sin_port = htons(port);

    //Opens the TCP Socket
    sockfd= socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Could not open the stream socket");
        exit(1);
    }

    int connect(sockfd,(const struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if (connect < 0) {
        perror("Could not connect to the server");
        exit(1);
    }








    /* open tco socket */





    close(sockfd);

}