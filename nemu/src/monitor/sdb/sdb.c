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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "watchpoint.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();
void free_wp(int des);
WP* new_wp(char *expr);
void print_wp();
word_t vaddr_read(vaddr_t addr, int len);
word_t expr(char *e, bool *success);

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  } else {
    return "si";
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args) {
  // default step number
  uint64_t n = 1;

  if (args) {
    n = atoi(args);
  }

  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  if (!args) {
    printf("Usage: info r/w\n");
    return 0;
  }

  if (strcmp(args, "r") == 0) {
    isa_reg_display();
  } else if (strcmp(args, "w") == 0) {
    print_wp();
  }
  return 0;
}

static int cmd_x(char *args) {
  char *cnt_str = strtok(NULL, " ");
  if (cnt_str) {
    uint32_t cnt = atoi(cnt_str);
    char *addr_str = strtok(NULL, " ");
    if (addr_str) {
      if (strlen(addr_str) >= 2 && addr_str[0] == '0' && addr_str[1] == 'x') {
        int addr = (int)strtol(addr_str + 2, NULL, 16);
        printf("%-14s%-28s%-s\n", "Address", "Hexadecimal", "Decimal");
        for (int i = 0; i < cnt; ++i) {
          printf("0x%-12x0x%02x  0x%02x  0x%02x  0x%02x", (addr),
                 vaddr_read(addr, 1), vaddr_read(addr + 1, 1),
                 vaddr_read(addr + 2, 1), vaddr_read(addr + 3, 1));
          printf("\t  %04d  %04d  %04d  %04d\n", vaddr_read(addr, 1),
                 vaddr_read(addr + 1, 1), vaddr_read(addr + 2, 1),
                 vaddr_read(addr + 3, 1));
          addr += 4;
        }
      } else {
        printf("the result of the given expression is NOT hexadecimal!");
      }
    }
  }
  return 0;
}

static int cmd_p(char *args) {
  if (!args) {
    return 0;
  }

  bool success = false;
  word_t val = expr(args, &success);
  if (!success) {
    printf("Invalid expression!\n");
  } else {
    printf("> %u\n", val);
  }
  return 0;
}

static int cmd_w(char *args) {
  if (!args) {
    return 0;
  }

  if (new_wp(args) == NULL) {
    printf("watchpoint poll is full!\n");
  }
  return 0;
}

static int cmd_d(char *args) {
  if (!args) {
    return 0;
  }

  free_wp(atoi(args));
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  { "si", "Single-step through N instructions, when N is not givern, just 1", cmd_si },
  { "info", "Print register status or wachpoint information", cmd_info },
  { "x", "Evaluate the expression, use the result as the start memory address, output N consecutive 4-bytes in hexadecimal", cmd_x },
  { "p", "Evaluate the expression", cmd_p },
  { "w", "Suspend program when the value of the expression changed", cmd_w },
  { "d", "Delete the watchpoint N", cmd_d },

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
