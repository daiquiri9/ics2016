#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

#define EXPR_LEN 64

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */
    char expr[EXPR_LEN];
    int val;
} WP;

//void init_wp_pool();  // monitor.c
WP* new_wp();
bool free_wp();
bool catch_wp();
void print_wp();

#endif
