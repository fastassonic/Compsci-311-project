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
#include <pthread.h>

#define SERV_TCP_PORT 23 /* server's port */

 void *recvThread(void * socketPtr){
    //This function will recieve the data from the server.
    int sockfd = *(int*) socketPtr;
    int x;
    char recieveBuffer [1504];

    //Here we loop through and recieve data from the server while checking for any errors.
    //If there is an error we exit the thread and close the socket(x<0).
    //Similarly, we exit thread and close socket if the connection is closed(x==0).
    //Otherwise, we just print the data we recieved as a message.
    while(1){
    x=recv(sockfd,recieveBuffer,sizeof(recieveBuffer)-1,0);
    if(x < 0){
    perror("Could not recieve message");
    close(sockfd);
    pthread_exit(NULL);
    }else if(x == 0){
    printf("\nConnection closed\n");
    close(sockfd);
    pthread_exit(NULL);
    }else {
        //This line makes sure that we dont print the whole buffer always.
        recieveBuffer[x] = '\0';
        printf("%s\n",recieveBuffer);
    }

}

}

void *sendThread(void * arg){
    //In this function we send the data to the server that the client types.
    int sockfd = *(int *) arg;
    char sendBuffer[1504];
    int sent;
    size_t stringLength;

    //This loop listens to the client and then sends the data that is typed.
    //It also listesns for errors and exits thread and closes the socket.
    while(1){
    fgets(sendBuffer, sizeof(sendBuffer), stdin);
    stringLength = strlen(sendBuffer);
    sent = send(sockfd,sendBuffer,stringLength, 0);
    if( sent < 0){ perror("Could not send message");
    close(sockfd);
    pthread_exit(NULL);
        }
    }
}


int main(int argc , char *argv[]) {

    int sockfd;
    struct sockaddr_in serv_addr;
    char *serv_host = "localhost";
    struct hostent *host_ptr;

    int port;
    int buff_size = 0;
    pthread_t t1,t2;

    //Read the host and the port if the user gives it otherwise sets to default.
    if(argc >= 2)
        serv_host = argv[1];
    if(argc == 3)
        sscanf(argv[2], "%d", &port);
    else
        port = SERV_TCP_PORT;

    //Get the address of the host and checks for error.
    if((host_ptr = gethostbyname(serv_host)) == NULL) {
        perror("There was an error with gethostbyname()");
        exit(1);
    }

    //Checks if thre address is IPv4,if not then print an error message.
    if(host_ptr->h_addrtype !=  AF_INET) {
        perror("The address type is unkown");
        exit(1);
    }

    //Set the struct to 0 and clear the information first. Then set/fill in the serv_addr struct.
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    //This line specifically sets the address. It does this by casting the address which we got from gethostbyname to struct in_addr. T
    //his changes the pointer type from char to in_addr. in_addr has one field s_addr(unint32_t) which is what stores the address. This
    //entire process allows us to access s_addr and copy to serv_addr.sin_addr.s_addr.
    serv_addr.sin_addr.s_addr = ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;
    serv_addr.sin_port = htons(port);

    //Opens the TCP Socket, checks for errors and prints it.
    sockfd= socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Could not open the stream socket");
        exit(1);
    }

    //Connects to the server and checks for error.

    if ((connect(sockfd,(const struct sockaddr*) &serv_addr, sizeof(serv_addr)))<0) {
        perror("Could not connect to the server");
        exit(1);
    }

    //Create the Send and Recieve thread also check for errors
     if(pthread_create(&t1, NULL, recvThread, &sockfd) != 0){
        perror("The recieve thread was not created.");
        close(sockfd);
        exit(1);
    }
     if(pthread_create(&t2, NULL, sendThread, &sockfd) != 0){
         perror("The send thread was not created.");
         close(sockfd);
         exit(1);

    }

    //Join the 2 threads togehter and check for errors.
     if(pthread_join(t1, NULL) != 0){
    perror("The recieve thread was not joined.");
    }
     if(pthread_join(t2, NULL) != 0) {
    perror("The send thread was not joined.");
    }

    //Still close the socket just in case.
    close(sockfd);

}