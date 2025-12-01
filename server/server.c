#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#define SERV_TCP_PORT 23 /* server's port number */
#define MAX_SIZE 80

struct clientthreadargs {
    int clientsocketfiledescriptor;
    struct sockaddr_in client_addr;
};


/* PLEASE READ BEFORE YOU GET CONFUSED/WORRIED
I am aware this throws off vscodes intellesense. But this compiles
The type pthread_rwlock_t is defined in pthreads.h 
It's a mutex that allows simoltanous reading locks but one writing lock.
this means when all the clients are reading, they don't have to take turns, and it's still locked from writing 
this also means while writing ofcourse you can't read*/
pthread_rwlock_t messagelock = PTHREAD_RWLOCK_INITIALIZER;
char **messages = NULL;
size_t numofmessages = 0;

void *clientthread(void *arg){
    struct clientthreadargs *args = arg;
    int clientfd = args->clientsocketfiledescriptor;
    //this saves the ip address of the client, Honestly, this may be unnecessary but useful for persistent client info
    uint32_t ip_address = args->client_addr.sin_addr.s_addr;
    //Our structure is no longer necessary and we can free up the pointer, removing the information from the global stack and removing an overflow
    free(args);
    //let our client know we can see them and write to them.
    write(clientfd,"Initial connected",strlen("Initial connected"));



}
int main(int argc, char *argv[]){
  // socketfiledescriptor is used to define the listening socket.
  // this socket is used for incoming connections before they get assigned their own sockets
  int listeningfiledescriptor;
  //this is a variable mapping the size of the adress... This is ipv4 exclusive so it should be 32 bytes by definition, but this was the way we were taught. Might ask about that 
  int cillen;
    // sserv_addr is server address, which should be our ipv4 address 
  struct sockaddr_in serv_addr;
  int port;
   // Message buffer. If possible I'd like to ahve this dynamically allocated
  char string[MAX_SIZE];
  //len refers to the actual length of the message...
  int len;

  /* command line: server [port_number] */

  if(argc == 2)
   //tldr turn port number from string into int.
     sscanf(argv[1], "%d", &port); /* read the port number if provided */
  else
     port = SERV_TCP_PORT;

  /* open a TCP socket (an Internet stream socket) */
  //this is important to note down that this is not for any specific client. this is listening socket
  if((listeningfiledescriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
     perror("can't open stream socket");
     exit(1);
  }

  
  /* 
   In linux for for some funny reason the server stuff is handeled as files
   so this is essentially telling the files managing each managed ip address to look for things
   this is itself a struct.
    */
    //this zeros out the server adress up to the point of the actual address size
  bzero((char *) &serv_addr, sizeof(serv_addr));
  // This tells the struct to only work on IPV4
  serv_addr.sin_family = AF_INET;
  // listen on all ip addressees on this network 
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  // and which port to listen on
  serv_addr.sin_port = htons(port);

  //This tells all ip addressed to bind the corresponding ports. This matters to gabriel because I"m using a multiconnection systme for testing purposes.
  if(bind(listeningfiledescriptor, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
     perror("can't bind local address");
     exit(1);
  }


  /* listen to the socket. the secound */
  listen(listeningfiledescriptor, 15);

  //keep track fo the thread index
  for(;;) {
    struct clientthreadargs *tempclient;
    tempclient = malloc(sizeof( *tempclient));
    //temporarily stores the client address, this will get saved to the thread and freed up here
    //this will get overwritten, but this is fine
    cillen=sizeof(tempclient->client_addr);
    tempclient->clientsocketfiledescriptor = accept(listeningfiledescriptor, (struct sockaddr *) &tempclient->client_addr, &cillen);
    //okay now we have to make our two important pieces of information
    pthread_t tid;
    pthread_create(&tid,NULL,clientthread,tempclient);
    pthread_detach(tid);






     
  }
}