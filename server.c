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
int useptr = 0;
int bufsize;
int* buffer;

void fill_buff(int connfd);
int get_buff();
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

    buffer = (int *) malloc(bufsize * sizeof(int));

    listenfd = Open_listenfd(port);
    
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
	pthread_mutex_lock(&m);
	//fprintf(stderr, "acquired lock\n");
	while(numrequests == bufsize) {
	  //fprintf(stderr, "producer waiting\n");
	  pthread_cond_wait(&empty, &m);
	}      
	//fprintf(stderr, "accepted\n");
	//fprintf(stderr, "connfd: %d\n", connfd);
	fill_buff(connfd);
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

void fill_buff(int connfd) {
  buffer[fillptr] = connfd;
  fillptr = (fillptr + 1) % bufsize;
  numrequests++;
}

int get_buff() {
  int tmp = buffer[useptr];
  useptr = (useptr + 1) % bufsize;
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
    int connfd = get_buff();
    fprintf(stderr, "connfd: %d\n", connfd);
    requestHandle(connfd);
    Close(connfd);
    //fprintf(stderr, "servicing request\n");
    pthread_cond_signal(&empty);
    pthread_mutex_unlock(&m);
    
  }
}





    


 
