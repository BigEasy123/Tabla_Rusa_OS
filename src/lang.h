#ifndef LANG_H
#define LANG_H

#include <stdint.h>

enum rusa_token_kind {
    RUSA_TOKEN_EOF = 0,
    RUSA_TOKEN_IDENTIFIER,
    RUSA_TOKEN_KEYWORD,
    RUSA_TOKEN_NUMBER,
    RUSA_TOKEN_STRING,
    RUSA_TOKEN_SYMBOL
};

struct rusa_token {
    enum rusa_token_kind kind;
    char text[48];
    unsigned int line;
    unsigned int col;
};

struct rusa_lexer {
    const char* source;
    const char* cursor;
    unsigned int line;
    unsigned int col;
};

struct rusa_ast {
    const char* source;
    char origin[64];
    unsigned int statement_count;
};

struct rusa_bytecode {
    const char* source;
    char origin[64];
    unsigned int op_count;
};

struct rusa_vm {
    unsigned int executed_ops;
    int last_status;
};

void lang_init(void);
void lang_set_call_handler(void (*handler)(char* command));
int lang_run_source(const char* source, const char* origin, const char* args);
int lang_run_file(const char* path, const char* args);
int lang_last_diag(char* origin, unsigned int origin_max,
                   unsigned int* line, unsigned int* col,
                   char* title, unsigned int title_max,
                   char* detail, unsigned int detail_max);
void lang_cmd(char* arg);
void rusa_lexer_init(struct rusa_lexer* lexer, const char* source);
struct rusa_token rusa_lexer_next_token(struct rusa_lexer* lexer);
int rusa_parse_source(const char* source, const char* origin, struct rusa_ast* out);
void rusa_ast_free(struct rusa_ast* ast);
int rusa_typecheck(struct rusa_ast* ast);
int rusa_compile(struct rusa_ast* ast, struct rusa_bytecode* out);
void rusa_vm_init(struct rusa_vm* vm);
int rusa_vm_execute(struct rusa_vm* vm, struct rusa_bytecode* bytecode, const char* args);
int rusa_eval_source(const char* source, const char* origin, const char* args);
int rusa_repl_step(const char* line);
int rusa_register_native(const char* name, void (*fn)(void));
int rusa_import_module(const char* name);
uint32_t rusa_native_count(void);

#endif
