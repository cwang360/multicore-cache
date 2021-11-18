#include <stdlib.h>
#include <stdio.h>
#include "lrustack.h"

LruStack::LruStack(int size) {
	size = size;
    
    index_map = (stack_node**) malloc(size * sizeof(stack_node*));
    if (index_map == NULL) {
        printf("Could not allocate memory for LRU stack index map\n");
        exit(1);
    }
    for (int i = 0; i < size; i++) {
        index_map[i] = NULL;
    }
    least_recent = NULL;
    most_recent = NULL;
}

int LruStack::get_lru() {
    return least_recent->index;
}

void LruStack::set_mru(int n) {
    // If already most recent, no need to do anything else.
    if (most_recent != NULL && most_recent->index == n) {
        return;
    }
    // get node corresponding to index
    stack_node* curr = index_map[n];

    // If not NULL, curr must be node with matching address. Update pointers
    // to move curr to most recent spot.
    if (curr != NULL) {
        curr->next_more_recent->next_less_recent = curr->next_less_recent;
        if (curr->next_less_recent != NULL) {
            curr->next_less_recent->next_more_recent = curr->next_more_recent;
        } else {
            // curr is at end (least recent) spot. Update least recent pointer
            // to node before curr.
            least_recent = curr->next_more_recent;
        }
        curr->next_more_recent = NULL;
        curr->next_less_recent = most_recent;
        most_recent->next_more_recent = curr;
        most_recent = curr;
    } else {
        // Node with index n does not exist, so make one and add it to the
        // most recent spot in the linked list.
        stack_node* temp = (stack_node*) malloc(sizeof(stack_node));
        if (temp == NULL) {
            printf("Could not allocate memory for LRU stack entry\n");
            exit(1);
        }
        temp->next_more_recent = NULL;
        temp->next_less_recent = most_recent;
        temp->index = n;
        if (most_recent != NULL) {
            most_recent->next_more_recent = temp;
        } else {
            least_recent = temp;
        }
        most_recent = temp;
        index_map[n] = temp;
    }
}

LruStack::~LruStack() {
    // Free all nodes in linked list.
    stack_node* curr = most_recent;
    while (curr != NULL) {
        stack_node* temp = curr->next_less_recent;
        free(curr);
        curr = temp;
    }
    free(index_map);
}