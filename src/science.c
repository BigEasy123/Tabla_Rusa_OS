#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "process.h"
#include "science.h"

#define SCIENCE_SCALE 1000
#define SCIENCE_ARRAY_MAX 8
#define SCIENCE_PEAK_MAX 12
#define SCIENCE_CRYSTAL_MAX 6
#define SCIENCE_SIM_MAX 8
#define SCIENCE_QUANTUM_MAX 8
#define SCIENCE_MATERIAL_MAX 8
#define SCIENCE_BAND_MAX 12
#define SCIENCE_PHONON_MAX 12

struct science_array_slot {
    int used;
    struct science_array_info info;
};

static struct science_array_slot arrays[SCIENCE_ARRAY_MAX];
static struct spectroscopy_peak_info peaks[SCIENCE_PEAK_MAX];
static struct crystal_lattice_info crystals[SCIENCE_CRYSTAL_MAX];
static struct simulation_job_info sim_jobs[SCIENCE_SIM_MAX];
static struct quantum_state_info quantum_states[SCIENCE_QUANTUM_MAX];
static struct material_info materials[SCIENCE_MATERIAL_MAX];
static struct band_point_info band_points[SCIENCE_BAND_MAX];
static struct phonon_mode_info phonon_modes[SCIENCE_PHONON_MAX];
static uint32_t next_array_id = 1;
static uint32_t next_peak_id = 1;
static uint32_t next_crystal_id = 1;
static uint32_t next_sim_id = 1;
static uint32_t next_quantum_id = 1;
static uint32_t next_material_id = 1;
static uint32_t next_band_id = 1;
static uint32_t next_phonon_id = 1;

struct unit_def {
    const char* name;
    const char* dimension;
    int32_t to_base_scaled;
};

struct constant_def {
    const char* name;
    int32_t value_scaled;
    const char* unit;
};

static const struct unit_def units[] = {
    {"m", "length", 1000}, {"cm", "length", 10}, {"mm", "length", 1}, {"km", "length", 1000000},
    {"s", "time", 1000}, {"min", "time", 60000}, {"h", "time", 3600000},
    {"g", "mass", 1}, {"kg", "mass", 1000},
    {"j", "energy", 1000}, {"kj", "energy", 1000000},
    {"hz", "frequency", 1000}, {"khz", "frequency", 1000000},
    {0, 0, 0}
};

static const struct constant_def constants[] = {
    {"c", 299792458, "m/s"},
    {"g", 9807, "m/s^2"},
    {"h", 663, "1e-36 J*s"},
    {"k_b", 1381, "1e-26 J/K"},
    {"e", 1602, "1e-22 C"},
    {"epsilon0", 8854, "1e-15 F/m"},
    {"mu0", 1257, "1e-9 N/A^2"},
    {0, 0, 0}
};

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

static int32_t parse_i32(const char* s){
    int sign = 1;
    int32_t value = 0;
    while(s && is_space(*s)) s++;
    if(s && *s == '-'){
        sign = -1;
        s++;
    }
    while(s && *s >= '0' && *s <= '9'){
        value = value * 10 + (int32_t)(*s - '0');
        s++;
    }
    return value * sign;
}

static uint32_t parse_values(const char* s, int32_t* out, uint32_t max){
    char token[16];
    uint32_t n = 0;
    while(s && *s && n < max){
        s = first_arg(s, token, sizeof(token));
        if(!token[0]) break;
        out[n++] = parse_i32(token);
    }
    return n;
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

static void print_scaled(int32_t value){
    if(value < 0){
        console_putc('-');
        value = -value;
    }
    console_write_dec((uint32_t)(value / SCIENCE_SCALE));
    console_putc('.');
    uint32_t frac = (uint32_t)(value % SCIENCE_SCALE);
    if(frac < 100) console_putc('0');
    if(frac < 10) console_putc('0');
    console_write_dec(frac);
}

static const struct unit_def* unit_find(const char* name){
    uint32_t i;
    for(i = 0; units[i].name; i++)
        if(str_eq(units[i].name, name)) return &units[i];
    return 0;
}

void science_init(void){
    fs_mkdir("/science");
    fs_mkdir("/science/sim");
    fs_mkdir("/science/materials");
    fs_mkdir("/science/quantum");
    fs_write("/science/constants.txt",
        "c speed of light\n"
        "g earth gravity\n"
        "h planck scaled\n"
        "k_b boltzmann scaled\n"
        "e elementary charge scaled\n"
        "epsilon0 vacuum permittivity scaled\n"
        "mu0 vacuum permeability scaled\n");
    fs_write("/science/domains.txt",
        "raman ir photoconductivity crystallography solid-state quantum statmech electromagnetism thermodynamics materials\n");
    spectroscopy_peak_add("raman", 520000, 900, "silicon optical phonon reference");
    crystal_create_lattice("silicon", "cubic", 5431, 5431, 5431, 90000, 90000, 90000);
    int silicon = material_register("silicon", "Si", "solid", 1120);
    quantum_state_create("particle-box", "n=1", 1000, "first scaffold quantum state");
    if(silicon > 0){
        band_point_add((uint32_t)silicon, "G", 0, 0);
        band_point_add((uint32_t)silicon, "X", 1000, 1120);
        phonon_mode_add((uint32_t)silicon, "T2g", 520000, "raman");
    }
    fs_append_line("/var/log/system.log", "science: units constants arrays fitting signal spectroscopy crystals simulations online");
}

int32_t unit_convert(int32_t value_scaled, const char* from, const char* to){
    const struct unit_def* a = unit_find(from);
    const struct unit_def* b = unit_find(to);
    if(!a || !b || !str_eq(a->dimension, b->dimension) || b->to_base_scaled == 0)
        return 0;
    return (value_scaled * a->to_base_scaled) / b->to_base_scaled;
}

int unit_check_dimension(const char* left, const char* right){
    const struct unit_def* a = unit_find(left);
    const struct unit_def* b = unit_find(right);
    return a && b && str_eq(a->dimension, b->dimension);
}

int constants_find(const char* name, int32_t* value_scaled, char* unit, uint32_t unit_max){
    uint32_t i;
    for(i = 0; constants[i].name; i++){
        if(str_eq(constants[i].name, name)){
            if(value_scaled) *value_scaled = constants[i].value_scaled;
            copy_text(unit, constants[i].unit, unit_max);
            return 0;
        }
    }
    return -1;
}

int array_create(const int32_t* values, uint32_t len){
    uint32_t i, j;
    if(!values || len == 0 || len > SCIENCE_ARRAY_MAX_LEN) return -1;
    for(i = 0; i < SCIENCE_ARRAY_MAX; i++){
        if(!arrays[i].used){
            arrays[i].used = 1;
            arrays[i].info.id = next_array_id++;
            arrays[i].info.len = len;
            for(j = 0; j < len; j++) arrays[i].info.values[j] = values[j];
            jobs_account("science-array", 1);
            return (int)arrays[i].info.id;
        }
    }
    return -1;
}

int array_free(uint32_t id){
    uint32_t i;
    for(i = 0; i < SCIENCE_ARRAY_MAX; i++){
        if(arrays[i].used && arrays[i].info.id == id){
            arrays[i].used = 0;
            return 0;
        }
    }
    return -1;
}

int array_get(uint32_t id, struct science_array_info* out){
    uint32_t i;
    for(i = 0; i < SCIENCE_ARRAY_MAX; i++){
        if(arrays[i].used && arrays[i].info.id == id){
            if(out) *out = arrays[i].info;
            return 0;
        }
    }
    return -1;
}

int fit_linear(const int32_t* x, const int32_t* y, uint32_t len, struct science_fit_result* out){
    int32_t sx = 0, sy = 0, sxx = 0, sxy = 0;
    int32_t denom;
    uint32_t i;
    if(!x || !y || !out || len < 2) return -1;
    for(i = 0; i < len; i++){
        sx += x[i];
        sy += y[i];
        sxx += x[i] * x[i];
        sxy += x[i] * y[i];
    }
    denom = (int32_t)len * sxx - sx * sx;
    if(denom == 0) return -1;
    out->a_scaled = (((int32_t)len * sxy - sx * sy) * SCIENCE_SCALE) / denom;
    out->b_scaled = ((sy * SCIENCE_SCALE) - out->a_scaled * sx) / (int32_t)len;
    out->error_scaled = 0;
    jobs_account("science-fit", 3);
    process_set_compute("compute", "science-fit", 94, 80);
    return 0;
}

int fit_polynomial(const int32_t* y, uint32_t len, struct science_fit_result* out){
    int32_t x[SCIENCE_ARRAY_MAX_LEN];
    uint32_t i;
    if(!y || len > SCIENCE_ARRAY_MAX_LEN) return -1;
    for(i = 0; i < len; i++) x[i] = (int32_t)i;
    return fit_linear(x, y, len, out);
}

int fit_exponential(const int32_t* y, uint32_t len, struct science_fit_result* out){
    if(!y || !out || len < 2 || y[0] == 0) return -1;
    out->a_scaled = y[0] * SCIENCE_SCALE;
    out->b_scaled = ((y[len - 1] - y[0]) * SCIENCE_SCALE) / (int32_t)(len - 1);
    out->error_scaled = 0;
    jobs_account("science-expfit", 3);
    process_set_compute("compute", "science-expfit", 92, 70);
    return 0;
}

uint32_t signal_smooth(const int32_t* in, uint32_t len, int32_t* out, uint32_t max){
    uint32_t i, n = len < max ? len : max;
    if(!in || !out) return 0;
    for(i = 0; i < n; i++){
        int32_t total = in[i];
        int32_t count = 1;
        if(i > 0){ total += in[i - 1]; count++; }
        if(i + 1 < len){ total += in[i + 1]; count++; }
        out[i] = total / count;
    }
    jobs_account("science-signal", 2);
    return n;
}

uint32_t signal_fft(const int32_t* in, uint32_t len, int32_t* out, uint32_t max){
    uint32_t i;
    int32_t dc = 0;
    int32_t alternating = 0;
    if(!in || !out || max == 0) return 0;
    for(i = 0; i < len; i++){
        dc += in[i];
        alternating += (i & 1) ? -in[i] : in[i];
    }
    out[0] = len ? dc / (int32_t)len : 0;
    if(max > 1) out[1] = alternating;
    jobs_account("science-fft", 4);
    return max > 1 ? 2 : 1;
}

int spectroscopy_peak_add(const char* domain, int32_t position_scaled, int32_t intensity_scaled, const char* assignment){
    uint32_t i;
    for(i = 0; i < SCIENCE_PEAK_MAX; i++){
        if(peaks[i].id == 0){
            peaks[i].id = next_peak_id++;
            copy_text(peaks[i].domain, domain, sizeof(peaks[i].domain));
            peaks[i].position_scaled = position_scaled;
            peaks[i].intensity_scaled = intensity_scaled;
            copy_text(peaks[i].assignment, assignment, sizeof(peaks[i].assignment));
            return (int)peaks[i].id;
        }
    }
    return -1;
}

int spectroscopy_peak_fit(uint32_t peak_id, struct spectroscopy_peak_info* out){
    uint32_t i;
    for(i = 0; i < SCIENCE_PEAK_MAX; i++){
        if(peaks[i].id == peak_id){
            if(out) *out = peaks[i];
            jobs_account("science-spectroscopy", 4);
            return 0;
        }
    }
    return -1;
}

uint32_t spectroscopy_peak_list(struct spectroscopy_peak_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < SCIENCE_PEAK_MAX; i++){
        if(peaks[i].id){
            if(out && n < max) out[n] = peaks[i];
            n++;
        }
    }
    return n;
}

int crystal_create_lattice(const char* name, const char* system,
                           int32_t a_scaled, int32_t b_scaled, int32_t c_scaled,
                           int32_t alpha_scaled, int32_t beta_scaled, int32_t gamma_scaled){
    uint32_t i;
    for(i = 0; i < SCIENCE_CRYSTAL_MAX; i++){
        if(crystals[i].id == 0){
            crystals[i].id = next_crystal_id++;
            copy_text(crystals[i].name, name, sizeof(crystals[i].name));
            copy_text(crystals[i].system, system, sizeof(crystals[i].system));
            crystals[i].a_scaled = a_scaled;
            crystals[i].b_scaled = b_scaled;
            crystals[i].c_scaled = c_scaled;
            crystals[i].alpha_scaled = alpha_scaled;
            crystals[i].beta_scaled = beta_scaled;
            crystals[i].gamma_scaled = gamma_scaled;
            jobs_account("science-crystal", 2);
            return (int)crystals[i].id;
        }
    }
    return -1;
}

int crystal_lattice_get(uint32_t id, struct crystal_lattice_info* out){
    uint32_t i;
    for(i = 0; i < SCIENCE_CRYSTAL_MAX; i++){
        if(crystals[i].id == id){
            if(out) *out = crystals[i];
            return 0;
        }
    }
    return -1;
}

int simulation_job_create(const char* domain, const char* name, uint32_t priority){
    uint32_t i;
    for(i = 0; i < SCIENCE_SIM_MAX; i++){
        if(sim_jobs[i].id == 0){
            sim_jobs[i].id = next_sim_id++;
            copy_text(sim_jobs[i].domain, domain, sizeof(sim_jobs[i].domain));
            copy_text(sim_jobs[i].name, name, sizeof(sim_jobs[i].name));
            copy_text(sim_jobs[i].status, "queued", sizeof(sim_jobs[i].status));
            sim_jobs[i].priority = priority ? priority : 70;
            sim_jobs[i].ticks = 0;
            copy_text(sim_jobs[i].summary, "waiting for simulation run", sizeof(sim_jobs[i].summary));
            jobs_account("science-sim", 1);
            return (int)sim_jobs[i].id;
        }
    }
    return -1;
}

int simulation_job_run(uint32_t id){
    uint32_t i;
    for(i = 0; i < SCIENCE_SIM_MAX; i++){
        if(sim_jobs[i].id == id){
            copy_text(sim_jobs[i].status, "complete", sizeof(sim_jobs[i].status));
            sim_jobs[i].ticks += 16;
            copy_text(sim_jobs[i].summary, "scaffold run complete; results archived in job table", sizeof(sim_jobs[i].summary));
            jobs_account("science-sim", 16);
            process_set_compute("compute", sim_jobs[i].domain, sim_jobs[i].priority, 90);
            process_tick("compute", 16);
            return 0;
        }
    }
    return -1;
}

int simulation_job_status(uint32_t id, struct simulation_job_info* out){
    uint32_t i;
    for(i = 0; i < SCIENCE_SIM_MAX; i++){
        if(sim_jobs[i].id == id){
            if(out) *out = sim_jobs[i];
            return 0;
        }
    }
    return -1;
}

uint32_t simulation_job_list(struct simulation_job_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < SCIENCE_SIM_MAX; i++){
        if(sim_jobs[i].id){
            if(out && n < max) out[n] = sim_jobs[i];
            n++;
        }
    }
    return n;
}

int quantum_state_create(const char* name, const char* basis, int32_t energy_scaled, const char* note){
    uint32_t i;
    for(i = 0; i < SCIENCE_QUANTUM_MAX; i++){
        if(quantum_states[i].id == 0){
            quantum_states[i].id = next_quantum_id++;
            copy_text(quantum_states[i].name, name, sizeof(quantum_states[i].name));
            copy_text(quantum_states[i].basis, basis, sizeof(quantum_states[i].basis));
            quantum_states[i].energy_scaled = energy_scaled;
            copy_text(quantum_states[i].note, note, sizeof(quantum_states[i].note));
            jobs_account("science-quantum", 3);
            process_set_compute("compute", "science-quantum", 91, 70);
            return (int)quantum_states[i].id;
        }
    }
    return -1;
}

int quantum_state_get(uint32_t id, struct quantum_state_info* out){
    uint32_t i;
    for(i = 0; i < SCIENCE_QUANTUM_MAX; i++){
        if(quantum_states[i].id == id){
            if(out) *out = quantum_states[i];
            return 0;
        }
    }
    return -1;
}

uint32_t quantum_state_list(struct quantum_state_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < SCIENCE_QUANTUM_MAX; i++){
        if(quantum_states[i].id){
            if(out && n < max) out[n] = quantum_states[i];
            n++;
        }
    }
    return n;
}

int material_register(const char* name, const char* formula, const char* phase, int32_t band_gap_scaled){
    uint32_t i;
    for(i = 0; i < SCIENCE_MATERIAL_MAX; i++){
        if(materials[i].id == 0){
            materials[i].id = next_material_id++;
            copy_text(materials[i].name, name, sizeof(materials[i].name));
            copy_text(materials[i].formula, formula, sizeof(materials[i].formula));
            copy_text(materials[i].phase, phase, sizeof(materials[i].phase));
            materials[i].band_gap_scaled = band_gap_scaled;
            jobs_account("science-materials", 2);
            return (int)materials[i].id;
        }
    }
    return -1;
}

int material_get(uint32_t id, struct material_info* out){
    uint32_t i;
    for(i = 0; i < SCIENCE_MATERIAL_MAX; i++){
        if(materials[i].id == id){
            if(out) *out = materials[i];
            return 0;
        }
    }
    return -1;
}

uint32_t material_list(struct material_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < SCIENCE_MATERIAL_MAX; i++){
        if(materials[i].id){
            if(out && n < max) out[n] = materials[i];
            n++;
        }
    }
    return n;
}

int band_point_add(uint32_t material_id, const char* label, int32_t k_scaled, int32_t energy_scaled){
    uint32_t i;
    if(material_get(material_id, 0) != 0) return -1;
    for(i = 0; i < SCIENCE_BAND_MAX; i++){
        if(band_points[i].id == 0){
            band_points[i].id = next_band_id++;
            band_points[i].material_id = material_id;
            copy_text(band_points[i].label, label, sizeof(band_points[i].label));
            band_points[i].k_scaled = k_scaled;
            band_points[i].energy_scaled = energy_scaled;
            jobs_account("science-band", 2);
            return (int)band_points[i].id;
        }
    }
    return -1;
}

uint32_t band_point_list(uint32_t material_id, struct band_point_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < SCIENCE_BAND_MAX; i++){
        if(band_points[i].id && (!material_id || band_points[i].material_id == material_id)){
            if(out && n < max) out[n] = band_points[i];
            n++;
        }
    }
    return n;
}

int phonon_mode_add(uint32_t material_id, const char* symmetry, int32_t frequency_scaled, const char* activity){
    uint32_t i;
    if(material_get(material_id, 0) != 0) return -1;
    for(i = 0; i < SCIENCE_PHONON_MAX; i++){
        if(phonon_modes[i].id == 0){
            phonon_modes[i].id = next_phonon_id++;
            phonon_modes[i].material_id = material_id;
            copy_text(phonon_modes[i].symmetry, symmetry, sizeof(phonon_modes[i].symmetry));
            phonon_modes[i].frequency_scaled = frequency_scaled;
            copy_text(phonon_modes[i].activity, activity, sizeof(phonon_modes[i].activity));
            jobs_account("science-phonon", 2);
            return (int)phonon_modes[i].id;
        }
    }
    return -1;
}

uint32_t phonon_mode_list(uint32_t material_id, struct phonon_mode_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < SCIENCE_PHONON_MAX; i++){
        if(phonon_modes[i].id && (!material_id || phonon_modes[i].material_id == material_id)){
            if(out && n < max) out[n] = phonon_modes[i];
            n++;
        }
    }
    return n;
}

void science_cmd(char* arg){
    char action[16];
    arg = (char*)first_arg(arg, action, sizeof(action));
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("science: units constants arrays fit signal spectroscopy crystals simulation quantum materials bands phonons\n");
        console_puts("domains: raman ir photoconductivity crystallography solid-state quantum statmech em thermo materials\n");
    } else if(str_eq(action, "unit")){
        char value[16], from[16], to[16];
        int32_t out;
        arg = (char*)first_arg(arg, value, sizeof(value));
        arg = (char*)first_arg(arg, from, sizeof(from));
        first_arg(arg, to, sizeof(to));
        if(!value[0] || !from[0] || !to[0]){
            console_puts("usage: science unit VALUE FROM TO\n");
            return;
        }
        out = unit_convert(parse_i32(value) * SCIENCE_SCALE, from, to);
        print_scaled(out);
        console_putc('\n');
    } else if(str_eq(action, "constant")){
        char name[24], unit[24];
        int32_t value;
        first_arg(arg, name, sizeof(name));
        if(constants_find(name, &value, unit, sizeof(unit)) != 0) console_puts("constant: not found\n");
        else {
            console_puts(name);
            console_puts("=");
            print_scaled(value);
            console_puts(" ");
            console_puts(unit);
            console_putc('\n');
        }
    } else if(str_eq(action, "smooth")){
        int32_t values[SCIENCE_ARRAY_MAX_LEN], out[SCIENCE_ARRAY_MAX_LEN];
        uint32_t i, n = parse_values(arg, values, SCIENCE_ARRAY_MAX_LEN);
        uint32_t m = signal_smooth(values, n, out, SCIENCE_ARRAY_MAX_LEN);
        for(i = 0; i < m; i++){
            console_write_dec((uint32_t)out[i]);
            console_putc(i + 1 < m ? ' ' : '\n');
        }
    } else if(str_eq(action, "fft")){
        int32_t values[SCIENCE_ARRAY_MAX_LEN], out[4];
        uint32_t i, n = parse_values(arg, values, SCIENCE_ARRAY_MAX_LEN);
        uint32_t m = signal_fft(values, n, out, 4);
        for(i = 0; i < m; i++){
            console_write_dec((uint32_t)out[i]);
            console_putc(i + 1 < m ? ' ' : '\n');
        }
    } else if(str_eq(action, "peaks")){
        struct spectroscopy_peak_info list[SCIENCE_PEAK_MAX];
        uint32_t i, n = spectroscopy_peak_list(list, SCIENCE_PEAK_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].domain);
            console_puts(" ");
            print_scaled(list[i].position_scaled);
            console_puts(" ");
            console_puts(list[i].assignment);
            console_putc('\n');
        }
    } else if(str_eq(action, "sim")){
        char sub[16], domain[24], name[32];
        arg = (char*)first_arg(arg, sub, sizeof(sub));
        if(str_eq(sub, "new")){
            int id;
            arg = (char*)first_arg(arg, domain, sizeof(domain));
            first_arg(arg, name, sizeof(name));
            id = simulation_job_create(domain, name, 90);
            console_puts("simulation job ");
            console_write_dec((uint32_t)id);
            console_putc('\n');
        } else if(str_eq(sub, "run")){
            uint32_t id = (uint32_t)parse_i32(arg);
            console_puts(simulation_job_run(id) == 0 ? "simulation complete\n" : "simulation not found\n");
        } else {
            struct simulation_job_info list[SCIENCE_SIM_MAX];
            uint32_t i, n = simulation_job_list(list, SCIENCE_SIM_MAX);
            for(i = 0; i < n; i++){
                console_write_dec(list[i].id);
                console_puts(" ");
                console_puts(list[i].domain);
                console_puts(" ");
                console_puts(list[i].status);
                console_putc('\n');
            }
        }
    } else if(str_eq(action, "quantum")){
        struct quantum_state_info list[SCIENCE_QUANTUM_MAX];
        uint32_t i, n = quantum_state_list(list, SCIENCE_QUANTUM_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" basis=");
            console_puts(list[i].basis);
            console_puts(" E=");
            print_scaled(list[i].energy_scaled);
            console_putc('\n');
        }
    } else if(str_eq(action, "materials")){
        struct material_info list[SCIENCE_MATERIAL_MAX];
        uint32_t i, n = material_list(list, SCIENCE_MATERIAL_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" ");
            console_puts(list[i].formula);
            console_puts(" gap=");
            print_scaled(list[i].band_gap_scaled);
            console_putc('\n');
        }
    } else if(str_eq(action, "bands")){
        struct band_point_info list[SCIENCE_BAND_MAX];
        uint32_t i, n = band_point_list(0, list, SCIENCE_BAND_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].material_id);
            console_puts(" ");
            console_puts(list[i].label);
            console_puts(" k=");
            print_scaled(list[i].k_scaled);
            console_puts(" E=");
            print_scaled(list[i].energy_scaled);
            console_putc('\n');
        }
    } else if(str_eq(action, "phonons")){
        struct phonon_mode_info list[SCIENCE_PHONON_MAX];
        uint32_t i, n = phonon_mode_list(0, list, SCIENCE_PHONON_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].material_id);
            console_puts(" ");
            console_puts(list[i].symmetry);
            console_puts(" ");
            print_scaled(list[i].frequency_scaled);
            console_puts(" ");
            console_puts(list[i].activity);
            console_putc('\n');
        }
    } else {
        console_puts("usage: science status|unit|constant|smooth|fft|peaks|sim|quantum|materials|bands|phonons\n");
    }
}
