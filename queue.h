#include <pthread.h>

struct queue {
  void* data; // your data
  int data_len;
  struct queue* next;
};


struct queue_head{
  struct queue* head;
  int size;
  int finish_filling;
  pthread_cond_t modified_cv;
  pthread_mutex_t lock;
};


struct queue* queue_create_node(void* data, int data_len);
struct queue_head* queue_init(struct queue_head** list) ;

void queue_mark_finish_filling(struct queue_head* list);
void* queue_take(struct queue_head* queue, int n);

void queue_prepend_one(struct queue_head* list , void* node, int node_sz);
void queue_prepend_all(struct queue_head* list , void* node, int node_sz,int n);
void queue_prepend(struct queue_head* list , struct queue* node) ;
