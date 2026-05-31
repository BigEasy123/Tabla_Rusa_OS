#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "editor.h"
#include "events.h"
#include "fs.h"
#include "lang.h"
#include "object.h"

#define RUSA_VAR_MAX 32
#define RUSA_FN_MAX 16
#define RUSA_PARAM_MAX 4
#define RUSA_TEXT_MAX 96
#define RUSA_BODY_MAX 384
#define RUSA_IMPORT_DEPTH 4
#define RUSA_LOOP_LIMIT 256

enum rusa_type {
    RUSA_NONE,
    RUSA_INT,
    RUSA_BOOL,
    RUSA_STRING
};

struct rusa_value {
    enum rusa_type type;
    int32_t number;
    char text[RUSA_TEXT_MAX];
};

struct rusa_var {
    char name[24];
    enum rusa_type type;
    struct rusa_value value;
};

struct rusa_fn {
    char name[24];
    char params[RUSA_PARAM_MAX][24];
    uint32_t param_count;
    char body[RUSA_BODY_MAX];
};

struct rusa_runtime {
    struct rusa_var vars[RUSA_VAR_MAX];
    struct rusa_fn fns[RUSA_FN_MAX];
    uint32_t event_id;
    int returning;
    struct rusa_value return_value;
};

static struct rusa_runtime runtime;
static void (*call_handler)(char* command) = 0;

struct rusa_diag {
    int active;
    char origin[64];
    uint32_t line;
    uint32_t col;
    char title[64];
    char detail[160];
    char hint[160];
    char source_line[128];
};

static struct rusa_diag last_diag;
static const char* current_source = 0;
static const char* current_origin = "<eval>";
static const char* current_stmt_pos = 0;

static char lower_char(char c){
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_name_start(char c){
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int is_name_char(char c){
    return is_name_start(c) || (c >= '0' && c <= '9') || c == '.' || c == '-';
}

static int str_eq(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != lower_char(*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int str_starts_kw(const char* s, const char* kw){
    size_t i = 0;
    while(is_space(*s)) s++;
    while(kw[i]){
        if(lower_char(s[i]) != lower_char(kw[i])) return 0;
        i++;
    }
    return !is_name_char(s[i]);
}

static const char* skip_ws(const char* p){
    for(;;){
        while(is_space(*p)) p++;
        if(*p == '#'){
            while(*p && *p != '\n') p++;
        } else {
            return p;
        }
    }
}

static size_t text_len(const char* s){
    size_t n = 0;
    while(s && s[n]) n++;
    return n;
}

static void copy_text(char* dst, size_t max, const char* src){
    size_t i = 0;
    if(max == 0) return;
    while(src && src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void copy_span(char* dst, size_t max, const char* start, const char* end){
    size_t i = 0;
    if(max == 0) return;
    while(start < end && *start && i + 1 < max){
        dst[i++] = *start++;
    }
    dst[i] = 0;
}

static void trim_in_place(char* s){
    size_t start = 0;
    size_t end = text_len(s);
    while(is_space(s[start])) start++;
    while(end > start && is_space(s[end - 1])) end--;
    if(start){
        size_t i = 0;
        while(start < end) s[i++] = s[start++];
        s[i] = 0;
    } else {
        s[end] = 0;
    }
}

static void diag_location(const char* pos, uint32_t* line, uint32_t* col){
    *line = 1;
    *col = 1;
    if(!current_source || !pos || pos < current_source)
        return;
    for(const char* p = current_source; *p && p < pos; p++){
        if(*p == '\n'){
            (*line)++;
            *col = 1;
        } else {
            (*col)++;
        }
    }
}

static void diag_source_line(uint32_t line_no, char* out, size_t max){
    uint32_t line = 1;
    const char* start = current_source ? current_source : "";
    const char* end;
    while(*start && line < line_no){
        if(*start == '\n') line++;
        start++;
    }
    end = start;
    while(*end && *end != '\n') end++;
    copy_span(out, max, start, end);
}

static void diag_set(const char* pos, const char* title, const char* detail, const char* hint){
    if(last_diag.active)
        return;
    last_diag.active = 1;
    copy_text(last_diag.origin, sizeof(last_diag.origin), current_origin ? current_origin : "<eval>");
    diag_location(pos ? pos : current_source, &last_diag.line, &last_diag.col);
    copy_text(last_diag.title, sizeof(last_diag.title), title);
    copy_text(last_diag.detail, sizeof(last_diag.detail), detail);
    copy_text(last_diag.hint, sizeof(last_diag.hint), hint);
    diag_source_line(last_diag.line, last_diag.source_line, sizeof(last_diag.source_line));
}

static void diag_print(void){
    if(!last_diag.active) return;
    console_puts("Rusa found a problem\n");
    console_puts("where: ");
    console_puts(last_diag.origin);
    console_putc(':');
    console_write_dec(last_diag.line);
    console_putc(':');
    console_write_dec(last_diag.col);
    console_putc('\n');
    console_puts("what: ");
    console_puts(last_diag.title);
    console_putc('\n');
    console_puts("plain english: ");
    console_puts(last_diag.detail);
    console_putc('\n');
    console_puts("source: ");
    console_puts(last_diag.source_line);
    console_putc('\n');
    console_puts("        ");
    for(uint32_t i=1; i<last_diag.col; i++)
        console_putc(' ');
    console_puts("^\n");
    console_puts("try: ");
    console_puts(last_diag.hint);
    console_putc('\n');
    console_puts("open: lang open-error\n");
}

static int diag_has(void){
    return last_diag.active;
}

static void diag_clear(void){
    last_diag.active = 0;
    last_diag.title[0] = 0;
    last_diag.detail[0] = 0;
    last_diag.hint[0] = 0;
    last_diag.source_line[0] = 0;
}

static struct rusa_value value_int(int32_t n){
    struct rusa_value v;
    v.type = RUSA_INT;
    v.number = n;
    v.text[0] = 0;
    return v;
}

static struct rusa_value value_bool(int b){
    struct rusa_value v;
    v.type = RUSA_BOOL;
    v.number = b ? 1 : 0;
    v.text[0] = 0;
    return v;
}

static struct rusa_value value_string(const char* s){
    struct rusa_value v;
    v.type = RUSA_STRING;
    v.number = 0;
    copy_text(v.text, sizeof(v.text), s);
    return v;
}

static int value_truth(struct rusa_value v){
    if(v.type == RUSA_STRING) return v.text[0] != 0;
    return v.number != 0;
}

static void value_print(struct rusa_value v){
    if(v.type == RUSA_STRING) console_puts(v.text);
    else if(v.type == RUSA_BOOL) console_puts(v.number ? "true" : "false");
    else console_write_dec((uint32_t)v.number);
}

static int parse_i32(const char* s, int32_t* out){
    int sign = 1;
    int32_t value = 0;
    while(is_space(*s)) s++;
    if(*s == '-'){
        sign = -1;
        s++;
    }
    if(*s < '0' || *s > '9') return 0;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (int32_t)(*s - '0');
        s++;
    }
    *out = value * sign;
    return 1;
}

static enum rusa_type parse_type(const char* s){
    if(str_eq(s, "int")) return RUSA_INT;
    if(str_eq(s, "bool")) return RUSA_BOOL;
    if(str_eq(s, "string")) return RUSA_STRING;
    return RUSA_NONE;
}

static struct rusa_var* var_find(const char* name){
    for(size_t i=0; i<RUSA_VAR_MAX; i++)
        if(runtime.vars[i].name[0] && str_eq(runtime.vars[i].name, name))
            return &runtime.vars[i];
    return 0;
}

static struct rusa_var* var_put(const char* name){
    struct rusa_var* v = var_find(name);
    if(v) return v;
    for(size_t i=0; i<RUSA_VAR_MAX; i++){
        if(runtime.vars[i].name[0] == 0){
            copy_text(runtime.vars[i].name, sizeof(runtime.vars[i].name), name);
            return &runtime.vars[i];
        }
    }
    return 0;
}

static struct rusa_fn* fn_find(const char* name){
    for(size_t i=0; i<RUSA_FN_MAX; i++)
        if(runtime.fns[i].name[0] && str_eq(runtime.fns[i].name, name))
            return &runtime.fns[i];
    return 0;
}

static struct rusa_fn* fn_put(const char* name){
    struct rusa_fn* f = fn_find(name);
    if(f) return f;
    for(size_t i=0; i<RUSA_FN_MAX; i++){
        if(runtime.fns[i].name[0] == 0){
            copy_text(runtime.fns[i].name, sizeof(runtime.fns[i].name), name);
            return &runtime.fns[i];
        }
    }
    return 0;
}

static const char* read_name(const char* p, char* out, size_t max){
    size_t i = 0;
    p = skip_ws(p);
    while(is_name_char(*p) && i + 1 < max)
        out[i++] = *p++;
    while(is_name_char(*p)) p++;
    out[i] = 0;
    return p;
}

static const char* find_matching(const char* open, char left, char right){
    int depth = 0;
    int quoted = 0;
    for(const char* p = open; *p; p++){
        if(*p == '"') quoted = !quoted;
        if(quoted) continue;
        if(*p == left) depth++;
        if(*p == right){
            depth--;
            if(depth == 0) return p;
        }
    }
    return 0;
}

static const char* find_top_brace(const char* p){
    int quoted = 0;
    while(*p){
        if(*p == '"') quoted = !quoted;
        if(!quoted && *p == '{') return p;
        p++;
    }
    return 0;
}

static const char* statement_end(const char* p){
    int quoted = 0;
    int paren = 0;
    int bracket = 0;
    while(*p){
        if(*p == '"') quoted = !quoted;
        if(!quoted){
            if(*p == '(') paren++;
            else if(*p == ')' && paren) paren--;
            else if(*p == '[') bracket++;
            else if(*p == ']' && bracket) bracket--;
            else if(((*p == '\n') || (*p == ';')) && paren == 0 && bracket == 0) return p;
            else if(*p == '{' || *p == '}') return p;
        }
        p++;
    }
    return p;
}

static struct rusa_value eval_expr(const char* expr);
static int eval_block(const char* source);

static const char* parse_primary(const char* p, struct rusa_value* out);

static const char* parse_factor(const char* p, struct rusa_value* out){
    p = skip_ws(p);
    if(*p == '-'){
        p = parse_factor(p + 1, out);
        if(out->type != RUSA_STRING) out->number = -out->number;
        return p;
    }
    return parse_primary(p, out);
}

static const char* parse_term(const char* p, struct rusa_value* out){
    struct rusa_value rhs;
    p = parse_factor(p, out);
    for(;;){
        p = skip_ws(p);
        if(*p != '*' && *p != '/' && *p != '%') return p;
        char op = *p++;
        p = parse_factor(p, &rhs);
        if(out->type == RUSA_STRING || rhs.type == RUSA_STRING) continue;
        if(op == '*') out->number *= rhs.number;
        else if(op == '/' && rhs.number) out->number /= rhs.number;
        else if(op == '%' && rhs.number) out->number %= rhs.number;
    }
}

static const char* parse_sum(const char* p, struct rusa_value* out){
    struct rusa_value rhs;
    p = parse_term(p, out);
    for(;;){
        p = skip_ws(p);
        if(*p != '+' && *p != '-') return p;
        char op = *p++;
        p = parse_term(p, &rhs);
        if(op == '+' && (out->type == RUSA_STRING || rhs.type == RUSA_STRING)){
            char joined[RUSA_TEXT_MAX];
            joined[0] = 0;
            if(out->type == RUSA_STRING) copy_text(joined, sizeof(joined), out->text);
            else {
                joined[0] = (char)('0' + (out->number % 10));
                joined[1] = 0;
            }
            if(rhs.type == RUSA_STRING) {
                size_t pos = text_len(joined);
                for(size_t i=0; rhs.text[i] && pos + 1 < sizeof(joined); i++)
                    joined[pos++] = rhs.text[i];
                joined[pos] = 0;
            }
            *out = value_string(joined);
        } else if(out->type != RUSA_STRING && rhs.type != RUSA_STRING){
            if(op == '+') out->number += rhs.number;
            else out->number -= rhs.number;
        }
    }
}

static const char* parse_compare(const char* p, struct rusa_value* out){
    struct rusa_value rhs;
    p = parse_sum(p, out);
    for(;;){
        p = skip_ws(p);
        int op = 0;
        if(p[0] == '=' && p[1] == '=') op = 1;
        else if(p[0] == '!' && p[1] == '=') op = 2;
        else if(p[0] == '<' && p[1] == '=') op = 3;
        else if(p[0] == '>' && p[1] == '=') op = 4;
        else if(p[0] == '<') op = 5;
        else if(p[0] == '>') op = 6;
        if(!op) return p;
        p += (op <= 4) ? 2 : 1;
        p = parse_sum(p, &rhs);
        int result = 0;
        if(op == 1) result = out->type == RUSA_STRING || rhs.type == RUSA_STRING ?
            str_eq(out->text, rhs.text) : out->number == rhs.number;
        else if(op == 2) result = out->type == RUSA_STRING || rhs.type == RUSA_STRING ?
            !str_eq(out->text, rhs.text) : out->number != rhs.number;
        else if(op == 3) result = out->number <= rhs.number;
        else if(op == 4) result = out->number >= rhs.number;
        else if(op == 5) result = out->number < rhs.number;
        else if(op == 6) result = out->number > rhs.number;
        *out = value_bool(result);
    }
}

static struct rusa_value eval_expr(const char* expr){
    struct rusa_value out = value_int(0);
    parse_compare(expr, &out);
    return out;
}

static struct rusa_value call_function(const char* name, const char* args){
    struct rusa_fn* fn = fn_find(name);
    struct rusa_value values[RUSA_PARAM_MAX];
    struct rusa_value saved[RUSA_PARAM_MAX];
    int had[RUSA_PARAM_MAX];
    const char* p = args;
    uint32_t argc = 0;
    if(!fn){
        diag_set(current_stmt_pos,
                 "function not found",
                 "This function name has not been defined yet. Define it with fn before calling it, or import the module that provides it.",
                 "Example: fn greet(name: string) { print name } then call greet(\"tabla\")");
        return value_int(0);
    }
    while(*p && argc < RUSA_PARAM_MAX){
        const char* start = p;
        int quoted = 0;
        int paren = 0;
        while(*p){
            if(*p == '"') quoted = !quoted;
            else if(!quoted && *p == '(') paren++;
            else if(!quoted && *p == ')' && paren) paren--;
            else if(!quoted && paren == 0 && *p == ',') break;
            p++;
        }
        char expr[96];
        copy_span(expr, sizeof(expr), start, p);
        trim_in_place(expr);
        values[argc++] = eval_expr(expr);
        if(*p == ',') p++;
    }
    for(uint32_t i=0; i<fn->param_count; i++){
        struct rusa_var* old = var_find(fn->params[i]);
        had[i] = old != 0;
        if(old) saved[i] = old->value;
        struct rusa_var* v = var_put(fn->params[i]);
        if(v){
            v->type = i < argc ? values[i].type : RUSA_NONE;
            v->value = i < argc ? values[i] : value_int(0);
        }
    }
    runtime.returning = 0;
    runtime.return_value = value_int(0);
    eval_block(fn->body);
    struct rusa_value result = runtime.return_value;
    runtime.returning = 0;
    for(uint32_t i=0; i<fn->param_count; i++){
        struct rusa_var* v = var_find(fn->params[i]);
        if(v && had[i]) v->value = saved[i];
        else if(v) v->name[0] = 0;
    }
    return result;
}

static const char* parse_primary(const char* p, struct rusa_value* out){
    p = skip_ws(p);
    if(*p == '"'){
        char text[RUSA_TEXT_MAX];
        size_t i = 0;
        p++;
        while(*p && *p != '"' && i + 1 < sizeof(text))
            text[i++] = *p++;
        while(*p && *p != '"') p++;
        if(*p == '"') p++;
        text[i] = 0;
        *out = value_string(text);
        return p;
    }
    if(*p == '('){
        const char* close = find_matching(p, '(', ')');
        char inner[96];
        if(close){
            copy_span(inner, sizeof(inner), p + 1, close);
            *out = eval_expr(inner);
            return close + 1;
        }
    }
    if((*p >= '0' && *p <= '9') || (*p == '-' && p[1] >= '0' && p[1] <= '9')){
        int32_t n = 0;
        parse_i32(p, &n);
        while(*p == '-' || (*p >= '0' && *p <= '9')) p++;
        *out = value_int(n);
        return p;
    }
    if(is_name_start(*p)){
        char name[24];
        p = read_name(p, name, sizeof(name));
        p = skip_ws(p);
        if(*p == '('){
            const char* close = find_matching(p, '(', ')');
            char args[128];
            if(close){
                copy_span(args, sizeof(args), p + 1, close);
                *out = call_function(name, args);
                return close + 1;
            }
        }
        if(str_eq(name, "true")){
            *out = value_bool(1);
        } else if(str_eq(name, "false")){
            *out = value_bool(0);
        } else {
            struct rusa_var* v = var_find(name);
            if(v){
                *out = v->value;
            } else {
                diag_set(current_stmt_pos, "unknown name",
                         "This name has not been created yet. Rusa variables are made with let before they are used.",
                         "Add a line like: let value: int = 0 before using this name.");
                *out = value_int(0);
            }
        }
        return p;
    }
    *out = value_int(0);
    return p;
}

static void assign_var(const char* statement, const char* source_pos, int declare){
    char name[24];
    char type_name[16];
    char expr[128];
    enum rusa_type type = RUSA_NONE;
    const char* p = statement;
    const char* eq;
    if(declare) p += 3;
    else p += 3;
    p = read_name(p, name, sizeof(name));
    if(name[0] == 0){
        diag_set(source_pos ? source_pos : current_stmt_pos,
                 "missing variable name",
                 "A variable statement needs a name after let or set.",
                 "Write something like: let count: int = 0");
        return;
    }
    p = skip_ws(p);
    if(*p == ':'){
        p++;
        p = read_name(p, type_name, sizeof(type_name));
        type = parse_type(type_name);
        p = skip_ws(p);
    }
    if(*p != '='){
        diag_set(source_pos ? source_pos + (p - statement) : current_stmt_pos,
                 "missing equals sign",
                 "Rusa expected '=' before the value you want to store.",
                 "Use: let name: int = 1 or set name = name + 1");
        return;
    }
    eq = p + 1;
    copy_text(expr, sizeof(expr), eq);
    trim_in_place(expr);
    struct rusa_value value = eval_expr(expr);
    struct rusa_var* var = var_put(name);
    if(!var){
        diag_set(source_pos ? source_pos : current_stmt_pos,
                 "too many variables",
                 "This small early runtime has run out of variable slots.",
                 "Reuse a variable name or shorten the program for now.");
        return;
    }
    var->type = type == RUSA_NONE ? value.type : type;
    var->value = value;
}

static int eval_statement(char* statement, const char* source_pos){
    current_stmt_pos = source_pos;
    trim_in_place(statement);
    if(statement[0] == 0) return 0;
    if(str_starts_kw(statement, "let")){
        assign_var(statement, source_pos, 1);
    } else if(str_starts_kw(statement, "set")){
        assign_var(statement, source_pos, 0);
    } else if(str_starts_kw(statement, "return")){
        runtime.return_value = eval_expr(statement + 6);
        runtime.returning = 1;
        return 1;
    } else if(str_starts_kw(statement, "print")){
        struct rusa_value v = eval_expr(statement + 5);
        value_print(v);
        console_putc('\n');
    } else if(str_starts_kw(statement, "run")){
        char command[160];
        copy_text(command, sizeof(command), statement + 3);
        trim_in_place(command);
        if(command[0] == '"' && command[text_len(command) - 1] == '"'){
            command[text_len(command) - 1] = 0;
            if(call_handler) call_handler(command + 1);
        } else if(call_handler) {
            call_handler(command);
        }
    } else if(str_starts_kw(statement, "call")){
        char name[24];
        const char* p = read_name(statement + 4, name, sizeof(name));
        char args[128];
        if(name[0] == 0){
            diag_set(source_pos, "missing function name",
                     "The call statement needs to say which function to run.",
                     "Use: call greet(\"tabla\")");
            return 1;
        }
        copy_text(args, sizeof(args), p);
        trim_in_place(args);
        if(args[0] == '('){
            size_t n = text_len(args);
            if(n && args[n - 1] == ')') args[n - 1] = 0;
            call_function(name, args + 1);
        } else {
            call_function(name, args);
        }
    } else if(fn_find(statement)) {
        call_function(statement, "");
    } else if(object_eval(statement)) {
        return 0;
    } else {
        char name[24];
        read_name(statement, name, sizeof(name));
        if(name[0]){
            diag_set(source_pos, "unknown statement",
                     "Rusa does not know how to run this line as a keyword, function, or OS object.",
                     "Try one of: let, set, print, fn, if, while, repeat, call, run, on, or file[\"...\"]");
            return 1;
        }
    }
    return runtime.returning || diag_has();
}

static void parse_params(struct rusa_fn* fn, const char* start, const char* end){
    fn->param_count = 0;
    const char* p = start;
    while(p < end && fn->param_count < RUSA_PARAM_MAX){
        char name[24];
        char type_name[16];
        p = read_name(p, name, sizeof(name));
        if(name[0] == 0) break;
        copy_text(fn->params[fn->param_count++], sizeof(fn->params[0]), name);
        p = skip_ws(p);
        if(*p == ':'){
            p++;
            p = read_name(p, type_name, sizeof(type_name));
        }
        while(p < end && *p != ',') p++;
        if(*p == ',') p++;
    }
}

static const char* eval_braced_statement(const char* p){
    const char* brace = find_top_brace(p);
    const char* close;
    char header[128];
    char body[RUSA_BODY_MAX];
    if(!brace){
        diag_set(p, "missing opening brace",
                 "This block statement needs a '{' to mark where its body begins.",
                 "Use braces like: while count < 3 { print count }");
        return statement_end(p);
    }
    close = find_matching(brace, '{', '}');
    if(!close){
        diag_set(brace, "missing closing brace",
                 "Rusa found the start of a block, but it never found the matching '}'.",
                 "Add a closing brace at the end of this block.");
        return statement_end(p);
    }
    copy_span(header, sizeof(header), p, brace);
    trim_in_place(header);
    copy_span(body, sizeof(body), brace + 1, close);
    if(str_starts_kw(header, "if")){
        char cond[96];
        copy_text(cond, sizeof(cond), header + 2);
        trim_in_place(cond);
        if(value_truth(eval_expr(cond))){
            eval_block(body);
        } else {
            const char* after = skip_ws(close + 1);
            if(str_starts_kw(after, "else")){
                const char* else_brace = find_top_brace(after);
                const char* else_close = else_brace ? find_matching(else_brace, '{', '}') : 0;
                if(else_brace && else_close){
                    char else_body[RUSA_BODY_MAX];
                    copy_span(else_body, sizeof(else_body), else_brace + 1, else_close);
                    eval_block(else_body);
                    close = else_close;
                }
            }
        }
    } else if(str_starts_kw(header, "while")){
        char cond[96];
        uint32_t guard = 0;
        copy_text(cond, sizeof(cond), header + 5);
        trim_in_place(cond);
        while(value_truth(eval_expr(cond)) && guard++ < RUSA_LOOP_LIMIT && !runtime.returning)
            eval_block(body);
    } else if(str_starts_kw(header, "repeat")){
        struct rusa_value n = eval_expr(header + 6);
        for(int32_t i=0; i<n.number && i<(int32_t)RUSA_LOOP_LIMIT && !runtime.returning; i++)
            eval_block(body);
    } else if(str_starts_kw(header, "fn")){
        char name[24];
        const char* hp = read_name(header + 2, name, sizeof(name));
        const char* open = hp;
        while(*open && *open != '(') open++;
        const char* end = *open == '(' ? find_matching(open, '(', ')') : 0;
        struct rusa_fn* fn = fn_put(name);
        if(name[0] == 0){
            diag_set(p, "missing function name",
                     "A function definition needs a name after fn.",
                     "Use: fn greet(name: string) { print name }");
            return close + 1;
        }
        if(!end){
            diag_set(open, "missing parameter list",
                     "A function definition needs parentheses after its name.",
                     "Use empty parentheses if there are no inputs: fn start() { print \"go\" }");
            return close + 1;
        }
        if(fn){
            parse_params(fn, open + 1, end);
            copy_text(fn->body, sizeof(fn->body), body);
        }
    } else if(str_starts_kw(header, "on")){
        char trigger[64];
        char name[32];
        const char* hp = skip_ws(header + 2);
        if(*hp == '"'){
            const char* end = hp + 1;
            while(*end && *end != '"') end++;
            copy_span(trigger, sizeof(trigger), hp + 1, end);
        } else {
            read_name(hp, trigger, sizeof(trigger));
        }
        name[0] = 'r'; name[1] = 'u'; name[2] = 's'; name[3] = 'a'; name[4] = '-';
        name[5] = 'e'; name[6] = 'v'; name[7] = '-';
        name[8] = (char)('0' + (runtime.event_id % 10));
        name[9] = 0;
        runtime.event_id++;
        if(events_register_source(name, trigger, body) != 0)
            diag_set(p, "event table full",
                     "The OS event table has no free slots for another persistent Rusa handler.",
                     "Remove or reuse an existing event handler.");
    } else if(str_starts_kw(header, "parallel")){
        eval_block(body);
    } else {
        diag_set(p, "unknown block",
                 "This looks like a block, but Rusa does not recognize the keyword before it.",
                 "Use if, while, repeat, fn, on, or parallel before a brace block.");
    }
    return close + 1;
}

static int eval_block(const char* source){
    const char* p = source;
    while(*p && !runtime.returning && !diag_has()){
        p = skip_ws(p);
        if(*p == 0) break;
        if(str_starts_kw(p, "if") || str_starts_kw(p, "while") || str_starts_kw(p, "repeat") ||
           str_starts_kw(p, "fn") || str_starts_kw(p, "on") || str_starts_kw(p, "parallel")){
            p = eval_braced_statement(p);
            continue;
        }
        const char* end = statement_end(p);
        char stmt[192];
        copy_span(stmt, sizeof(stmt), p, end);
        eval_statement(stmt, p);
        p = end;
        if(*p == ';' || *p == '\n') p++;
    }
    return (runtime.returning || diag_has()) ? 1 : 0;
}

static int import_module(const char* name, uint32_t depth){
    char path[64];
    const char* text;
    const char* saved_source = current_source;
    const char* saved_origin = current_origin;
    size_t pos = 0;
    int result;
    if(depth > RUSA_IMPORT_DEPTH) return -1;
    pos = 0;
    pos += 0;
    copy_text(path, sizeof(path), "/lib/rusa/");
    pos = text_len(path);
    for(size_t i=0; name[i] && pos + 6 < sizeof(path); i++)
        path[pos++] = name[i];
    path[pos++] = '.';
    path[pos++] = 'r';
    path[pos++] = 'u';
    path[pos++] = 's';
    path[pos++] = 'a';
    path[pos] = 0;
    if(fs_read(path, &text) != 0) return -1;
    result = lang_run_source(text, path, "");
    current_source = saved_source;
    current_origin = saved_origin;
    diag_clear();
    return result;
}

static int lang_preflight(const char* source){
    int brace_depth = 0;
    int paren_depth = 0;
    int quoted = 0;
    const char* last_brace = source;
    const char* last_paren = source;
    for(const char* p = source; p && *p; p++){
        if(*p == '"')
            quoted = !quoted;
        if(quoted)
            continue;
        if(*p == '{'){
            brace_depth++;
            last_brace = p;
        } else if(*p == '}'){
            brace_depth--;
            if(brace_depth < 0){
                diag_set(p, "extra closing brace",
                         "There is a '}' here, but Rusa is not inside a block that needs closing.",
                         "Remove this brace or add a matching opening brace earlier.");
                return -1;
            }
        } else if(*p == '('){
            paren_depth++;
            last_paren = p;
        } else if(*p == ')'){
            paren_depth--;
            if(paren_depth < 0){
                diag_set(p, "extra closing parenthesis",
                         "There is a ')' here, but Rusa is not inside parentheses.",
                         "Remove this parenthesis or add a matching '(' earlier.");
                return -1;
            }
        }
    }
    if(quoted){
        diag_set(source, "unclosed string",
                 "A string started with a quote, but never closed.",
                 "Add a closing quote, for example: print \"hello\"");
        return -1;
    }
    if(paren_depth > 0){
        diag_set(last_paren, "missing closing parenthesis",
                 "Rusa found '(' but did not find the matching ')'.",
                 "Close the parentheses before the end of the statement.");
        return -1;
    }
    if(brace_depth > 0){
        diag_set(last_brace, "missing closing brace",
                 "Rusa found '{' but did not find the matching '}'.",
                 "Add a closing brace at the end of the block.");
        return -1;
    }
    return 0;
}

int lang_run_source(const char* source, const char* origin, const char* args){
    (void)args;
    const char* p = source;
    current_source = source;
    current_origin = origin ? origin : "<inline>";
    diag_clear();
    runtime.returning = 0;
    runtime.return_value = value_int(0);
    console_puts("rusa ");
    console_puts(origin ? origin : "<inline>");
    console_putc('\n');
    if(lang_preflight(source) != 0){
        diag_print();
        return -1;
    }
    while(*p){
        p = skip_ws(p);
        if(str_starts_kw(p, "import")){
            char stmt[80];
            char name[32];
            const char* end = statement_end(p);
            copy_span(stmt, sizeof(stmt), p + 6, end);
            trim_in_place(stmt);
            if(stmt[0] == '"' && stmt[text_len(stmt) - 1] == '"'){
                stmt[text_len(stmt) - 1] = 0;
                copy_text(name, sizeof(name), stmt + 1);
            } else {
                copy_text(name, sizeof(name), stmt);
            }
            if(import_module(name, 0) != 0){
                diag_set(p, "import not found",
                         "Rusa could not find a reusable module with this name in /lib/rusa.",
                         "Check the spelling or create /lib/rusa/name.rusa.");
                diag_print();
                return -1;
            }
            p = end;
            if(*p == ';' || *p == '\n') p++;
        } else {
            break;
        }
    }
    eval_block(p);
    if(diag_has()){
        diag_print();
        return -1;
    }
    return 0;
}

int lang_run_file(const char* path, const char* args){
    const char* text;
    if(fs_read(path, &text) != 0)
        return -2;
    return lang_run_source(text, path, args);
}

static void lang_event_source(const char* source){
    lang_run_source(source, "<event>", "");
}

void lang_set_call_handler(void (*handler)(char* command)){
    call_handler = handler;
}

void lang_init(void){
    fs_mkdir("/share/rusa");
    fs_mkdir("/lib/rusa");
    fs_write("/share/rusa/README",
        "Rusa is the native Tabla Rusa OS language.\n"
        "It uses readable statements with braces for blocks, typed values, functions, loops, imports, and OS objects.\n"
        "Source files use .rusa. TRX remains the lower-level bytecode format.\n"
        "Diagnostics show plain-English errors with line, column, source highlight, and lang open-error.\n");
    fs_write("/share/rusa/keywords",
        "import\nlet\nset\nfn\nreturn\nif\nelse\nwhile\nrepeat\nparallel\non\nrun\ncall\nprint\ntrue\nfalse\n"
        "file\nprocess\nservice\nwindow\nprogram\nmath\nphys\n");
    fs_write("/share/rusa/examples",
        "import std\n"
        "let total: int = 0\n"
        "while total < 3 {\n"
        "  print total\n"
        "  set total = total + 1\n"
        "}\n"
        "fn greet(name: string) {\n"
        "  print \"hello \" + name\n"
        "}\n"
        "call greet(\"tabla\")\n"
        "on \"fs.write\" { print \"filesystem changed\" }\n"
        "file[\"/home/readme.txt\"].read()\n");
    fs_write("/share/rusa/diagnostics",
        "Rusa diagnostics:\n"
        "lang check /home/projects/app.rusa\n"
        "lang run /home/projects/app.rusa\n"
        "lang last-error\n"
        "lang open-error\n"
        "Errors include where, plain english, source, caret, and an editor jump command.\n");
    fs_write("/share/rusa/objects",
        "file: read exists open write\n"
        "process: trace stop\n"
        "service: start stop status\n"
        "window: focus info\n"
        "program: run\n"
        "net: use net socket plus fd read/write\n");
    fs_write("/lib/rusa/std.rusa",
        "fn say(message: string) {\n"
        "  print message\n"
        "}\n"
        "fn twice(n: int) {\n"
        "  return n * 2\n"
        "}\n");
    fs_write("/lib/rusa/std.trx",
        "name=std\nkind=rusa-stdlib\nentry=bytecode\nabi=rusa:0.1\nformat=TRX1\nbytecode:\n"
        "PRINT Rusa standard library loaded\n"
        "CALL object types\n"
        "HALT\n");
    events_set_source_handler(lang_event_source);
    fs_append_line("/var/log/system.log", "lang: Rusa source parser online");
}

static const char* first_arg(char* arg, char** rest){
    while(is_space(*arg)) arg++;
    char* start = arg;
    while(*arg && !is_space(*arg)) arg++;
    if(*arg){
        *arg = 0;
        arg++;
    }
    while(is_space(*arg)) arg++;
    *rest = arg;
    return start;
}

void lang_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "about")){
        console_puts("Rusa 0.2 - Tabla Rusa OS native source language\n");
        console_puts("features: variables, typed values, expressions, fn, loops, imports, events, OS objects\n");
        console_puts("docs: /share/rusa/README /share/rusa/keywords /share/rusa/examples\n");
    } else if(str_eq(action, "keywords")){
        const char* text;
        if(fs_read("/share/rusa/keywords", &text) == 0) console_puts(text);
    } else if(str_eq(action, "examples")){
        const char* text;
        if(fs_read("/share/rusa/examples", &text) == 0) console_puts(text);
    } else if(str_eq(action, "docs")){
        const char* text;
        if(fs_read("/share/rusa/README", &text) == 0) console_puts(text);
    } else if(str_eq(action, "stdlib")){
        const char* text;
        if(fs_read("/lib/rusa/std.rusa", &text) == 0) console_puts(text);
    } else if(str_eq(action, "std")){
        console_puts("Rusa std modules: std\n");
        console_puts("use: import std inside .rusa, or lang import std\n");
    } else if(str_eq(action, "import")){
        const char* name = first_arg(rest, &rest);
        if(import_module(name, 0) == 0) console_puts("import loaded\n");
        else console_puts("import: module not found\n");
    } else if(str_eq(action, "run")){
        const char* path = first_arg(rest, &rest);
        int result = lang_run_file(path, rest);
        if(result == -2) console_puts("rusa: source not found\n");
    } else if(str_eq(action, "check")){
        const char* path = first_arg(rest, &rest);
        if(path[0] == 0){
            console_puts("usage: lang check PATH\n");
        } else {
            int result = lang_run_file(path, rest);
            if(result == 0) console_puts("rusa check: ok\n");
            else if(result == -2) console_puts("rusa: source not found\n");
        }
    } else if(str_eq(action, "eval")){
        lang_run_source(rest, "<eval>", "");
    } else if(str_eq(action, "last-error") || str_eq(action, "diag")){
        if(last_diag.active) diag_print();
        else console_puts("rusa: no saved diagnostic\n");
    } else if(str_eq(action, "open-error")){
        if(!last_diag.active){
            console_puts("rusa: no saved diagnostic\n");
        } else if(last_diag.origin[0] == '<'){
            console_puts("rusa: this diagnostic came from eval text, not a file\n");
        } else {
            editor_open_at(last_diag.origin, last_diag.line, last_diag.col, last_diag.title);
        }
    } else if(str_eq(action, "object")){
        const char* name = first_arg(rest, &rest);
        const char* text;
        if(fs_read("/share/rusa/objects", &text) != 0){
            console_puts("object docs missing\n");
        } else if(name[0] == 0){
            console_puts(text);
        } else {
            console_puts("Rusa object ");
            console_puts(name);
            console_puts(": see /share/rusa/objects\n");
        }
    } else {
        console_puts("usage: lang about | keywords | examples | docs | stdlib | std | import NAME | run/check PATH | eval SOURCE | last-error | open-error | object [NAME]\n");
    }
}
