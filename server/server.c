    /*
        For those of you who do not know what you're looking at
        DO NOT RUN THIS IN ITS CURRENT STATE
        I HAVE GOTTEN THIS INTO A STATE WHERE IT WORKS IN AN ABSOLUTELY PERFECT ENVIROMENT
        REALITY IS NOT PERFECT
        IT WILL CAUSE CORRUPTION IF RAN NOW. I'M ONLY COMMITING THIS BECAUSE I NEED TO STOP FOR TODAY. 
        I WILL BE FIXING THIS LATER.

    */


    #include<stdlib.h>
    #include <signal.h>
    #include <string.h>
    #include <stdio.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/eventfd.h>
    #include <sys/select.h>
    #define SERV_TCP_PORT 23 /* server's port number */
    #define MAX_SIZE 1500

    struct clientthreadargs {
        int clientsocketfiledescriptor;
        struct sockaddr_in client_addr;
    };


    static pthread_mutex_t broadcastlistmtx = PTHREAD_MUTEX_INITIALIZER;
    // with the broadcaster thread the rwlock was redudant 
    pthread_mutex_t messagelock = PTHREAD_MUTEX_INITIALIZER;
    char **messages = NULL;
    size_t numofmessages = 0;


    int efd;
    int globalstop;
    int *broadcastfds = NULL;
    size_t broadcastsize = 0;
    static void signal_handler(int signo)
    {
        if (signo == SIGTERM)
        {
            printf("Shutting down!\n");
            globalstop=1;

        }
        else{
            exit (EXIT_SUCCESS);
        }

        
        //default action
    }
    void *clientthread(void *arg){
        struct clientthreadargs *args = arg;
        int clientfd = args->clientsocketfiledescriptor;
        //this saves the ip address of the client, Honestly, this may be unnecessary but useful for persistent client info
        uint32_t ip_address = args->client_addr.sin_addr.s_addr;
        //Our structure is no longer necessary and we can free up the pointer, removing the information from the global stack and removing an overflow
        free(args);
        //let our client know we can see them and write to them.
        write(clientfd,"Initial connected",strlen("Initial connected"));
        int connected = 1;
        while ((connected && !globalstop))
        {
            char string[MAX_SIZE];
            int messagelength;
            messagelength = read(clientfd, string, MAX_SIZE-1);
            //This is to terminate the stirng 
            
            if (messagelength <= 0){
                connected = 0;
                pthread_mutex_lock(&broadcastlistmtx);
                int index = -1;
                for (int i = 0; i < broadcastsize; i++) {
                    if (broadcastfds[i] == clientfd) {
                        index = i;
                        break;
                    }
                }
                // This is apperently the good pactice using 
                if (index != -1){
                    for (int i = index; i < broadcastsize - 1; i++) {
                        broadcastfds[i] = broadcastfds[i + 1];
                    }
                    broadcastsize--;    
                    if (broadcastsize > 0) {
                        //reallocating to a temproary value for safety purposes
                        int *temp = realloc(broadcastfds, sizeof(int) * broadcastsize);
                        if (temp != NULL) {
                            broadcastfds = temp; 
                        }
                        // if it's lessthen/equal to zero, lets jsut restart broadcast fds-
                        } else {
                            free(broadcastfds);
                            broadcastfds = NULL;
                        }
                    
                }
                pthread_mutex_unlock(&broadcastlistmtx);
            } else{
                string[messagelength] = '\0';
                pthread_mutex_lock(&messagelock);
                char** temp = realloc(messages,sizeof(char*) * (numofmessages+1));
                if (temp == NULL){
                    //If realloc failes. do this
                    pthread_mutex_unlock(&messagelock);
                    continue;
                }
                char *copy = strdup(string);
                if(copy == NULL){pthread_mutex_unlock(&messagelock); continue;}
                messages=temp;
                messages[numofmessages] = copy;
                numofmessages++;
                u_int64_t u = 1;
                write(efd,&u,sizeof(u_int64_t));
                pthread_mutex_unlock(&messagelock);
                
            }
        }
        close(clientfd);
    }
        
        
        




    void *broadcastthread(void *arg){
        int lastreadindex = -1;
        u_int64_t u;
        while(!globalstop){
            int size = read(efd,&u,sizeof(u));
            if (size != sizeof(u)) {
                continue;
            }
            for (int n=0; n<u;n++){
                lastreadindex++;
                pthread_mutex_lock(&messagelock);
                char* tempmessage = strdup(messages[lastreadindex]);
                pthread_mutex_unlock(&messagelock);
                if (!tempmessage) continue;
                pthread_mutex_lock(&broadcastlistmtx);
                for(int i=0;i<broadcastsize;i++){
                    write(broadcastfds[i],tempmessage,strlen(tempmessage));
                }
                pthread_mutex_unlock(&broadcastlistmtx);
                free(tempmessage);
            }
        }
        
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
        // this is the event file descriptor 
        efd = eventfd(0,0);
        if (efd == -1) {
            perror("eventfd");
            exit(1);
        }
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

    globalstop=0;
    /* listen to the socket. the secound */
    listen(listeningfiledescriptor, 15);
    signal(SIGTERM, signal_handler);
    //this is for a lazy implementation of client cleanup. the client should be responsible for disposing of itself, not the broadcast thread. this just stops it from crashing on a failed write.
    signal(SIGPIPE, SIG_IGN);
    pthread_t broadcastthreadtid;
    pthread_create(&broadcastthreadtid,NULL,broadcastthread,NULL);
    
    //keep track fo the thread index
    while (!globalstop) {
        struct clientthreadargs *tempclient;
        tempclient = malloc(sizeof( *tempclient));
        //temporarily stores the client address, this will get saved to the thread and freed up here
        //this will get overwritten, but this is fine
        cillen=sizeof(tempclient->client_addr);
        tempclient->clientsocketfiledescriptor = accept(listeningfiledescriptor, (struct sockaddr *) &tempclient->client_addr, &cillen);
        if (tempclient-> clientsocketfiledescriptor != -1){
            pthread_mutex_lock(&broadcastlistmtx);
            int *temp = realloc(broadcastfds,sizeof(int) * (broadcastsize+1));
            if (temp == NULL){
                //only happens if realloc fails. It should not fail but-
                pthread_mutex_unlock(&broadcastlistmtx);
                continue;
            }
            broadcastfds = temp;
            broadcastfds[broadcastsize] = tempclient->clientsocketfiledescriptor;
            broadcastsize++;
            pthread_mutex_unlock(&broadcastlistmtx);
            //okay now we have to make our two important pieces of information
            pthread_t tid;
            pthread_create(&tid,NULL,clientthread,tempclient);
            pthread_detach(tid);
        }

        
    }
    pthread_join(broadcastthreadtid,NULL);

    free(broadcastfds);
    for(int i=0; i<numofmessages;i++){
        free(messages[i]);
    }
    free(messages);
    }