#include "cs537.h"
#include "request.h"
//
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//





pthread_mutex_t m;
pthread_cond_t empty, fill;
int numrequests = 0;
int fillptr = 0;
//int useptr = 0;
int bufsize;
buff_t* buffer = NULL; //head of list
buff_t* tail = NULL; //end of list



void fill_buff(buff_t* node);
buff_t* get_buff();
void *consumer(void *arg);

// CS537: Parse the new arguments too
void getargs(int *port, int *numworkers, int *bufsize, int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <# worker threads> <buffer size>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *numworkers = atoi(argv[2]);
    *bufsize = atoi(argv[3]);
}


int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, numworkers;
    struct sockaddr_in clientaddr;
    
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    
    pthread_mutex_init(&m, NULL); // lock for producer and consumer loops
    pthread_cond_init(&empty, NULL); // CV for master/producer to wait on
    pthread_cond_init(&fill, NULL); // CV for workers/consumers to wait on
    
    getargs(&port, &numworkers, &bufsize, argc, argv);
    
    //
    // CS537: Create some threads...
    //
    pthread_t threads[numworkers]; // array of worker threads
    int i;
    for (i = 0; i < numworkers; i++) {
        pthread_create(&threads[i], NULL, consumer, NULL);
    }
    
    
    listenfd = Open_listenfd(port);
    
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        
        pthread_mutex_lock(&m);
        buff_t* newNode = malloc(sizeof(buff_t));
        newNode->fd = connfd;
        //fprintf(stderr, "acquired lock\n");
        
        
        
        Rio_readinitb(&rio, newNode->fd);
        Rio_readlineb(&rio, buf, MAXLINE);
        sscanf(buf, "%s %s %s", method, uri, version);
        //printf("AFTER SCAN: %s %s %s\n", method, uri, version);
        
        requestReadhdrs(&rio);
        
        is_static = requestParseURI(uri, filename, cgiargs);
        //printf("AFTER PARSE: uri->%s filename->%s cgiargs->%s\n",uri,filename,cgiargs);
        
        newNode->buf = buf;
        newNode->rio = rio;
        //fprintf(stderr,"new buf: %s\n",newNode->buf);
        newNode->method = method;
        newNode->uri = uri;
        newNode->version= version;
        newNode->is_static = is_static;
        newNode->filename = filename;
        newNode->filenamesize = strlen(filename);
        stat(filename, &sbuf);
        newNode->filesize = sbuf.st_size;
        
        newNode->cgiargs = cgiargs;
        printf("%d\n",newNode->filesize);
        
        
        
        
        while(numrequests == bufsize) {
            //fprintf(stderr, "producer waiting\n");
            pthread_cond_wait(&empty, &m);
        }
        //fprintf(stderr, "accepted\n");
        //fprintf(stderr, "connfd: %d\n", connfd);
        fill_buff(newNode);
        //fprintf(stderr, "done filling\n");
        //requestHandle(connfd);
        pthread_cond_signal(&fill);
        pthread_mutex_unlock(&m);
        fprintf(stderr, "producer released lock\n");
        //fprintf(stderr, "numrequests: %d\n", numrequests);
        //
        // CS537: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads
        // do the work. However, for SFF, you may have to do a little work
        // here (e.g., a stat() on the filename) ...
        //
        //requestHandle(connfd);
        
        //Close(connfd);
    }
}

void fill_buff(buff_t *node) { //First In first out
    fprintf(stderr,"buffer fill: %p\n",buffer);
    if(buffer ==NULL){
        buffer = node;
	tail = node;
    }
    else{
        buff_t* tmp = tail;
	tmp->next = node;
	tail = node;
	tail->previous = tmp;	
    }
    fprintf(stderr,"buffer after fill: %p\n",buffer);
    numrequests++;
}

void fill_buff2(buff_t *node){ //smallest filename first
    if(buffer == NULL){
        buffer = node;
    }
    else{
        buff_t* tmp = buffer;
        while(tmp !=NULL){
            if(node->filenamesize < tmp->filenamesize){
                if(tmp->previous==NULL){//front of list
                    buffer=node;
                    buffer->next = tmp;
                    tmp->previous = buffer;
                    break;
                }
                else{
                    buff_t* prevTmp = tmp->previous;
                    node->previous= prevTmp;
                    prevTmp->next = node;
                    node->next = tmp;
		    break;
                }
            }
            else{
                if(tmp->next == NULL){//if at end of list
                    tmp->next = node;
                    break;
                }
                tmp = tmp->next;
            }
        }
    }
    
}


void fill_buff3(buff_t *node){ //smallest file first
    if(buffer == NULL){
        buffer = node;
    }
    else{
        buff_t* tmp = buffer;
        while(tmp !=NULL){
            if(node->filesize < tmp->filesize){
                if(tmp->previous==NULL){//front of list
                    buffer=node;
                    buffer->next = tmp;
                    tmp->previous = buffer;
                    break;
                }
                else{
                    buff_t* prevTmp = tmp->previous;
                    node->previous= prevTmp;
                    prevTmp->next = node;
                    node->next = tmp;
		    break;
                }
            }
            else{
                if(tmp->next == NULL){//if at end of list
                    tmp->next = node;
                    break;
                }
                tmp = tmp->next;
            }
        }
    }
    
}


buff_t* get_buff() {
    buff_t* tmp = buffer;
    fprintf(stderr,"buffer get: %p\n",buffer);
    //int tmpfd = (int)tmp->fd;
    buffer = buffer->next;
    fprintf(stderr,"buffer after get: %p\n",buffer);
    numrequests--;
    return tmp;
}


void *consumer(void *arg) {
    while(1) {
        pthread_mutex_lock(&m);
        
        while(numrequests == 0) {
            //fprintf(stderr, "numrequests: %d\n", numrequests);
            pthread_cond_wait(&fill, &m);
            //fprintf(stderr, "return from wait\n");
            //fprintf(stderr, "numrequests2: %d\n", numrequests);
            
        }
        //fprintf(stderr, "consumer got lock\n");
        
        // service request
        buff_t* connfd = get_buff();
        fprintf(stderr, "connfd: %d\n", connfd->fd);
        fprintf(stderr,"connfd->filename: %s\n",connfd->filename);
        requestHandle(connfd);
        Close(connfd->fd);
        free(connfd);
        //fprintf(stderr, "servicing request\n");
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&m);
        
    }
}
