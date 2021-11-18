#ifndef __LRUSTACK_H
#define __LRUSTACK_H

typedef struct stack_node {
    int index; // index of way
    struct stack_node* next_more_recent;
    struct stack_node* next_less_recent;
} stack_node;

class LruStack {
    private:
        int size;   // Corresponds to the associativity
        stack_node* most_recent;
        stack_node* least_recent;
        stack_node** index_map; // array to map a way's index to its node (so we don't have to search through list)
    public:
        LruStack(int size);
        int get_lru();
        void set_mru(int n);
        ~LruStack();
};



#endif