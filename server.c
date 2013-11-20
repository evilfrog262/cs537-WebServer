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
//int fillptr = 0;
//int useptr = 0;
int bufsize;
buff_t* buffer = NULL; //head of list
buff_t* tail = NULL; //end of list



void fill_buff(buff_t* node);
void fill_buff2(buff_t* node);
void fill_buff3(buff_t* node);
buff_t* get_buff();
void *consumer(void *arg);

// CS537: Parse the new arguments too
void getargs(int *port, int *numworkers, int *bufsize, char **sched, int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port> <# worker threads> <buffer size> <scheduling algorithm>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *numworkers = atoi(argv[2]);
    *bufsize = atoi(argv[3]);
    if ((strcmp(argv[4], "FIFO") != 0) && (strcmp(argv[4], "SFNF") != 0) && (strcmp(argv[4], "SFF") != 0)) {
      fprintf(stderr, "Use valid scheduling algorithm: FIFO, SFNF, SFF\n");
      exit(1);
    }
    *sched = argv[4];
}


int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, numworkers;
    char *sched;
    struct sockaddr_in clientaddr;
    
    struct stat sbuf;


    pthread_mutex_init(&m, NULL); // lock for producer and consumer loops
    pthread_cond_init(&empty, NULL); // CV for master/producer to wait on
    pthread_cond_init(&fill, NULL); // CV for workers/consumers to wait on
    
    getargs(&port, &numworkers, &bufsize, &sched, argc, argv);

    
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
        Rio_readinitb(&((*newNode).rio), newNode->fd);
        Rio_readlineb(&((*newNode).rio), newNode->buf, MAXLINE);
        sscanf(newNode->buf, "%s %s %s", newNode->method, newNode->uri, newNode->version);

        
        requestReadhdrs(&((*newNode).rio));
        
        newNode->is_static = requestParseURI(newNode->uri, newNode->filename, newNode->cgiargs);

        
        stat(newNode->filename, &sbuf);
        newNode->filesize = sbuf.st_size;
	newNode->filenamesize = strlen(newNode->filename);
        
        //newNode->cgiargs = cgiargs;
        
        
        
        
        while(numrequests == bufsize) {
            pthread_cond_wait(&empty, &m);
        }
	if (strcmp(sched, "FIFO") == 0) {
	  fill_buff(newNode);
	} else if (strcmp(sched, "SFNF") == 0) {
	  fill_buff2(newNode);
	} else if (strcmp(sched, "SFF") == 0) {
	  fill_buff3(newNode);
	} 
        
        pthread_cond_signal(&fill);
        pthread_mutex_unlock(&m);

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
    
    if(buffer == NULL){
        buffer = node;
	tail = node;
	buffer->next=NULL;
        buffer->previous = NULL;
    }
    else{
        buff_t* tmp = tail;
	tmp->next = node;
	tail = node;
	tail->previous = tmp;
	tail->next = NULL;	
    }
    numrequests++;
}

void fill_buff2(buff_t *node){ //smallest filename first
    
    if(buffer == NULL){
        buffer = node;
	node->next = NULL;
	node->previous=NULL;
	//fprintf(stderr, "added to front of list\n");
    }
    else{
        buff_t* tmp = buffer;
        while(tmp !=NULL){
            if(node->filenamesize < tmp->filenamesize){
                if(tmp->previous==NULL){//front of list
                    buffer=node;
                    buffer->next = tmp;
                    tmp->previous = buffer;
		    buffer->previous=NULL;
                    break;
                }
                else{
                    buff_t* prevTmp = tmp->previous;
                    node->previous= prevTmp;
                    prevTmp->next = node;
                    node->next = tmp;
		    tmp->previous = node;
		    break;
                }
            }
            else{
                if(tmp->next == NULL){//if at end of list
                    tmp->next = node;
		    node->next=NULL;
		    node->previous=tmp;
                    break;
                }
                tmp = tmp->next;
            }
        }
    }
    numrequests++;
}


void fill_buff3(buff_t *node){ //smallest file first
    
    //fprintf(stderr,"placing: %s into buffer\n",node->filename);
    if(buffer == NULL){
        buffer = node;
	node->next = NULL;
	node->previous=NULL;
    }
    else{
        buff_t* tmp = buffer;
        while(tmp !=NULL){
            if(node->filesize < tmp->filesize){
                if(tmp->previous==NULL){//front of list
                    buffer=node;
                    buffer->next = tmp;
                    tmp->previous = buffer;
		    buffer->previous=NULL;
                    break;
                }
                else{
                    buff_t* prevTmp = tmp->previous;
                    node->previous= prevTmp;
                    prevTmp->next = node;
                    node->next = tmp;
		    tmp->previous = node;
		    break;
                }
            }
            else{
                if(tmp->next == NULL){//if at end of list
                    tmp->next = node;
		    node->next=NULL;
		    node->previous=tmp;
                    break;
                }
                
            }
	  tmp = tmp->next;
        }
    }
    numrequests++;   
}


buff_t* get_buff() {
    
    buff_t* tmp = buffer;
    buffer = buffer->next;
    numrequests--;
    return tmp;
}


void *consumer(void *arg) {
    while(1) {
        pthread_mutex_lock(&m);
        while(numrequests == 0) {
            pthread_cond_wait(&fill, &m);
        }
        
        // service request
        buff_t* requestbuff = get_buff();
	pthread_cond_signal(&empty);
        pthread_mutex_unlock(&m);
        requestHandle(requestbuff);
        Close(requestbuff->fd);
        free(requestbuff);

        
        
    }
}
