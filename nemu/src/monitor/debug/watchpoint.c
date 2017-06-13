#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP* new_wp() {
    if(free_ == NULL) assert(0);

    WP *new = free_;
    free_ = free_->next;

    new->next = head;
    head = new;
    return new;
}

bool free_wp(int no){
    WP *p = head, *prev = NULL;
    prev->next = head;
    while(p && p->NO != no){
        prev = p;
        p = p->next;
    }

    // unfound
    if(p == NULL) {
        return false;
    }
    // found
    prev->next = p->next;
    p->next = free_;
    free_ = p;
    return true;
}

bool catch_wp(){
    bool catched = false;
    WP *p = head;
    while(p){
        bool succ = true;
        int result = expr(p->expr, &succ);
        if(p->val != result){
            printf("Watchpoint %d: %s\n", p->NO, p->expr);
            printf("Old value = 0x%x\n", p->val);
            printf("New value = 0x%x\n", result);
            p->val = result;
            catched = true;
        }
        p = p->next;
    }
    return catched;
}
