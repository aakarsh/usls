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

int queue_destroy(struct queue_head* q);
struct queue_head* queue_new() ;

void queue_mark_finish_filling(struct queue_head* list);
void* queue_take(struct queue_head* queue, int n);

void queue_prepend_one(struct queue_head* list , void* node, int node_sz);
void queue_prepend_all(struct queue_head* list , void* node, int node_sz,int n);
void queue_prepend(struct queue_head* list , struct queue* node) ;



typedef struct queue* (*queue_transformer)(void* obj, int id,void* priv, struct queue_head* out_q);

struct queue_transformer_arg {
   int id;
   char name[20];
   struct queue_head* in_q;
   struct queue_head* out_q;
   queue_transformer transform;
   void* priv; 
};



pthread_t* start_tranformers(char* name,queue_transformer transform,void* priv, struct queue_head* in_q, struct queue_head* out_q, int n);
void join_transformers(pthread_t* id , int n);
