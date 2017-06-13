#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);
static void info_reg();

static int cmd_x(char *args);

static int cmd_p(char *args);

static int cmd_w(char *args);

static int cmd_d(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },

	/* TODO: Add more commands */
    { "si", "Take N(default: 1) more steps of the execution of the program", cmd_si},
    { "info", "Display informations about all registers or watchpoints", cmd_info},
    { "x", "Display memory starting from the given address by N * 4B", cmd_x},
    { "p", "Display the result of expression", cmd_p},
    { "w", "Set watchpoint for expression", cmd_w},
    { "d", "Delete watchpoint", cmd_d},

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

static int cmd_si(char *args) {
    if(!args){
        cpu_exec(1);
        return 0;
    }

    uint32_t n;
    sscanf(args, "%u", &n);
    if(n){
        cpu_exec(n);
    }
    else{
        printf("Invalid argument!\n");
        printf("Usage: si N\n");
    }
    return 0;

}

static int cmd_info(char *args) {
    if(args == NULL){
        printf("Must be followed with an argument!\n");
        printf("Usage: info r\n");
        printf("       info w\n");
        return 0;
    }

    char arg;
    sscanf(args, "%c", &arg);

    if(arg == 'r'){
        info_reg();
    }
    else if(arg == 'w'){
        print_wp();
    }
    else{
        printf("Invalid argument!\n"); 
        printf("Usage: info r\n");
        printf("       info w\n");
    }
    return 0;
}

static void info_reg() {
    int i = 0;

    for(i = R_EAX; i <= R_EDI; i++){
        printf("%s: 0x%x\n", regsl[i], reg_l(i));
    }
    printf("eip: 0x%x\n", cpu.eip);
}

static int cmd_x(char *args) {
    if(args == NULL){
        printf("Must be followed with arguments!\n");
        printf("Usage: x N EXPR\n");
        return 0;
    }

    uint32_t n, addr, i, j;
    /*sscanf(args, "%u %i", &n, &addr);*/
    char *num = strtok(NULL, " ");
    if(sscanf(num, "%u", &n) < 1){
        printf("Invalid number!\n");
        return 0;
    }
    char *e = num + strlen(num) + 1;
    
    bool succ = true;
    addr = expr(e, &succ);
    if(!succ){
        printf("Must be followed with expression!\n");
        printf("Usage: x N EXPR\n");
        return 0;
    }

    for(i = 0; i < n; i += 4){
        printf("0x%08x:\t", addr + i * 4);
        for(j = 0; (i + j) < n && j < 4; j++){
            uint32_t info = swaddr_read(addr + (i + j) * 4, 4);
            printf("0x%08x\t", info);
        }
        printf("\n");
    }

    return 0;
}

static int cmd_p(char *args) {
    if(args == NULL){
        printf("Must be followed with an expression!\n");
        printf("Usage: p $eax + 2 * 3\n");
        return 0;
    }

    bool succ = true;
    int result = expr(args, &succ);
    if(succ) printf("0x%x\n", result);
    return 0;
}

static int cmd_w(char *args) {
    if(args == NULL){
        printf("Must be followed with an expression!\n");
        printf("Usage: w $eip == 0x100000\n");
        return 0;
    }

    if(strlen(args) > EXPR_LEN){
        printf("Expression is too long!\n");
        return 0;
    }

    bool succ = true;
    int result = expr(args, &succ);
    if(succ) {
        WP *wp = new_wp();
        wp->val = result;
        strcpy(wp->expr, args);
        printf("Watchpoint %d: %s\n", wp->NO, args);
    }
    return 0;
}

static int cmd_d(char *args) {
    if(args == NULL){
        printf("Must be followed with the Number of watchpoint!\n");
        printf("Usage: d 1\n");
        return 0;
    }

    int no;
    if(sscanf(args, "%u", &no) < 1){
        printf("Invalid number!\n");
        return 0;
    }
    if(free_wp(no) == false){
        printf("No watchpoint at %d.\n", no);
    }
    return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
