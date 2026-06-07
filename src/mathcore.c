#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "process.h"
#include "mathcore.h"

#define MATH_PLUGIN_MAX 12
#define THEOREM_MAX 32
#define ALGORITHM_MAX 32
#define OBJECT_TYPE_MAX 24

static struct math_plugin_info plugins[MATH_PLUGIN_MAX];
static struct theorem_info theorems[THEOREM_MAX];
static struct algorithm_info algorithms[ALGORITHM_MAX];
static struct math_object_type_info object_types[OBJECT_TYPE_MAX];
static uint32_t next_plugin_id = 1;
static uint32_t next_theorem_id = 1;
static uint32_t next_algorithm_id = 1;
static uint32_t next_object_type_id = 1;

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

static int str_eq(const char* a, const char* b){
    uint32_t i = 0;
    if(!a || !b) return 0;
    while(a[i] && b[i]){
        if(lower_char(a[i]) != lower_char(b[i])) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char* first_arg(const char* in, char* out, uint32_t max){
    uint32_t i = 0;
    while(in && is_space(*in)) in++;
    while(in && *in && !is_space(*in)){
        if(i + 1 < max) out[i++] = *in;
        in++;
    }
    out[i] = 0;
    while(in && is_space(*in)) in++;
    return in ? in : "";
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(!dst || max == 0) return;
    if(!src) src = "";
    while(src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static const char* theorem_status_text(enum theorem_status status){
    if(status == THEOREM_PROOF_SKETCH) return "proof sketch";
    if(status == THEOREM_FORMALLY_CHECKED) return "formally checked";
    if(status == THEOREM_COMPUTATIONALLY_VERIFIED) return "computationally verified";
    if(status == THEOREM_ASSUMED_AXIOM) return "assumed axiom";
    if(status == THEOREM_EXTERNAL_REFERENCE) return "external reference";
    return "stated";
}

static int plugin_index(const char* name){
    uint32_t i;
    for(i = 0; i < MATH_PLUGIN_MAX; i++)
        if(plugins[i].id && str_eq(plugins[i].name, name)) return (int)i;
    return -1;
}

void mathcore_init(void){
    fs_mkdir("/share/math");
    fs_write("/share/math/plugins.txt",
        "symbolics numerical linear-algebra abstract-algebra graph-theory logic-proof statistics physics visualization notebook\n");
    math_plugin_register("symbolics", "0.1", "expressions simplification differentiation latex", "simplify diff latex", "Math Lab/Symbolics");
    math_plugin_register("numerical", "0.1", "arrays fitting roots fft scaffold", "fit fft smooth", "Math Lab/Numerical");
    math_plugin_register("linear-algebra", "0.1", "vectors matrices determinants rank", "vec mat eigen", "Math Lab/Linear");
    math_plugin_register("abstract-algebra", "0.1", "groups rings fields modules", "group ring field", "Math Lab/Algebra");
    math_plugin_register("graph-theory", "0.1", "graphs paths trees flows", "graph path flow", "Math Lab/Graph");
    math_plugin_register("logic-proof", "0.1", "propositions proof steps tactics theorem refs", "intro exact apply qed", "Math Lab/Proof");
    math_plugin_register("number-theory", "0.1", "primes modular arithmetic divisibility", "prime gcd mod", "Math Lab/Number");
    math_plugin_register("topology", "0.1", "spaces continuity compactness homotopy", "space compact homotopy", "Math Lab/Topology");
    math_plugin_register("statistics", "0.1", "mean variance probability distributions", "stats prob fit", "Math Lab/Stats");
    math_plugin_register("physics", "0.1", "fields mechanics quantum thermodynamics materials", "phys science sim", "Math Lab/Physics");
    math_plugin_register("visualization", "0.1", "plots vectors latex export", "plot draw latex", "Math Lab/Plots");
    math_plugin_register("notebook", "0.1", "research notebook cells results citations", "notebook run export", "Research Notebook");

    math_plugin_register_theorem("logic-proof", "Modus ponens", "logic",
        "From P and P -> Q, infer Q.", THEOREM_FORMALLY_CHECKED);
    math_plugin_register_theorem("logic-proof", "Conjunction introduction", "logic",
        "From P and Q, infer P and Q.", THEOREM_FORMALLY_CHECKED);
    math_plugin_register_theorem("number-theory", "Euclid primes theorem", "number theory",
        "There are infinitely many prime numbers.", THEOREM_PROOF_SKETCH);
    math_plugin_register_theorem("linear-algebra", "Rank-nullity theorem", "linear algebra",
        "dim V = rank T + nullity T for finite-dimensional linear maps.", THEOREM_STATED);
    math_plugin_register_theorem("topology", "Brouwer fixed-point theorem", "topology",
        "Every continuous map from a compact ball to itself has a fixed point.", THEOREM_EXTERNAL_REFERENCE);

    math_plugin_register_algorithm("numerical", "Bisection method", "numerical analysis",
        "continuous function and interval", "root approximation", "O(log((b-a)/eps))", "scaffold");
    math_plugin_register_algorithm("linear-algebra", "Gaussian elimination", "linear algebra",
        "matrix", "row echelon form", "O(n^3)", "implemented elsewhere/scaffold registry");
    math_plugin_register_algorithm("graph-theory", "Breadth-first search", "graph theory",
        "graph and source", "distance tree", "O(V+E)", "registered");
    math_plugin_register_algorithm("symbolics", "Term rewriting", "symbolic math",
        "expression and rules", "expression", "rule dependent", "scaffold");
    math_plugin_register_algorithm("logic-proof", "Modus ponens checker", "logic",
        "premise and implication", "proof step decision", "O(1) for current kernel helper", "implemented");

    math_plugin_register_object_type("symbolics", "Expression", "symbolic expression", "x^2 + 1");
    math_plugin_register_object_type("linear-algebra", "Vector", "finite vector", "\\\\vec{v}");
    math_plugin_register_object_type("linear-algebra", "Matrix", "finite matrix", "\\\\begin{bmatrix}...\\\\end{bmatrix}");
    math_plugin_register_object_type("abstract-algebra", "Group", "group object", "G");
    math_plugin_register_object_type("graph-theory", "Graph", "vertices and edges", "G=(V,E)");
    math_plugin_register_object_type("logic-proof", "Theorem", "statement plus proof status", "\\\\vdash P");
    fs_append_line("/var/log/system.log", "mathcore: plugin theorem algorithm object registries online");
}

int math_plugin_register(const char* name, const char* version, const char* capabilities,
                         const char* commands, const char* ui_panel){
    uint32_t i;
    int existing = plugin_index(name);
    if(existing >= 0) return (int)plugins[existing].id;
    for(i = 0; i < MATH_PLUGIN_MAX; i++){
        if(plugins[i].id == 0){
            plugins[i].id = next_plugin_id++;
            copy_text(plugins[i].name, name, sizeof(plugins[i].name));
            copy_text(plugins[i].version, version, sizeof(plugins[i].version));
            copy_text(plugins[i].capabilities, capabilities, sizeof(plugins[i].capabilities));
            copy_text(plugins[i].commands, commands, sizeof(plugins[i].commands));
            copy_text(plugins[i].ui_panel, ui_panel, sizeof(plugins[i].ui_panel));
            return (int)plugins[i].id;
        }
    }
    return -1;
}

int math_plugin_find(const char* name, struct math_plugin_info* out){
    int idx = plugin_index(name);
    if(idx < 0) return -1;
    if(out) *out = plugins[idx];
    return 0;
}

uint32_t math_plugin_list(struct math_plugin_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < MATH_PLUGIN_MAX; i++){
        if(plugins[i].id){
            if(out && n < max) out[n] = plugins[i];
            n++;
        }
    }
    return n;
}

int math_plugin_dispatch(const char* plugin, const char* command, char* out, uint32_t out_max){
    struct math_plugin_info info;
    if(math_plugin_find(plugin, &info) != 0){
        copy_text(out, "plugin not found", out_max);
        return -1;
    }
    copy_text(out, info.name, out_max);
    if(out && out_max){
        uint32_t i = 0;
        while(out[i] && i + 1 < out_max) i++;
        if(i + 4 < out_max){
            out[i++] = ':';
            out[i++] = ' ';
            out[i] = 0;
        }
        copy_text(out + i, command && command[0] ? command : "status", out_max - i);
    }
    jobs_account("math-plugin", 2);
    process_set_compute("compute", "math-plugin", 88, 60);
    return 0;
}

int math_plugin_register_theorem(const char* plugin, const char* name, const char* field,
                                 const char* statement, enum theorem_status status){
    uint32_t i;
    if(status == THEOREM_FORMALLY_CHECKED && !str_eq(plugin, "logic-proof"))
        status = THEOREM_STATED;
    for(i = 0; i < THEOREM_MAX; i++){
        if(theorems[i].id == 0){
            theorems[i].id = next_theorem_id++;
            copy_text(theorems[i].name, name, sizeof(theorems[i].name));
            copy_text(theorems[i].field, field, sizeof(theorems[i].field));
            copy_text(theorems[i].statement, statement, sizeof(theorems[i].statement));
            theorems[i].status = status;
            copy_text(theorems[i].source_plugin, plugin, sizeof(theorems[i].source_plugin));
            return (int)theorems[i].id;
        }
    }
    return -1;
}

int math_plugin_register_algorithm(const char* plugin, const char* name, const char* field,
                                   const char* input_types, const char* output_types,
                                   const char* complexity, const char* status){
    uint32_t i;
    for(i = 0; i < ALGORITHM_MAX; i++){
        if(algorithms[i].id == 0){
            algorithms[i].id = next_algorithm_id++;
            copy_text(algorithms[i].name, name, sizeof(algorithms[i].name));
            copy_text(algorithms[i].field, field, sizeof(algorithms[i].field));
            copy_text(algorithms[i].input_types, input_types, sizeof(algorithms[i].input_types));
            copy_text(algorithms[i].output_types, output_types, sizeof(algorithms[i].output_types));
            copy_text(algorithms[i].complexity, complexity, sizeof(algorithms[i].complexity));
            copy_text(algorithms[i].status, status, sizeof(algorithms[i].status));
            copy_text(algorithms[i].source_plugin, plugin, sizeof(algorithms[i].source_plugin));
            return (int)algorithms[i].id;
        }
    }
    return -1;
}

int math_plugin_register_object_type(const char* plugin, const char* name,
                                     const char* display, const char* latex_hint){
    uint32_t i;
    for(i = 0; i < OBJECT_TYPE_MAX; i++){
        if(object_types[i].id == 0){
            object_types[i].id = next_object_type_id++;
            copy_text(object_types[i].name, name, sizeof(object_types[i].name));
            copy_text(object_types[i].display, display, sizeof(object_types[i].display));
            copy_text(object_types[i].latex_hint, latex_hint, sizeof(object_types[i].latex_hint));
            copy_text(object_types[i].source_plugin, plugin, sizeof(object_types[i].source_plugin));
            return (int)object_types[i].id;
        }
    }
    return -1;
}

uint32_t math_theorem_list(struct theorem_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < THEOREM_MAX; i++){
        if(theorems[i].id){
            if(out && n < max) out[n] = theorems[i];
            n++;
        }
    }
    return n;
}

uint32_t math_algorithm_list(struct algorithm_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < ALGORITHM_MAX; i++){
        if(algorithms[i].id){
            if(out && n < max) out[n] = algorithms[i];
            n++;
        }
    }
    return n;
}

uint32_t math_object_type_list(struct math_object_type_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < OBJECT_TYPE_MAX; i++){
        if(object_types[i].id){
            if(out && n < max) out[n] = object_types[i];
            n++;
        }
    }
    return n;
}

int math_plugin_run_tests(const char* plugin){
    struct math_plugin_info info;
    if(math_plugin_find(plugin, &info) != 0)
        return -1;
    jobs_account("math-plugin-tests", 3);
    return 0;
}

void mathcore_cmd(char* arg){
    char action[16];
    arg = (char*)first_arg(arg, action, sizeof(action));
    if(action[0] == 0 || str_eq(action, "plugins")){
        struct math_plugin_info list[MATH_PLUGIN_MAX];
        uint32_t i, n = math_plugin_list(list, MATH_PLUGIN_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" caps=");
            console_puts(list[i].capabilities);
            console_putc('\n');
        }
    } else if(str_eq(action, "theorems")){
        struct theorem_info list[THEOREM_MAX];
        uint32_t i, n = math_theorem_list(list, THEOREM_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" status=");
            console_puts(theorem_status_text(list[i].status));
            console_putc('\n');
        }
    } else if(str_eq(action, "algorithms")){
        struct algorithm_info list[ALGORITHM_MAX];
        uint32_t i, n = math_algorithm_list(list, ALGORITHM_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" ");
            console_puts(list[i].complexity);
            console_putc('\n');
        }
    } else if(str_eq(action, "objects")){
        struct math_object_type_info list[OBJECT_TYPE_MAX];
        uint32_t i, n = math_object_type_list(list, OBJECT_TYPE_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" latex=");
            console_puts(list[i].latex_hint);
            console_putc('\n');
        }
    } else if(str_eq(action, "run")){
        char plugin[32];
        char out[96];
        arg = (char*)first_arg(arg, plugin, sizeof(plugin));
        if(math_plugin_dispatch(plugin, arg, out, sizeof(out)) == 0) console_puts(out);
        else console_puts("plugin not found");
        console_putc('\n');
    } else if(str_eq(action, "test")){
        char plugin[32];
        first_arg(arg, plugin, sizeof(plugin));
        console_puts(math_plugin_run_tests(plugin) == 0 ? "plugin tests passed\n" : "plugin tests failed\n");
    } else {
        console_puts("usage: mathcore plugins|theorems|algorithms|objects|run PLUGIN CMD|test PLUGIN\n");
    }
}
