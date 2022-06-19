#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>

struct node {
    int val;
    struct node *next;
};

typedef struct node Node;

Node *list;

bool is_marked(Node *n) {
    return (unsigned long) n->next & (unsigned long) 0x1;
}

void mark(Node *n) {
    n->next = (Node *) ((unsigned long) n->next | (unsigned long) 0x1);
}

Node *get_next(Node *n) {
    return (Node *) ((unsigned long) n->next & ~(unsigned long) 0x1);
}

Node *initialize_list() {
    Node *head = malloc(sizeof(Node));
    return head;
}

void free_list(Node *head) {
    Node *tmp = head;
    while (get_next(tmp) != NULL) {
        Node *node = get_next(tmp);
        tmp->next = get_next(node);
        free(node);
    }
    free(head);
}

bool insert(Node *head, const int val) {
    Node *left = head;
    while (true) {
        Node *right = head->next;

        Node *new_node = malloc(sizeof(Node));
        if (new_node == NULL)
            return false;
        new_node->val = val;
        new_node->next = right;
        if (atomic_compare_exchange_strong(&left->next, &right, new_node))
            return true;
        free(new_node);
    }
}

bool delete(Node *head, const int val) {
    Node *pre = head;
    Node *curr = head->next;
    if (curr == NULL)
        return false;
    while (curr != NULL) {
        if (is_marked(curr)) {
            Node *next = get_next(curr);
            if (!atomic_compare_exchange_strong(&pre->next, &curr, next)) {
                if (is_marked(pre))
                    return false;
            }
            curr = get_next(pre);
        }
        if (val == curr->val)
            break;
        pre = curr;
        curr = get_next(pre);
    }

    if (val == curr->val) {
        mark(curr);
        Node *next = get_next(curr);
        if (!atomic_compare_exchange_strong(&pre->next, &curr, next)) {
            //std::cout << "marked node has been deleted by other thread" << std::endl;
        }
        return true;
    } else { // val < cur->val
        return false;
    }
}

bool contains(Node* head, int val) {
    Node* curr = head->next;

    while (curr != NULL) {
        if (is_marked(curr)) {
            curr = get_next(curr);
            continue;
        }
        if (curr->val == val) {
            break;
        }
        curr = get_next(curr);
    }

    if (curr == NULL) {
        return false;
    }
    return true;
}

void print_list(Node *head) {
    Node *tmp = head->next;
    while (tmp != NULL) {
        printf("%d ", tmp->val);
        tmp = tmp->next;
    }
}



void insert_node() {
    for (int i = 0; i < 10; ++i) {
        insert(list, i);
    }
}

void delete_node() {
    delete(list, 5);
}


void delete_node1() {
    delete(list, 2);
}

void delete_node2() {
    delete(list, 4);
}

int main() {
    list = initialize_list();
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;
    pthread_t thread4;
    pthread_create(&thread1, NULL, insert_node, NULL);
    pthread_create(&thread2, NULL, delete_node, NULL);
    pthread_create(&thread3, NULL, delete_node1, NULL);
    pthread_create(&thread4, NULL, delete_node2, NULL);
//
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
    print_list(list);

}