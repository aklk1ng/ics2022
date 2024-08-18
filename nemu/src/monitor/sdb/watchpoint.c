/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32

word_t expr(char *e, bool *success);

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char exp[128];
  uint32_t old;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL, *cur;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    strcpy(wp_pool[i].exp, "\0");
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP *search_(WP *wp) {
  WP *tmp = head;
  while (tmp) {
    if (tmp->next == wp) {
      break;
    }
    tmp = tmp->next;
  }
  return tmp;
}

WP* new_wp(char *exp) {
  // no enough wp
  if (!free_) {
    return NULL;
  }

  if (!head) {
    head = free_;
    free_ = free_->next;
    head->next = NULL;
    strcpy(head->exp, exp);
    cur = head;
  } else {
    cur->next = free_;
    free_ = free_->next;
    cur = cur->next;
    cur->next = NULL;
    strcpy(cur->exp, exp);
  }

  return cur;
}

void free_wp(int id) {
  if (!free_ ) {
    return;
  }

  WP *tmp = head;
  while (tmp) {
    if (tmp->NO == id) {
      break;
    }
    tmp = tmp->next;
  }

  if (!tmp) {
    printf("there is no such watchpoint!\n");
    return;
  }

  // delete the first watchpoint in head list
  if (tmp == head) {
    head = head->next;
    tmp->next = free_;
    free_ = tmp;
  } else if (tmp == cur) { // delete the last watchpoint in head list
    tmp->next = free_;
    free_ = tmp;
    cur = search_(tmp);
    cur->next = NULL;
  } else {
    search_(tmp)->next = tmp->next;
    tmp->next = free_;
    free_ = tmp;
  }
}

void check_wp() {
  bool success;
  WP *tmp = head;
  while (tmp) {
    word_t res = expr(tmp->exp, &success);
    if (!success) {
      printf("The expression of watchpoint %d is invalid!\n", tmp->NO);
    } else if (res != tmp->old) {
      printf("Num:%-6d\t Expr:%-20s\t  New Val:%-14u\t  Old Val:%-14u\n",
             tmp->NO, tmp->exp, res, tmp->old);
      tmp->old = res;
      nemu_state.state = NEMU_STOP;
    }
    tmp = tmp->next;
  }
}

void print_wp() {
  WP *tmp = head;
  if (!head) {
    printf("there is no watchpoint\n");
    return;
  }
  printf("%-15s%-7s%s\n", "NO", "Exp", "Val");
  while(tmp) {
    printf("%02d\t%10s\t%-10u\n", tmp->NO, tmp->exp, tmp->old);
    tmp = tmp->next;
  }
}
