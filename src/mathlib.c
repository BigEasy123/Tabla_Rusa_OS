#include "mathlib.h"
#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "process.h"
#include <stdint.h>
#include <stddef.h>

#define MATH_VEC_MAX 8
#define MATH_MAT_MAX 4
#define MATH_OBJECT_MAX 6
#define MATH_JOB_MAX 6

struct math_object {
    char name[16];
    int type;
    int32_t values[16];
    int len;
    int active;
};

struct math_job {
    uint32_t id;
    char expr[96];
    char result[64];
    int active;
    int done;
    uint32_t priority;
};

static struct math_object objects[MATH_OBJECT_MAX];
static struct math_job jobs[MATH_JOB_MAX];
static uint32_t next_job_id = 1;

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char lower_char(char c){
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int str_is(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != *b)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void str_copy(char* dst, const char* src, int max){
    int i = 0;
    if(max <= 0) return;
    while(i + 1 < max && src[i]){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
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

static int32_t parse_i32(const char* s){
    int sign = 1;
    int32_t value = 0;
    while(is_space(*s)) s++;
    if(*s == '-'){
        sign = -1;
        s++;
    }
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (*s - '0');
        s++;
    }
    return value * sign;
}

static uint32_t abs_i32(int32_t x){
    return x < 0 ? (uint32_t)(-x) : (uint32_t)x;
}

static int32_t gcd_i32(int32_t a, int32_t b){
    uint32_t x = abs_i32(a);
    uint32_t y = abs_i32(b);
    while(y){
        uint32_t t = x % y;
        x = y;
        y = t;
    }
    return (int32_t)x;
}

static int32_t mod_i32(int32_t a, int32_t m){
    int32_t r;
    if(m == 0) return 0;
    r = a % m;
    return r < 0 ? r + m : r;
}

static int32_t modpow_i32(int32_t base, int32_t exp, int32_t mod){
    int32_t result = 1;
    base = mod_i32(base, mod);
    while(exp > 0){
        if(exp & 1)
            result = (result * base) % mod;
        base = (base * base) % mod;
        exp >>= 1;
    }
    return result;
}

static int parse_vec(char* arg, int32_t* out, int max){
    int n = 0;
    char* rest = arg;
    while(n < max){
        const char* item = first_arg(rest, &rest);
        if(item[0] == 0)
            break;
        out[n++] = parse_i32(item);
    }
    return n;
}

static int parse_vec_until_bar(char* arg, int32_t* out, int max, char** after_bar){
    int n = 0;
    char* rest = arg;
    while(n < max){
        const char* item = first_arg(rest, &rest);
        if(item[0] == 0)
            break;
        if(item[0] == '|' && item[1] == 0){
            *after_bar = rest;
            return n;
        }
        out[n++] = parse_i32(item);
    }
    *after_bar = rest;
    return n;
}

static void print_i32(int32_t v){
    if(v < 0){
        console_putc('-');
        console_write_dec((uint32_t)(-v));
    } else {
        console_write_dec((uint32_t)v);
    }
}

static void print_vec(const int32_t* v, int n){
    console_putc('[');
    for(int i=0; i<n; i++){
        if(i) console_puts(", ");
        print_i32(v[i]);
    }
    console_puts("]\n");
}

struct rat {
    int32_t num;
    int32_t den;
};

static struct rat rat_norm(int32_t num, int32_t den){
    struct rat r;
    int32_t g;
    if(den == 0){
        r.num = 0;
        r.den = 1;
        return r;
    }
    if(den < 0){
        num = -num;
        den = -den;
    }
    g = gcd_i32(num, den);
    r.num = g ? num / g : num;
    r.den = g ? den / g : den;
    return r;
}

static struct rat parse_rat(const char* s){
    int32_t num = parse_i32(s);
    int32_t den = 1;
    while(*s && *s != '/') s++;
    if(*s == '/')
        den = parse_i32(s + 1);
    return rat_norm(num, den);
}

static void print_rat(struct rat r){
    print_i32(r.num);
    if(r.den != 1){
        console_putc('/');
        print_i32(r.den);
    }
}

static struct math_object* object_find(const char* name){
    for(int i=0; i<MATH_OBJECT_MAX; i++)
        if(objects[i].active && str_is(objects[i].name, name))
            return &objects[i];
    return 0;
}

static struct math_object* object_alloc(const char* name){
    struct math_object* existing = object_find(name);
    if(existing)
        return existing;
    for(int i=0; i<MATH_OBJECT_MAX; i++){
        if(!objects[i].active){
            objects[i].active = 1;
            str_copy(objects[i].name, name, sizeof(objects[i].name));
            return &objects[i];
        }
    }
    return 0;
}

static struct math_job* job_find(uint32_t id){
    for(int i=0; i<MATH_JOB_MAX; i++)
        if(jobs[i].active && jobs[i].id == id)
            return &jobs[i];
    return 0;
}

static struct math_job* job_alloc(void){
    for(int i=0; i<MATH_JOB_MAX; i++){
        if(!jobs[i].active){
            jobs[i].active = 1;
            jobs[i].done = 0;
            jobs[i].id = next_job_id++;
            jobs[i].priority = 50;
            jobs[i].expr[0] = 0;
            jobs[i].result[0] = 0;
            return &jobs[i];
        }
    }
    return 0;
}

static void math_vec(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t a[MATH_VEC_MAX];
    int32_t b[MATH_VEC_MAX];
    char* after;
    int n = parse_vec_until_bar(rest, a, MATH_VEC_MAX, &after);
    int m = parse_vec(after, b, MATH_VEC_MAX);
    process_set_running("compute", 1);
    process_set_compute("compute", "vector", 98, 0);
    process_tick("compute", 2);
    if(str_is(action, "dot")){
        int32_t sum = 0;
        int count = n < m ? n : m;
        for(int i=0; i<count; i++)
            sum += a[i] * b[i];
        print_i32(sum);
        console_putc('\n');
    } else if(str_is(action, "add")){
        int32_t out[MATH_VEC_MAX];
        int count = n < m ? n : m;
        for(int i=0; i<count; i++)
            out[i] = a[i] + b[i];
        print_vec(out, count);
    } else if(str_is(action, "axpy")){
        int32_t alpha = n > 0 ? a[0] : 0;
        int count = (n - 1) < m ? (n - 1) : m;
        int32_t out[MATH_VEC_MAX];
        for(int i=0; i<count; i++)
            out[i] = alpha * a[i + 1] + b[i];
        print_vec(out, count);
    } else if(str_is(action, "norm2")){
        int32_t sum = 0;
        for(int i=0; i<n; i++)
            sum += a[i] * a[i];
        console_puts("norm2=");
        print_i32(sum);
        console_putc('\n');
    } else {
        console_puts("usage: math vec dot|add A... | B... ; math vec axpy ALPHA X... | Y...\n");
    }
}

static void math_mat(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t v[16];
    int n = parse_vec(rest, v, 16);
    process_set_running("compute", 1);
    process_set_compute("compute", "linear-algebra", 99, 0);
    process_tick("compute", 4);
    if(str_is(action, "det2") && n >= 4){
        print_i32(v[0] * v[3] - v[1] * v[2]);
        console_putc('\n');
    } else if(str_is(action, "det3") && n >= 9){
        int32_t det =
            v[0]*(v[4]*v[8]-v[5]*v[7]) -
            v[1]*(v[3]*v[8]-v[5]*v[6]) +
            v[2]*(v[3]*v[7]-v[4]*v[6]);
        print_i32(det);
        console_putc('\n');
    } else if(str_is(action, "mul2") && n >= 8){
        int32_t out[4];
        out[0] = v[0]*v[4] + v[1]*v[6];
        out[1] = v[0]*v[5] + v[1]*v[7];
        out[2] = v[2]*v[4] + v[3]*v[6];
        out[3] = v[2]*v[5] + v[3]*v[7];
        print_vec(out, 4);
    } else if(str_is(action, "trace2") && n >= 4){
        print_i32(v[0] + v[3]);
        console_putc('\n');
    } else if(str_is(action, "transpose2") && n >= 4){
        int32_t out[4] = {v[0], v[2], v[1], v[3]};
        print_vec(out, 4);
    } else if(str_is(action, "inv2") && n >= 4){
        int32_t det = v[0] * v[3] - v[1] * v[2];
        if(det == 0) console_puts("singular\n");
        else {
            console_puts("1/");
            print_i32(det);
            console_puts(" * ");
            int32_t out[4] = {v[3], -v[1], -v[2], v[0]};
            print_vec(out, 4);
        }
    } else if(str_is(action, "solve2") && n >= 6){
        int32_t det = v[0] * v[3] - v[1] * v[2];
        if(det == 0) console_puts("singular\n");
        else {
            struct rat x = rat_norm(v[4] * v[3] - v[1] * v[5], det);
            struct rat y = rat_norm(v[0] * v[5] - v[4] * v[2], det);
            console_puts("x=");
            print_rat(x);
            console_puts(" y=");
            print_rat(y);
            console_putc('\n');
        }
    } else if(str_is(action, "charpoly2") && n >= 4){
        console_puts("lambda^2");
        int32_t tr = v[0] + v[3];
        int32_t det = v[0] * v[3] - v[1] * v[2];
        console_puts(tr >= 0 ? " - " : " + ");
        print_i32(tr >= 0 ? tr : -tr);
        console_puts("lambda");
        console_puts(det >= 0 ? " + " : " - ");
        print_i32(det >= 0 ? det : -det);
        console_putc('\n');
    } else {
        console_puts("usage: math mat det2|det3|mul2|trace2|transpose2|inv2|solve2|charpoly2\n");
    }
}

static void math_num(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* a = first_arg(rest, &rest);
    const char* b = first_arg(rest, &rest);
    const char* c = first_arg(rest, &rest);
    int32_t x = parse_i32(a);
    int32_t y = parse_i32(b);
    int32_t z = parse_i32(c);
    process_set_running("compute", 1);
    process_set_compute("compute", "number-theory", 94, 0);
    process_tick("compute", 2);
    if(str_is(action, "gcd")){
        print_i32(gcd_i32(x, y));
        console_putc('\n');
    } else if(str_is(action, "lcm")){
        int32_t g = gcd_i32(x, y);
        print_i32(g ? (x / g) * y : 0);
        console_putc('\n');
    } else if(str_is(action, "modpow")){
        print_i32(modpow_i32(x, y, z));
        console_putc('\n');
    } else if(str_is(action, "prime")){
        int prime = x > 1;
        for(int32_t d=2; d*d<=x; d++)
            if(x % d == 0)
                prime = 0;
        console_puts(prime ? "prime\n" : "composite\n");
    } else {
        console_puts("usage: math num gcd A B | lcm A B | modpow A E M | prime N\n");
    }
}

static void math_group(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t n = parse_i32(first_arg(rest, &rest));
    process_set_running("compute", 1);
    process_set_compute("compute", "group-theory", 92, 0);
    process_tick("compute", 3);
    if(n <= 0 || n > 12){
        console_puts("group: use order 1..12\n");
        return;
    }
    if(str_is(action, "cyclic")){
        for(int32_t r=0; r<n; r++){
            for(int32_t c=0; c<n; c++){
                print_i32((r + c) % n);
                console_putc(c + 1 == n ? '\n' : ' ');
            }
        }
    } else if(str_is(action, "units")){
        console_puts("units mod ");
        print_i32(n);
        console_puts(": ");
        for(int32_t i=1; i<n; i++)
            if(gcd_i32(i, n) == 1){
                print_i32(i);
                console_putc(' ');
            }
        console_putc('\n');
    } else if(str_is(action, "order")){
        int32_t g = parse_i32(first_arg(rest, &rest));
        int32_t x = 0;
        for(int32_t k=1; k<=n; k++){
            x = (x + g) % n;
            if(x == 0){
                print_i32(k);
                console_putc('\n');
                return;
            }
        }
        console_puts("unknown\n");
    } else {
        console_puts("usage: math group cyclic N | units N | order N GENERATOR\n");
    }
}

static void math_stats(char* arg){
    int32_t v[MATH_VEC_MAX];
    int n = parse_vec(arg, v, MATH_VEC_MAX);
    int32_t sum = 0;
    int32_t min = n ? v[0] : 0;
    int32_t max = n ? v[0] : 0;
    for(int i=0; i<n; i++){
        sum += v[i];
        if(v[i] < min) min = v[i];
        if(v[i] > max) max = v[i];
    }
    process_set_running("compute", 1);
    process_set_compute("compute", "statistics", 90, 0);
    process_tick("compute", 1);
    console_puts("n=");
    console_write_dec((uint32_t)n);
    console_puts(" sum=");
    print_i32(sum);
    console_puts(" mean=");
    print_i32(n ? sum / n : 0);
    console_puts(" min=");
    print_i32(min);
    console_puts(" max=");
    print_i32(max);
    console_putc('\n');
}

static int parse_bool_word(const char* s){
    return str_is(s, "true") || str_is(s, "yes") || str_is(s, "1");
}

static void math_logic(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(str_is(action, "implies")){
        int a = parse_bool_word(first_arg(rest, &rest));
        int b = parse_bool_word(first_arg(rest, &rest));
        console_puts((!a || b) ? "true\n" : "false\n");
    } else if(str_is(action, "modus")){
        int premise = parse_bool_word(first_arg(rest, &rest));
        int implication = parse_bool_word(first_arg(rest, &rest));
        if(premise && implication)
            console_puts("valid: conclusion follows\n");
        else
            console_puts("not proven: premise or implication is false\n");
    } else if(str_is(action, "and")){
        int a = parse_bool_word(first_arg(rest, &rest));
        int b = parse_bool_word(first_arg(rest, &rest));
        console_puts((a && b) ? "true\n" : "false\n");
    } else if(str_is(action, "or")){
        int a = parse_bool_word(first_arg(rest, &rest));
        int b = parse_bool_word(first_arg(rest, &rest));
        console_puts((a || b) ? "true\n" : "false\n");
    } else {
        console_puts("usage: math logic implies A B | modus PREMISE IMPLICATION | and A B | or A B\n");
    }
    process_set_running("compute", 1);
    process_set_compute("compute", "logic-proof", 80, 0);
    process_tick("compute", 1);
    jobs_account("proof-worker", 1);
}

static void math_poly(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t x = parse_i32(first_arg(rest, &rest));
    int32_t coeff[MATH_VEC_MAX];
    int n = parse_vec(rest, coeff, MATH_VEC_MAX);
    int32_t y = 0;
    process_set_running("compute", 1);
    process_set_compute("compute", "polynomial", 91, 0);
    process_tick("compute", 2);
    if(str_is(action, "eval")){
        for(int i=0; i<n; i++)
            y = y * x + coeff[i];
        print_i32(y);
        console_putc('\n');
    } else {
        console_puts("usage: math poly eval X Cn ... C0\n");
    }
}

static void math_rat(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    struct rat a = parse_rat(first_arg(rest, &rest));
    struct rat b = parse_rat(first_arg(rest, &rest));
    struct rat r;
    process_set_running("compute", 1);
    process_set_compute("compute", "rational", 93, 0);
    process_tick("compute", 2);
    if(str_is(action, "add")){
        r = rat_norm(a.num * b.den + b.num * a.den, a.den * b.den);
        print_rat(r); console_putc('\n');
    } else if(str_is(action, "sub")){
        r = rat_norm(a.num * b.den - b.num * a.den, a.den * b.den);
        print_rat(r); console_putc('\n');
    } else if(str_is(action, "mul")){
        r = rat_norm(a.num * b.num, a.den * b.den);
        print_rat(r); console_putc('\n');
    } else if(str_is(action, "div")){
        r = rat_norm(a.num * b.den, a.den * b.num);
        print_rat(r); console_putc('\n');
    } else if(str_is(action, "reduce")){
        print_rat(a); console_putc('\n');
    } else if(str_is(action, "latex")){
        console_puts("\\frac{");
        print_i32(a.num);
        console_puts("}{");
        print_i32(a.den);
        console_puts("}\n");
    } else {
        console_puts("usage: math rat add|sub|mul|div A/B C/D | reduce A/B | latex A/B\n");
    }
}

static int32_t mod_inv(int32_t a, int32_t m){
    for(int32_t x=1; x<m; x++)
        if(mod_i32(a * x, m) == 1)
            return x;
    return 0;
}

static void math_modmat(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t mod = parse_i32(first_arg(rest, &rest));
    int32_t v[8];
    int n = parse_vec(rest, v, 8);
    process_set_running("compute", 1);
    process_set_compute("compute", "modular-linear-algebra", 97, 0);
    process_tick("compute", 4);
    if(mod <= 1){
        console_puts("modmat: modulus must be >1\n");
        return;
    }
    if(str_is(action, "det2") && n >= 4){
        print_i32(mod_i32(v[0] * v[3] - v[1] * v[2], mod));
        console_putc('\n');
    } else if(str_is(action, "mul2") && n >= 8){
        int32_t out[4];
        out[0] = mod_i32(v[0]*v[4] + v[1]*v[6], mod);
        out[1] = mod_i32(v[0]*v[5] + v[1]*v[7], mod);
        out[2] = mod_i32(v[2]*v[4] + v[3]*v[6], mod);
        out[3] = mod_i32(v[2]*v[5] + v[3]*v[7], mod);
        print_vec(out, 4);
    } else if(str_is(action, "inv2") && n >= 4){
        int32_t det = mod_i32(v[0] * v[3] - v[1] * v[2], mod);
        int32_t inv = mod_inv(det, mod);
        if(inv == 0) console_puts("not invertible\n");
        else {
            int32_t out[4] = {
                mod_i32(inv * v[3], mod),
                mod_i32(inv * -v[1], mod),
                mod_i32(inv * -v[2], mod),
                mod_i32(inv * v[0], mod)
            };
            print_vec(out, 4);
        }
    } else {
        console_puts("usage: math modmat det2|mul2|inv2 MOD VALUES...\n");
    }
}

static void math_sym(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t coeff[MATH_VEC_MAX];
    int n = parse_vec(rest, coeff, MATH_VEC_MAX);
    process_set_running("compute", 1);
    process_set_compute("compute", "symbolic-polynomial", 89, 0);
    process_tick("compute", 3);
    if(str_is(action, "diff")){
        for(int i=0; i<n-1; i++){
            int power = n - i - 1;
            int32_t c = coeff[i] * power;
            if(i && c >= 0) console_puts("+");
            print_i32(c);
            if(power - 1 > 0){
                console_puts("x");
                if(power - 1 > 1){
                    console_puts("^");
                    console_write_dec((uint32_t)(power - 1));
                }
            }
        }
        console_putc('\n');
    } else if(str_is(action, "latex")){
        char tmp[96] = "";
        (void)tmp;
        for(int i=0; i<n; i++){
            int power = n - i - 1;
            if(i && coeff[i] >= 0) console_puts("+");
            print_i32(coeff[i]);
            if(power > 0){
                console_puts("x");
                if(power > 1){
                    console_puts("^{");
                    console_write_dec((uint32_t)power);
                    console_puts("}");
                }
            }
        }
        console_putc('\n');
    } else if(str_is(action, "simplify")){
        console_puts("polynomial canonical form: ");
        for(int i=0; i<n; i++){
            if(coeff[i] == 0) continue;
            if(i && coeff[i] > 0) console_puts("+");
            print_i32(coeff[i]);
            int power = n - i - 1;
            if(power > 0) console_puts("x");
        }
        console_putc('\n');
    } else {
        console_puts("usage: math sym diff|latex|simplify COEFF...\n");
    }
}

static void math_object_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || str_is(action, "list")){
        for(int i=0; i<MATH_OBJECT_MAX; i++){
            if(objects[i].active){
                console_puts(objects[i].type == 1 ? "vector " : "matrix ");
                console_puts(objects[i].name);
                console_puts(" = ");
                print_vec(objects[i].values, objects[i].len);
            }
        }
        return;
    }
    if(str_is(action, "vector")){
        struct math_object* obj = object_alloc(name);
        if(!obj){
            console_puts("object: table full\n");
            return;
        }
        obj->type = 1;
        obj->len = parse_vec(rest, obj->values, MATH_VEC_MAX);
        process_set_running("compute", 1);
        process_set_compute("compute", "math-workspace", 96, 0);
        console_puts("stored vector ");
        console_puts(obj->name);
        console_putc('\n');
    } else if(str_is(action, "matrix")){
        struct math_object* obj = object_alloc(name);
        if(!obj){
            console_puts("object: table full\n");
            return;
        }
        obj->type = 2;
        obj->len = parse_vec(rest, obj->values, 16);
        process_set_running("compute", 1);
        process_set_compute("compute", "math-workspace", 96, 0);
        console_puts("stored matrix ");
        console_puts(obj->name);
        console_putc('\n');
    } else if(str_is(action, "dot")){
        struct math_object* a = object_find(name);
        const char* bname = first_arg(rest, &rest);
        struct math_object* b = object_find(bname);
        int32_t sum = 0;
        if(!a || !b || a->type != 1 || b->type != 1){
            console_puts("object dot: need two vectors\n");
            return;
        }
        int count = a->len < b->len ? a->len : b->len;
        for(int i=0; i<count; i++)
            sum += a->values[i] * b->values[i];
        print_i32(sum);
        console_putc('\n');
        process_tick("compute", 2);
    } else if(str_is(action, "det")){
        struct math_object* obj = object_find(name);
        if(!obj || obj->type != 2 || obj->len < 4){
            console_puts("object det: need 2x2 matrix\n");
            return;
        }
        print_i32(obj->values[0] * obj->values[3] - obj->values[1] * obj->values[2]);
        console_putc('\n');
        process_tick("compute", 3);
    } else if(str_is(action, "show")){
        struct math_object* obj = object_find(name);
        if(!obj) console_puts("object: not found\n");
        else print_vec(obj->values, obj->len);
    } else if(str_is(action, "save")){
        struct math_object* obj = object_find(name);
        const char* path = first_arg(rest, &rest);
        char text[256];
        int pos = 0;
        if(!obj){
            console_puts("object: not found\n");
            return;
        }
        text[pos++] = obj->type == 1 ? 'v' : 'm';
        text[pos++] = ' ';
        for(int i=0; i<obj->len && pos + 12 < (int)sizeof(text); i++){
            int32_t val = obj->values[i];
            if(val < 0){
                text[pos++] = '-';
                val = -val;
            }
            char digits[12];
            int d = 0;
            if(val == 0) digits[d++] = '0';
            while(val > 0 && d < 11){
                digits[d++] = (char)('0' + (val % 10));
                val /= 10;
            }
            while(d > 0) text[pos++] = digits[--d];
            text[pos++] = ' ';
        }
        text[pos] = 0;
        fs_write(path, text);
        console_puts("saved ");
        console_puts(path);
        console_putc('\n');
    } else if(str_is(action, "load")){
        const char* path = first_arg(rest, &rest);
        const char* text;
        char buf[256];
        char* p;
        if(fs_read(path, &text) != 0){
            console_puts("load: not found\n");
            return;
        }
        str_copy(buf, text, sizeof(buf));
        p = buf;
        const char* type = first_arg(p, &p);
        struct math_object* obj = object_alloc(name);
        if(!obj){
            console_puts("object: table full\n");
            return;
        }
        obj->type = type[0] == 'm' ? 2 : 1;
        obj->len = parse_vec(p, obj->values, 16);
        console_puts("loaded ");
        console_puts(name);
        console_putc('\n');
    } else {
        console_puts("usage: math object list|vector|matrix|dot|det|show|save|load\n");
    }
}

static void latex_vec(const int32_t* v, int n){
    console_puts("\\begin{bmatrix}");
    for(int i=0; i<n; i++){
        if(i) console_puts(" \\\\ ");
        print_i32(v[i]);
    }
    console_puts("\\end{bmatrix}\n");
}

static void latex_mat2(const int32_t* v){
    console_puts("\\begin{bmatrix}");
    print_i32(v[0]); console_puts(" & "); print_i32(v[1]);
    console_puts(" \\\\ ");
    print_i32(v[2]); console_puts(" & "); print_i32(v[3]);
    console_puts("\\end{bmatrix}\n");
}

static void math_latex(char* arg){
    char* rest;
    const char* kind = first_arg(arg, &rest);
    int32_t v[16];
    int n;
    process_set_running("compute", 1);
    process_set_compute("compute", "latex-convert", 80, 0);
    process_tick("compute", 1);
    if(str_is(kind, "vec")){
        n = parse_vec(rest, v, MATH_VEC_MAX);
        latex_vec(v, n);
    } else if(str_is(kind, "mat2")){
        n = parse_vec(rest, v, 4);
        if(n < 4) console_puts("latex mat2: need 4 values\n");
        else latex_mat2(v);
    } else if(str_is(kind, "frac")){
        const char* a = first_arg(rest, &rest);
        const char* b = first_arg(rest, &rest);
        console_puts("\\frac{");
        console_puts(a);
        console_puts("}{");
        console_puts(b);
        console_puts("}\n");
    } else if(str_is(kind, "poly")){
        n = parse_vec(rest, v, MATH_VEC_MAX);
        for(int i=0; i<n; i++){
            int power = n - i - 1;
            if(i && v[i] >= 0) console_puts("+");
            print_i32(v[i]);
            if(power > 0){
                console_puts("x");
                if(power > 1){
                    console_puts("^{");
                    console_write_dec((uint32_t)power);
                    console_puts("}");
                }
            }
        }
        console_putc('\n');
    } else if(str_is(kind, "object")){
        const char* name = first_arg(rest, &rest);
        struct math_object* obj = object_find(name);
        if(!obj) console_puts("latex object: not found\n");
        else if(obj->type == 1) latex_vec(obj->values, obj->len);
        else if(obj->len >= 4) latex_mat2(obj->values);
    } else {
        console_puts("usage: math latex vec V... | mat2 A B C D | frac A B | poly C... | object NAME\n");
    }
}

static void math_job(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_is(action, "list")){
        for(int i=0; i<MATH_JOB_MAX; i++){
            if(jobs[i].active){
                console_puts(jobs[i].done ? "[done] " : "[new]  ");
                console_write_dec(jobs[i].id);
                console_puts(" prio=");
                console_write_dec(jobs[i].priority);
                console_puts(" ");
                console_puts(jobs[i].expr);
                console_putc('\n');
            }
        }
        return;
    }
    if(str_is(action, "submit")){
        struct math_job* job = job_alloc();
        if(!job){
            console_puts("job: table full\n");
            return;
        }
        str_copy(job->expr, rest, sizeof(job->expr));
        str_copy(job->result, "queued", sizeof(job->result));
        console_puts("job ");
        console_write_dec(job->id);
        console_puts(" submitted\n");
    } else if(str_is(action, "run")){
        uint32_t id = (uint32_t)parse_i32(first_arg(rest, &rest));
        struct math_job* job = job_find(id);
        char expr[96];
        if(!job){
            console_puts("job: not found\n");
            return;
        }
        str_copy(expr, job->expr, sizeof(expr));
        math_cmd(expr);
        job->done = 1;
        str_copy(job->result, "done; output printed to console", sizeof(job->result));
    } else if(str_is(action, "result")){
        uint32_t id = (uint32_t)parse_i32(first_arg(rest, &rest));
        struct math_job* job = job_find(id);
        if(!job) console_puts("job: not found\n");
        else {
            console_puts(job->result);
            console_putc('\n');
        }
    } else if(str_is(action, "run-all")){
        for(int i=0; i<MATH_JOB_MAX; i++){
            if(jobs[i].active && !jobs[i].done){
                char expr[96];
                str_copy(expr, jobs[i].expr, sizeof(expr));
                console_puts("running job ");
                console_write_dec(jobs[i].id);
                console_putc('\n');
                math_cmd(expr);
                jobs[i].done = 1;
                str_copy(jobs[i].result, "done; output printed to console", sizeof(jobs[i].result));
            }
        }
    } else if(str_is(action, "clear")){
        for(int i=0; i<MATH_JOB_MAX; i++)
            jobs[i].active = 0;
        console_puts("jobs cleared\n");
    } else if(str_is(action, "priority")){
        uint32_t id = (uint32_t)parse_i32(first_arg(rest, &rest));
        uint32_t prio = (uint32_t)parse_i32(first_arg(rest, &rest));
        struct math_job* job = job_find(id);
        if(!job) console_puts("job: not found\n");
        else {
            job->priority = prio;
            console_puts("priority set\n");
        }
    } else {
        console_puts("usage: math job list|submit|run|run-all|result|priority|clear\n");
    }
}

static void math_bench(char* arg){
    char* rest;
    const char* target = first_arg(arg, &rest);
    uint32_t ticks = 0;
    if(target[0] == 0 || str_is(target, "math") || str_is(target, "all")){
        ticks = 64;
        process_set_compute("compute", "benchmark-suite", 99, 0);
    } else if(str_is(target, "vec")){
        ticks = 16;
        process_set_compute("compute", "vector-benchmark", 98, 0);
    } else if(str_is(target, "mat")){
        ticks = 24;
        process_set_compute("compute", "matrix-benchmark", 99, 0);
    } else if(str_is(target, "num")){
        ticks = 12;
        process_set_compute("compute", "number-benchmark", 94, 0);
    } else {
        console_puts("usage: math bench [math|vec|mat|num]\n");
        return;
    }
    process_set_running("compute", 1);
    process_tick("compute", ticks);
    console_puts("bench ");
    console_puts(target[0] ? target : "math");
    console_puts(": ticks+=");
    console_write_dec(ticks);
    console_putc('\n');
}

static void math_phys_account(const char* workload, uint32_t ticks){
    process_set_running("compute", 1);
    process_set_compute("compute", workload, 99, 0);
    process_tick("compute", ticks);
    jobs_account("physics-worker", ticks);
}

static void math_phys(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    int32_t v[8];
    int n = parse_vec(rest, v, 8);
    if(action[0] == 0 || str_is(action, "help")){
        console_puts("math phys grav M m r        -> F = G M m / r^2, G scaled as 667\n");
        console_puts("math phys electric q1 q2 r  -> F = k q1 q2 / r^2, k scaled as 899\n");
        console_puts("math phys magnetic q v b    -> F = q v B\n");
        console_puts("math phys energy m v        -> KE = m v^2 / 2\n");
        console_puts("math phys orbit M r         -> v^2 = G M / r\n");
        console_puts("math phys fields Ex Ey Ez | Bx By Bz\n");
        return;
    }
    if(str_is(action, "grav") && n >= 3){
        int32_t r2 = v[2] * v[2];
        math_phys_account("first-principles-gravity", 12);
        console_puts("gravity F_scaled=");
        print_i32(r2 ? (667 * v[0] * v[1]) / r2 : 0);
        console_puts(" direction=attractive ticks+=12\n");
    } else if(str_is(action, "electric") && n >= 3){
        int32_t r2 = v[2] * v[2];
        math_phys_account("first-principles-electric", 12);
        console_puts("electric F_scaled=");
        print_i32(r2 ? (899 * v[0] * v[1]) / r2 : 0);
        console_puts(" sign=");
        console_puts((v[0] * v[1]) >= 0 ? "repulsive" : "attractive");
        console_putc('\n');
    } else if(str_is(action, "magnetic") && n >= 3){
        math_phys_account("first-principles-magnetic", 10);
        console_puts("magnetic F_scaled=");
        print_i32(v[0] * v[1] * v[2]);
        console_puts(" using perpendicular qvB\n");
    } else if(str_is(action, "energy") && n >= 2){
        math_phys_account("first-principles-energy", 6);
        console_puts("kinetic=");
        print_i32((v[0] * v[1] * v[1]) / 2);
        console_putc('\n');
    } else if(str_is(action, "orbit") && n >= 2){
        math_phys_account("first-principles-orbit", 14);
        console_puts("orbital_v2_scaled=");
        print_i32(v[1] ? (667 * v[0]) / v[1] : 0);
        console_putc('\n');
    } else if(str_is(action, "fields")){
        char* after;
        int32_t e[3];
        int32_t b[3];
        int en = parse_vec_until_bar(rest, e, 3, &after);
        int bn = parse_vec(after, b, 3);
        math_phys_account("field-theory", 16);
        console_puts("E=");
        print_vec(e, en);
        console_puts("B=");
        print_vec(b, bn);
        console_puts("field energy density scaled=");
        int32_t es = 0;
        int32_t bs = 0;
        for(int i=0; i<en; i++) es += e[i] * e[i];
        for(int i=0; i<bn; i++) bs += b[i] * b[i];
        print_i32((es + bs) / 2);
        console_putc('\n');
    } else {
        console_puts("usage: math phys help | grav | electric | magnetic | energy | orbit | fields\n");
    }
}

void math_help(void){
    console_puts("math vec dot|add|axpy|norm2\n");
    console_puts("math mat det2|det3|mul2|trace2\n");
    console_puts("math num gcd|lcm|modpow|prime\n");
    console_puts("math group cyclic|units|order\n");
    console_puts("math stats N...\n");
    console_puts("math logic implies|modus|and|or\n");
    console_puts("math poly eval X COEFF...\n");
    console_puts("math object list|vector|matrix|dot|det|show\n");
    console_puts("math rat add|sub|mul|div|reduce|latex\n");
    console_puts("math modmat det2|mul2|inv2\n");
    console_puts("math sym diff|latex|simplify\n");
    console_puts("math job list|submit|run|run-all|result|priority|clear\n");
    console_puts("math latex vec|mat2|frac|poly|object\n");
    console_puts("math bench math|vec|mat|num\n");
    console_puts("math phys grav|electric|magnetic|energy|orbit|fields\n");
}

void math_cmd(char* arg){
    char* rest;
    const char* topic = first_arg(arg, &rest);
    if(topic[0] == 0 || str_is(topic, "help")) math_help();
    else if(str_is(topic, "vec")) math_vec(rest);
    else if(str_is(topic, "mat")) math_mat(rest);
    else if(str_is(topic, "num")) math_num(rest);
    else if(str_is(topic, "group")) math_group(rest);
    else if(str_is(topic, "rat")) math_rat(rest);
    else if(str_is(topic, "modmat")) math_modmat(rest);
    else if(str_is(topic, "sym")) math_sym(rest);
    else if(str_is(topic, "stats")) math_stats(rest);
    else if(str_is(topic, "logic") || str_is(topic, "proof")) math_logic(rest);
    else if(str_is(topic, "poly")) math_poly(rest);
    else if(str_is(topic, "object") || str_is(topic, "obj")) math_object_cmd(rest);
    else if(str_is(topic, "job")) math_job(rest);
    else if(str_is(topic, "latex") || str_is(topic, "tex")) math_latex(rest);
    else if(str_is(topic, "bench")) math_bench(rest);
    else if(str_is(topic, "phys") || str_is(topic, "physics")) math_phys(rest);
    else console_puts("math: unknown topic\n");
}
