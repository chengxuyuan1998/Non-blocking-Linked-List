#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>


typedef struct Node {
    int value;
    struct Node *next;
} Node;

typedef struct List {
    Node *head;
    Node *tail;
} List;

Node *marked_ref(Node *origin) {
    return (Node*) ((unsigned long)origin | (unsigned long)0x1);
}

Node *unmarked_ref(Node *marked) {
    return (Node*) ((unsigned long)marked & ~(unsigned long)0x1);
}

int is_marked_ref(Node *ref) {
    return ((unsigned long)ref & (unsigned long)0x1);
}

Node *newNode(int value) {
    Node *node = (Node *)malloc(sizeof(Node));
    node->value = value;
    node->next = NULL;
    return node;
}

List *newList() {
    List *list = (List *)malloc(sizeof(List));
    list->head = newNode(NULL);
    list->tail = newNode(NULL);
    list->head->next = list->tail;
    return list;
}

int compare(int a, int b) {
    // ... Self defined ...
    // 0: equal, -1: a < b, 1: a > b
    if (a < b)
        return -1;
    else if (a == b)
        return 0;
    else
        return 1;
}

void search(List *list, int value, Node **prev, Node **cur) {
    Node *prev_next;
    search_again:
    do {
        Node *t = list->head;
        Node *t_next = list->head->next;
        // 1. 找prev和cur
        do {
            if(!is_marked_ref(t_next)) {
                (*prev) = t; // 这里保证prev肯定没被标记
                prev_next = t_next;
            }
            t = unmarked_ref(t_next);
            if(t == list->tail) {
                break;
            }
            t_next = t->next;
        } while(is_marked_ref(t_next) || compare(t->value, value) < 0);
        *cur = t;
        // 2. 检查相邻
        if(prev_next == *cur) {
            if(*cur != list->tail && is_marked_ref((*cur)->next)) {
                goto search_again; // 相邻但后面的节点被标记，则重新搜索
            } else {
                return; // 就是正确结果
            }
        }
        // 3. 删除被标记的节点
        if(atomic_compare_exchange_strong(&(*prev)->next, &prev_next, *cur)) {
            // cur和prev不是相邻,说明prev_next被标记,则需要删除
            free(prev_next);
            if(*cur != list->tail && is_marked_ref((*cur)->next)) {
                // 若cur被标记 => 还是需要重新搜索一次,并将其移除
                // 一轮只移除一个marked节点,直到没有为止
                // 但不会移除后面的marked节点
                goto search_again;
            } else {
                return;
            }
        }
    } while(1);
}

int insert(List *list, int value) {
    Node *node = newNode(value);
    Node *prev, *after;
    do {
        search(list, value, &prev, &after); // 查找节点
        if(after != list->tail && compare(after->value, value) == 0) {
            free(node);
            return 0; // 重复, 直接返回
        }
        node->next = after;
        if(atomic_compare_exchange_strong(&(prev->next), &after, node)) {
            // CAS设置前置节点的next指针, CAS本身会检测mark, 因为mark是在next指针内
            return 1;
        }
    } while(1);
}


int delete(List *list, int value) {
    Node *prev, *cur, *after;
    do {
        search(list, value, &prev, &cur);
        if(cur == list->tail || compare(value, cur->value)) {
            // 没找到, 退出
            return 0;
        }
        after = cur->next;
        // 1. 执行逻辑删除, CAS设置mark
        if(!is_marked_ref(cur->next)) {
            if(atomic_compare_exchange_strong(&(cur->next), &after, marked_ref(after))) {
                break;
            }
        }
    } while(1);
    // 2. 执行物理删除: CAS连接前后节点
    if(!atomic_compare_exchange_strong(&(prev->next), &cur, after)) {
        // CAS失败, 执行一次search, search会保证:
        // a. cur, after相邻且不会被标记
        // b. 中间的marked节点会被删除
        // 由于CAS失败,说明prev->next != cur
        // 通过search遍历,有顺序保证,可以保证已标记的cur会被删除
        // 后面会说明
        search(list, value, &prev, &cur);
    } else {
        free(cur);
    }
    return 1;
}

Node *find(List *list, int value) {
    Node *prev, *cur;
    search(list, value, &prev, &cur);
    if(cur == list->tail || compare(cur->value, value)) {
        return NULL;
    }
    return cur;
}

void print_list(Node *head) {
    Node *tmp = head->next;
    while (tmp != NULL) {
        printf("%d ", tmp->value);
        tmp = tmp->next;
    }
}

List* list;

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
    list = newList();
//    insert_node();
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
    print_list(list->head);
}