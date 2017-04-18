#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
	NOTYPE = 256, EQ,

	/* TODO: Add more token types */
    NUMBER,

};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +"     , NOTYPE} , // spaces
	{"\\+"    , '+'}    , // plus
    {"-"      , '-'}    ,
    {"\\*"    , '*'}    ,
    {"/"      , '/'}    ,
    {"\\("    , '('}    ,
    {"\\)"    , ')'}    ,
	{"=="     , EQ}     , // equal
    {"[0-9]+" , NUMBER} ,
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
                    case NOTYPE:
                        break;
                    case EQ:
                        
                        tokens[nr_token++].type = EQ;
                        break;
                    case NUMBER:
                        if(substr_len >= 31){
                            assert(0);
                        }
                        strncpy(tokens[nr_token].str, substr_start, substr_len);
                        tokens[nr_token].str[substr_len] = '\0';
                        tokens[nr_token++].type = NUMBER;
                        break;
					default:
                        tokens[nr_token++].type = rules[i].token_type;
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true; 
}

static bool check_parentheses(int p, int q, bool *success){
    bool surrounded = true;

    if(tokens[p].type != '(' || tokens[q].type != ')'){
        surrounded = false;
    }

    int open = 0, i;
    for(i = p; i <= q; i++){    // start with p in case of ")...("
        if(tokens[i].type == '('){
            open++;
        }
        else if(tokens[i].type == ')'){
            open--;
            if(open < 0){
                *success = false;
                return false;
            }
            else if(open == 0 && i != q){    // close p
                surrounded = false;
            }
        }
    }

    if(open){
        *success = surrounded = false;
    }
    return surrounded;
}

static uint32_t get_dominant_operator(int p, int q, bool *success){
    int i, open = 0, last_type = NOTYPE;
    for(i = p; i <= q; i++){
        int cur_type = tokens[i].type;

        if(cur_type == '('){
            open++;
            continue;
        }
        else if(cur_type == ')'){
            open--;
            continue;
        }

        if(!open){
            if(cur_type == last_type){
                printf("Duplicate token %d!\n", cur_type);
                *success = false;
                return 0;
            }

            // TODO: add priority
        }



    }
}

static uint32_t eval(int p, int q, bool *success){
    if(p > q){
        printf("Bad expression!");
        return 0;
    }
    else if(p == q){
        if(tokens[p].type == NUMBER){
            return strtol(tokens[p].str, NULL, 0);
        }
        else{
            printf("Single token, need a NUMBER.\n");
            return 0;
        }
    }
    else if(check_parentheses(p, q, success) == true){
        return eval(p + 1, q - 1, success);
    }
    else{
        if(!success){
            printf("Parentheses unmatched!\n");
            return false;
        }

    }
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}

	/* TODO: Insert codes to evaluate the expression. */
	panic("please implement me");
	return 0;
}

