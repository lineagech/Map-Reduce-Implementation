/******************************************************************************
 * Your implementation of the MapReduce framework API.
 *
 * Other than this comment, you are free to modify this file as much as you
 * want, as long as you implement the API specified in the header file.
 *
 * Note: where the specification talks about the "caller", this is the program
 * which is not your code.  If the caller is required to do something, that
 * means your code may assume it has been done.
 ******************************************************************************/

#include "mapreduce.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define DEBUG 0
#if DEBUG
#define MR_TRC(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define MR_TRC(fmt, ...) 
#endif

/*
typedef struct {
    struct map_reduce *mr;
    int infd;
    int mapper_id;
} tr_map_arg;

typedef struct {
    struct map_reduce *mr;
    int outfd;
} tr_reduce_arg;
*/

/* Static variables */
static struct map_reduce *p_mr = NULL;
/*static int infd = -1;*/
static int outfd = -1;

void tr_cleanup (void *arg) {
    int thread_id = *(int *)(arg);
    MR_TRC("tr_cleanup with id %d\n", thread_id);
    p_mr->exec_done[thread_id] = 1;
    pthread_cond_signal(&((p_mr->buf_avail)[thread_id]));
}

void *tr_map (void *arg) {
    int mapper_id = *(int *)(arg);
    int infd = p_mr->infds[mapper_id];
    //pthread_cleanup_push(tr_cleanup, arg);
    MR_TRC("tr_map with id %d\n", mapper_id);
    p_mr->map(p_mr, infd, mapper_id, p_mr->num_threads);  
    p_mr->exec_done[mapper_id] = 1;
    pthread_cond_signal(&((p_mr->buf_avail)[mapper_id]));
    pthread_exit(0);
}

void *tr_reduce (void *arg) {
    MR_TRC("tr_reduce with id \n");
    p_mr->reduce(p_mr, outfd, p_mr->num_threads);
    pthread_exit(0);
}

struct map_reduce *mr_create(map_fn map, reduce_fn reduce, int threads, int buffer_size) {
    int i;

    p_mr = malloc(sizeof(struct map_reduce));
    if (p_mr == NULL) {
        goto mr_create_err; 
    }        
    
    /* Store arguments */
    p_mr->map = map;
    p_mr->reduce = reduce;
    p_mr->num_threads = threads;
    p_mr->buffer_size = buffer_size;
    
    /* Allocate threads and buffer memory */
    p_mr->threads_arr = malloc(sizeof(pthread_t) * threads);
    p_mr->threads_arg_arr = malloc(sizeof(int) * threads);
    p_mr->curr_buf_size = malloc(sizeof(int) * threads);
    memset(p_mr->curr_buf_size, 0, sizeof(int) * threads);
    if (p_mr->threads_arr == NULL || p_mr->threads_arg_arr == NULL) {
        goto mr_create_err;   
    }
    p_mr->buffer_arr = malloc(sizeof(char *) * threads);
    if (p_mr->buffer_arr == NULL) {
        goto mr_create_err;   
    }
    /* Allocate each thread buffer memory */
    for ( i = 0; i < threads; i++ ) {
        p_mr->buffer_arr[i] = malloc(buffer_size);
        if (p_mr->buffer_arr[i] == NULL) {
            goto mr_create_err;
        }
    }

    /* Initialize lock and conditional variables */
    p_mr->mutex_arr = malloc(sizeof(pthread_mutex_t) * threads);
    p_mr->buf_avail = malloc(sizeof(pthread_cond_t) * threads);
    for ( i = 0; i < threads; i++ ) {
        pthread_mutex_init(&((p_mr->mutex_arr)[i]), NULL);
        pthread_cond_init(&((p_mr->buf_avail)[i]), NULL);
    }

    p_mr->exec_done = malloc(sizeof(int) * threads); 
    memset(p_mr->exec_done, 0, sizeof(int) * threads);
    
    p_mr->infds = malloc(sizeof(int) * threads);
    memset(p_mr->infds, -1, sizeof(int) * threads);
    
    MR_TRC("mr_create buffer size %d\n", p_mr->buffer_size);
    MR_TRC("mr_create finished\n");   
    return p_mr;

mr_create_err:
    return NULL;
}

void mr_destroy(struct map_reduce *mr) {
    int i;

    /* Free allocated memory */
    for ( i = 0; i < mr->num_threads; i++) {
        free(mr->buffer_arr[i]);
    }
    free(mr->buffer_arr);
    free(mr->threads_arr);
    free(mr->threads_arg_arr);
    free(mr->curr_buf_size);
    free(mr->mutex_arr);
    free(mr->buf_avail);
    free(mr->exec_done);
    free(mr->infds);
    free(mr);
}

int mr_start(struct map_reduce *mr, const char *inpath, const char *outpath) {
    int i;
    
    if (mr == NULL || inpath == NULL || outpath == NULL) 
        return 1;

    /* Open input and output files */
    /*infd = open(inpath, O_RDONLY);*/
    outfd = open(outpath, O_RDWR | O_CREAT, 0666);
    
    /* Create map threads */
    for ( i = 0; i < mr->num_threads; i++ ) {
        mr->infds[i] = open(inpath, O_RDONLY);
	    mr->threads_arg_arr[i] = i; 
        pthread_create(&mr->threads_arr[i], NULL, tr_map, (void *)(&(mr->threads_arg_arr[i])));
    }

    /* Create reduce thread */
    pthread_create(&mr->reduce_thread, NULL, tr_reduce, NULL);
    
    return 0;
}

int mr_finish(struct map_reduce *mr) {
    int i;

    if (mr == NULL) 
        return 1;
    /* Wait for all map threads execution done */
    for ( i = 0; i < mr->num_threads; i++ ) {
        pthread_join(mr->threads_arr[i], NULL);
        close(mr->infds[i]);
    }
    
    /* Wait for reduce thread execution done */
    pthread_join(mr->reduce_thread, NULL);
    
    /*close(infd);*/
    close(outfd);
    return 0;
}

int mr_produce(struct map_reduce *mr, int id, const struct kvpair *kv) {
    char *p_buf_pos = NULL;
    uint32_t push_size;
    MR_TRC("mr_produce starting with id %d\n", id);
    pthread_mutex_lock( &((mr->mutex_arr)[id]) );
     
    //MR_TRC("mr_produce with id %d, key size is %u and key is %s, value size is %u and value is %d\n", id, kv->keysz, (char*)kv->key, kv->valuesz, *(int*)kv->value);
    push_size = sizeof(uint32_t) + kv->keysz + sizeof(uint32_t) + kv->valuesz;
    /* Check if there will be overflow */
    while (mr->curr_buf_size[id] + push_size > mr->buffer_size) {
        // give up the lock
        // wait for notification from reducer 
        // every time when reducer remove kv from the buffer, it will call pthread_cond_signal
        MR_TRC("mr_produce current buffer size %d, and push size %d\n", mr->curr_buf_size[id], push_size);
        if (push_size > mr->buffer_size ) {
            pthread_mutex_unlock( &((mr->mutex_arr)[id]) );
            return -1;
        }
        pthread_cond_wait(&((mr->buf_avail)[id]), &((mr->mutex_arr)[id])); 
    }
    p_buf_pos = mr->buffer_arr[id] + mr->curr_buf_size[id];
    /* push value to the buffer */
    memcpy(p_buf_pos, kv->value, kv->valuesz);
    p_buf_pos += kv->valuesz;
    /* push value size to the buffer */
    memcpy(p_buf_pos, &kv->valuesz, sizeof(uint32_t));
    p_buf_pos += sizeof(uint32_t);
    /* push key to the buffer */
    memcpy(p_buf_pos, kv->key, kv->keysz);
    p_buf_pos += kv->keysz;
    /* push key size to the buffer */
    memcpy(p_buf_pos, &kv->keysz, sizeof(uint32_t));
    p_buf_pos += sizeof(uint32_t);
          
    /* Update current buffer size */
    mr->curr_buf_size[id] += push_size;
    /* Signal reducer if it is blocked due to waiting for buf_avail */
    pthread_cond_signal(&((mr->buf_avail)[id]));
    MR_TRC("mr_produce with id %d, key size is %u and key is %s, value size is %u and value is %d\n", id, kv->keysz, (char*)kv->key, kv->valuesz, *(int*)kv->value);
    MR_TRC("mr_produce end with id %d, current buffer size %u\n", id, mr->curr_buf_size[id]); 
    pthread_mutex_unlock( &((mr->mutex_arr)[id]) );
    return 1;
}

int mr_consume(struct map_reduce *mr, int id, struct kvpair *kv) {
    char *p_buf_pos;
    int push_size; // the last kvpair content's size 
    MR_TRC("mr_consume starting with id %d\n", id);
    pthread_mutex_lock( &((mr->mutex_arr)[id]) );
    
    /* Check current buffer size is greater than zero, if it is zero, the thread should be blocked */
    while (mr->curr_buf_size[id] == 0) {
        if (mr->exec_done[id] == 1) {
            pthread_mutex_unlock( &((mr->mutex_arr)[id]) );    
            return 0;
        }
        pthread_cond_wait(&((mr->buf_avail)[id]), &((mr->mutex_arr)[id])); 
    }
    p_buf_pos = mr->buffer_arr[id] + mr->curr_buf_size[id];
    /* get key size */
    p_buf_pos -= sizeof(uint32_t);
    memcpy(&(kv->keysz), p_buf_pos, sizeof(uint32_t));
    /* get key */
    p_buf_pos -= kv->keysz;
    memcpy(kv->key, p_buf_pos, kv->keysz);
    /* get value size */
    p_buf_pos -= sizeof(uint32_t);
    memcpy(&(kv->valuesz), p_buf_pos, sizeof(uint32_t));
    /* get value */
    p_buf_pos -= kv->valuesz;
    memcpy(kv->value, p_buf_pos, sizeof(uint32_t));
   
    push_size = sizeof(uint32_t) + kv->valuesz + sizeof(uint32_t) + kv->keysz;
    mr->curr_buf_size[id] -= push_size;

    /* Signal mapper if it is blocked due to there is no enough buffer size */
    pthread_cond_signal(&((mr->buf_avail)[id]));
    MR_TRC("mr_consume with id %d, key size is %u and key is %s, value size is %u and value is %d\n", id, kv->keysz, (char*)kv->key, kv->valuesz, *(int*)kv->value);
    MR_TRC("mr_consume with id %d, cuurent buffer size %u\n", id, mr->curr_buf_size[id]);

    pthread_mutex_unlock( &((mr->mutex_arr)[id]) );
    return 1;
}
