#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#define SERV_TCP_PORT 23 /* server's port */
static int clientGlobal = 1;
static pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;
char fifosToClient [64], fifosFromClient [64];

 void *recvThread(void * socketPtr){
    //This function will recieve the data from the server. and write to the FIFO.
    int sockfd = *(int*) socketPtr;
    int x;
    char recieveBuffer [1500];
    //Opens the write FIFO
    int writeFIFO = open(fifosToClient, O_WRONLY );
    if(writeFIFO < 0){
    perror("Could not open the write FIFO");
    return NULL;
    }

    //Here we loop through and recieve data from the server while checking for any errors.
    // Then we write that data using a FIFO.
    while(1){
    x=recv(sockfd,recieveBuffer,sizeof(recieveBuffer),0);
    if(x < 0){
    //Locks the mutex and sets the global signal to 0.Then unlocks the mutex
    pthread_mutex_lock(&clientMutex);
    clientGlobal = 0;
    pthread_mutex_unlock(&clientMutex);
    perror("Could not recieve message");
    break;
    }else if(x == 0){
    //If connection is closed then it locks mutex, sets the global signal to 0, and then unlocks mutex
    pthread_mutex_lock(&clientMutex);
    clientGlobal = 0;
    pthread_mutex_unlock(&clientMutex);
    printf("\nConnection closed\n");
    break;
    }else {
        write(writeFIFO, recieveBuffer,x);
    }

}
    //Close the FIFO.
     close(writeFIFO);
     return NULL;

}

void *sendThread(void * arg){
    //In this function we send the data to the server that the client types.
    int sockfd = *(int *) arg;
    char sendBuffer[1500];
    int sent, checkSend;
    //Opens the read FIFO and checks for errors
    int readFIFO = open(fifosFromClient, O_RDONLY);

    if(readFIFO < 0){
    perror("Could not open the read FIFO.");
    return NULL;
    }

    //This loop reads from the FIFO and then sends the data to the server.
    //It also checks for errors.
    while(1){
        //Checks if recieve thread is still running before asking for input.
    pthread_mutex_lock(&clientMutex);
    checkSend = clientGlobal;
    pthread_mutex_unlock(&clientMutex);
    if(!checkSend){
        break;
        }
    //Reads the data fromt the FIFO and then send it to the server.
    int j = read(readFIFO, sendBuffer, sizeof(sendBuffer));
    if( j <= 0){
    break;
    }

    sent = send(sockfd,sendBuffer,j, 0);
    //If send fails then it stops the loop and sets the global signal to 0.
    if( sent < 0){ perror("Could not send message");
    pthread_mutex_lock(&clientMutex);
    clientGlobal = 0;
    pthread_mutex_unlock(&clientMutex);
    break;
        }
    }
    close(readFIFO);
    return NULL;
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

    //Get each clients pid and add it to fifo name.Also give access to the threads.
    pid_t pid = getpid();
    sprintf(fifosToClient, "/tmp/toClient_%d", pid);
    sprintf(fifosFromClient, "/tmp/fromClient_%d", pid);


    //Create the Fifos using the mkfifo. Check for errors except if it exists then its fine.
    if(mkfifo(fifosToClient, 0666) < 0 && errno != EEXIST){
    perror("Could not create toClient with mkfifo()");
    exit(1);
    }

    if(mkfifo(fifosFromClient, 0666) < 0 && errno != EEXIST){
    perror("Could not create fromClient with mkfifo()");
    exit(1);
    }

    //Create the Send and Recieve thread also check for errors.
     if(pthread_create(&t1, NULL, recvThread, &sockfd) != 0 ){
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