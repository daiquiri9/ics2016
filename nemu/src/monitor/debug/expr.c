#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>

enum {
	NOTYPE = 256, EQ, NEQ, LG_AND, LG_OR,   // binary
    DEREF, NEG,                             // unary
    REG = 512, DEC_NUM, HEX_NUM,

};

static struct rule {
	char *regex;
	int token_type;
    int priority;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +"             , NOTYPE  , 0}   , // spaces
    {"0x[0-9a-fA-F]+" , HEX_NUM , 1}   , // HEX before DEC, in case of 0
    {"[0-9]+"         , DEC_NUM , 1}   ,
    {"\\$[a-zA-Z]+"   , REG     , 1}   ,
    {"\\|\\|"         , LG_OR   , 20}  ,
    {"&&"             , LG_AND  , 21}  ,
	{"=="             , EQ      , 30}  ,
    {"!="             , NEQ     , 30}  , // != before !
    {"!"              , '!'     , 60}  ,
    // DEREF, 60
    // NEG,   60
	{"\\+"            , '+'     , 51}  ,
    {"-"              , '-'     , 51}  ,
    {"\\*"            , '*'     , 52}  ,
    {"/"              , '/'     , 52}  ,
    {"\\("            , '('     , 100} ,
    {"\\)"            , ')'     , 100} ,
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
    int priority;
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

				/*Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);*/
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
                    case NOTYPE:
                        break;
                    case DEC_NUM:
                    case HEX_NUM:
                    case REG:
                        if(substr_len >= 31){
                            printf("Operand is too long!\n");
                            return false;
                        }
					default:
                        strncpy(tokens[nr_token].str, substr_start, substr_len);
                        tokens[nr_token].str[substr_len] = '\0';
                        tokens[nr_token].priority = rules[i].priority;
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

static bool check_parentheses(int p, int q){
    if(tokens[p].type == '(' && tokens[q].type == ')'){
        int open = 0;
        for(int i = p + 1; i < q; i++){
            if(tokens[i].type == '('){
                open++;
            }
            else if(tokens[i].type == ')'){
                open--;
                if(open < 0){
                    return false;
                }
            }
        }

        if(open == 0){
            return true;
        }
    }
    return false;
}

static uint32_t get_dominant_operator(int p, int q){
    int open = 0;
    int op = -1, dominant = 100;

    for(int i = p; i <= q; i++){
        int cur_type = tokens[i].type;
        int cur_priority = tokens[i].priority;

        if(cur_type == '('){
            open++;
            continue;
        }
        else if(cur_type == ')'){
            open--;
            if(open < 0) return i;
            continue;
        }
        if(open > 0) continue;

        switch(cur_type){
            case '+':
            case '-':
            case '*':
            case '/':
            case LG_OR:
            case LG_AND:
            case EQ:
            case NEQ:
            case '!':
            case DEREF:
            case NEG:
                if(dominant >= cur_priority){ // dominant op has lowest priority
                    op = i;
                    dominant = cur_priority;
                }
                break;
            default:
                break;
        }
    }
    return op;
}

static void syntax_error(int i, char *msg, bool *succ){
    *succ = false;
    /*printf("Tokens[%d]:%s, %s\n", i, tokens[i].str, msg);*/
    printf("Syntax error near `%s`.\n", tokens[i].str);
}

static uint32_t eval(int p, int q, bool *success){
    if(!*success){
        return 0;
    }

    if(p > q){
        syntax_error(q, "need operand.", success);
        return 0;
    }
    else if(p == q){
        if(tokens[p].type == REG){
            char *reg = tokens[p].str;
            reg++;  // skip $
            
            if(strcasecmp(reg, "eip") == 0) return cpu.eip;
            for(int i = 0; i < 8; i++){
                if(strcasecmp(reg, regsl[i]) == 0) return reg_l(i);
                if(strcasecmp(reg, regsw[i]) == 0) return reg_w(i);
                if(strcasecmp(reg, regsb[i]) == 0) return reg_b(i);
            }

            syntax_error(p, "invalid register.", success);
            return 0;
        }
        else if(tokens[p].type >= DEC_NUM){
            return strtol(tokens[p].str, NULL, 0);
        }
        else{
            syntax_error(p, "invalid operands.", success);
            return 0;
        }
    }
    else if(check_parentheses(p, q) == true){
        return eval(p + 1, q - 1, success);
    }
    else{
        int op = get_dominant_operator(p, q);
        if(op < 0){
            syntax_error(p, "no dominant operator.", success);
            return 0;
        }

        int op_type = tokens[op].type;
        uint32_t val1 = 0, val2 = 0;
        if(op_type == '!' || op_type == DEREF || op_type == NEG){     //unary
            val1 = eval(op + 1, q, success);
        }
        else{                                                         //binary
            val1 = eval(p, op - 1, success);
            val2 = eval(op + 1, q, success);
        }

        switch(op_type){
            case '+':
                return val1 + val2;
            case '-':
                return val1 - val2;
            case '*':
                return val1 * val2;
            case '/':
                if(val2 == 0){
                    if(*success) printf("Division by 0\n");
                    *success = false;
                    return 0;
                }
                return val1 / val2;
            case LG_OR:
                return val1 || val2;
            case LG_AND:
                return val1 && val2;
            case EQ:
                return val1 == val2;
            case NEQ:
                return val1 != val2;
            case '!':
                return !val1;
            case DEREF:
                return swaddr_read(val1, 4);
            case NEG:
                return -val1;
            default:
                break;
        }
    }

    return 0;
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}

    for(int i = 0; i < nr_token; i++){
        // * -> DEREF
        if(tokens[i].type == '*' && (i == 0 || 
                    (tokens[i - 1].type != ')' && tokens[i - 1].type < 512))){
            tokens[i].type = DEREF;
            tokens[i].priority = 60;
        } 
        // - -> NEG
        if(tokens[i].type == '-' && (i == 0 || 
                    (tokens[i - 1].type != ')' && tokens[i - 1].type < 512))){
            tokens[i].type = NEG;
            tokens[i].priority = 60;
        }
    }

    uint32_t val = eval(0, nr_token - 1, success);
	return val;
}

