#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "BXP/bxp.h"
#include "ADTs/heapprioqueue.h"
#include "ADTs/queue.h"
#include "ADTs/hashmap.h"
#define UNUSED __attribute__((unused))

#define PORT 19999
#define SERVICE "DTS"
#define USAGE "./dtsv3"

#define USECS (10 * 1000)

const PrioQueue *q = NULL;
const Map *map = NULL;

int globid = 0;

void *receive();
void *timer(void *args);

typedef struct event {
    int oneshot;
    int repeat;
    int cancelled;
    unsigned long numRepeats;
    unsigned long interval;
    unsigned long id;
    unsigned long clid;
    unsigned int port;
    char *host; 
    char *service;
} Event;

long int hash(void *key, long int N){ 
    return (((unsigned long)key)%N);
}

/* Compares the two timeval structs
   Returns -1 for p2>p1 | 1 for p1>p2 | 0 for p1==p2 */
int compare(void *p1, void *p2){
    struct timeval *s1 = (struct timeval *)p1;
    struct timeval *s2 = (struct timeval *)p2;
    int c;
    if((s1->tv_sec) < (s2->tv_sec))
        c = -1;
    else if((s1->tv_sec) > (s2->tv_sec))
        c = 1;
    else{
        if((s1->tv_usec) < (s2->tv_usec))
            c = -1;
        else if((s1->tv_usec) > (s2->tv_usec))
            c = 1;
        else
            c = 0;
    }
    return c;

}

int extractWords(char *buf, char *sep, char *words[]) {
    int i;
    char *p;

    for (p = strtok(buf, sep), i = 0; p != NULL; p = strtok(NULL, sep),i++)
        words[i] = p;
    words[i] = NULL;
    return i;
}

int main(UNUSED int argc, UNUSED char *argv[]) {
    q = HeapPrioQueue(compare, doNothing, doNothing);
    map = HashMap(100, 0.f, hash, compare, NULL, NULL);
    
    pthread_t receiver_t, timer_t;
    
    pthread_create(&receiver_t, NULL, receive, NULL);
    pthread_create(&timer_t, NULL, timer, NULL);
    
    pthread_join(receiver_t, NULL);
    pthread_join(timer_t, NULL);

    return 0;
}

int oneshot_insert(char *words[]){
    Event *eve = (Event *)malloc(sizeof(Event));
    struct timeval *t = (struct timeval *)malloc(sizeof(struct timeval)); 
    int status = 1;
    //Initialize timeval
    scanf(words[2], "%lu", &(t->tv_sec));
    scanf(words[3], "%lu", &(t->tv_usec));
    
    //Initialize event
    eve->oneshot = 1;
    eve->repeat = 0;
    eve->cancelled = 0;
    eve->numRepeats = 0;
    eve->id = globid;
    scanf(words[1], "%lu", &(eve->clid));
    scanf(words[6], "%u", &(eve->port));
    scanf(words[4], "%lu", &(eve->host));
    eve->host = strdup(words[4]); 
    eve->service = strdup(words[5]);
    
    //Inset priority queue where priority is the time since the Epoch
    q->insert(q, t, (void *)eve);
    
    //Insert map where key is id
    map->putUnique(map, (void *)&globid, (void *)eve);
    
    //Increment the id for the next events
    globid++;
    return status;  
}

int repeat_insert(char *words[]){
    Event *eve = (Event *)malloc(sizeof(Event));
    struct timeval *t = (struct timeval *)malloc(sizeof(struct timeval));
    int status = 1;

    /* Convert msec to sec and usec */
    long secs = 0;
    long msecs = 0;
    long usecs = 0;
    scanf(words[2], "%ld", &msecs);
    usecs = msecs * 1000;
    while(usecs > 1000000){
        usecs = usecs - 1000000;
        secs++;
    }
    
    //Initialize the timeval struct
    t->tv_sec = secs;
    t->tv_usec = usecs;
    
    //Initialize event struct
    eve->oneshot = 0;
    eve->repeat = 1;
    eve->cancelled = 0;
    scanf(words[3], "%lu", &(eve->numRepeats));
    eve->id = globid;
    scanf(words[1], "%lu", &(eve->clid));
    scanf(words[6], "%u", &(eve->port));
    scanf(words[4], "%lu", &(eve->host));
    eve->host = strdup(words[4]); 
    eve->service = strdup(words[5]);
    
    //Inset priority queue where priority is the time since the Epoch
    q->insert(q, t, (void *)eve);
    
    //Insert map where key is id
    map->putUnique(map, (void *)&globid, (void *)eve);
    
    //Increment the id for the next events
    globid++; 
    return status; 
}

int update_cancel(){
    //TODO: Cancel a previous request
    int status = 1;
    Event *eve_ptr;
    map->get(map, (void *)&globid, (void *)&eve_ptr);
    eve_ptr->cancelled = 0;
    return status;
}

void *receive(){
    
    BXPEndpoint sender;
    char *query = (char *)malloc(BUFSIZ);
    char *resp = (char *)malloc(BUFSIZ);
    unsigned len;
    BXPService bxps;
    char *service;
    unsigned short port;
    char *w[25];
    int N;

    service = SERVICE;
    port = PORT;
    
    //Initialize BXP system - bind to ‘port’ if non-zero
    assert(bxp_init(port, 1));
    
    //Offer service named `service' in this process
    bxps = bxp_offer(service);
    
    if (bxps == NULL) {
        fprintf(stderr, "Failure offering Echo service\n");
        free(query);
        free(resp);
        exit(EXIT_FAILURE);
    }
    //obtain the next query message from `bxps' - blocks until message available
    while ((len = bxp_query(bxps, &sender, query, BUFSIZ)) > 0) {
        query[len] = '\0';
        N = extractWords(query, "|", w);
        if(strcmp(w[0], "OneShot") == 0 && N == 7){
                oneshot_insert(w);
                sprintf(resp, "1%s", query);
        }
        else if(strcmp(w[0], "Repeat") == 0 && N == 7){
                repeat_insert(w);
                sprintf(resp, "1%s", query);
        }
        else if(strcmp(w[0], "Cancel") == 0 && N == 2){
                update_cancel();
                sprintf(resp, "1%s", query);
        }
        else{
            sprintf(resp, "0%s", query);
        }
        bxp_response(bxps, &sender, resp, strlen(resp) + 1);
    }
    pthread_exit(NULL);
}

void *timer(UNUSED void *args) {
    struct timeval now;
    struct timeval *tval_ptr;
    
    
    for(;;) {
        //Queue to store ready to harvest events
        const Queue *queue = Queue_create(NULL);
        //Queue to store the repeat events
        const Queue *r_queue = Queue_create(NULL);
        
        Event *eve_ptr;
        usleep(USECS);
        gettimeofday(&now, NULL);

        /* Get the events that are ready to harvest,
           Then add to queue */
        while(q->min(q, (void **)&tval_ptr, (void **)&eve_ptr)){
            //If now > tval_ptr, returns 1
            if(compare((void *)&now, (void *)&tval_ptr) == 1){
                //Remove the minimum event from the priority queue
                q->removeMin(q, (void **)&tval_ptr, (void **)&eve_ptr);
                //Add to queue
                queue->enqueue(queue, eve_ptr);
            }
            else
                break;
        }
        /* Process the events that are in the queue */
        while(queue->dequeue(queue, (void **)&eve_ptr)){
            //If it is a repeat query, fire it
            if((eve_ptr->repeat == 1) && (eve_ptr->numRepeats > 0)){
                printf("Event fired: %lu|%s|%s|%u\n", eve_ptr->clid, eve_ptr->host, eve_ptr->service, eve_ptr->port);
                eve_ptr->numRepeats--;
                //If it still have repeats left add to r_queue
                if(eve_ptr->numRepeats > 0)
                    r_queue->enqueue(r_queue, eve_ptr);
                //If not recycle the heap storage
                else{
                    //remove from the map
                    //free heap
                }
            }
            //If the event is not cancelled, fire it
            else if(eve_ptr->cancelled == 0){
                printf("Event fired: %lu|%s|%s|%u\n", eve_ptr->clid, eve_ptr->host, eve_ptr->service, eve_ptr->port);
            }
            //If it is cancelled, recycle the heap storage
            else{
                //remove from the map
                //free heap
            }
        }

        /* Go through r_queue and insert the repeat events
           to the priority queue with a new priority */
        while(r_queue->dequeue(r_queue, (void **)&eve_ptr)){
            struct timeval *new_now;
            new_now = (struct timeval *)malloc(sizeof(struct timeval));
            q->insert(q, new_now, (void *)eve_ptr);
        }
    }
}