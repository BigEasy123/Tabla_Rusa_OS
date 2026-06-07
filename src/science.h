#ifndef SCIENCE_H
#define SCIENCE_H

#include <stdint.h>

#define SCIENCE_ARRAY_MAX_LEN 16

struct science_array_info {
    uint32_t id;
    uint32_t len;
    int32_t values[SCIENCE_ARRAY_MAX_LEN];
};

struct science_fit_result {
    int32_t a_scaled;
    int32_t b_scaled;
    int32_t error_scaled;
};

struct spectroscopy_peak_info {
    uint32_t id;
    char domain[16];
    int32_t position_scaled;
    int32_t intensity_scaled;
    char assignment[48];
};

struct crystal_lattice_info {
    uint32_t id;
    char name[24];
    char system[24];
    int32_t a_scaled;
    int32_t b_scaled;
    int32_t c_scaled;
    int32_t alpha_scaled;
    int32_t beta_scaled;
    int32_t gamma_scaled;
};

struct simulation_job_info {
    uint32_t id;
    char domain[24];
    char name[32];
    char status[24];
    uint32_t priority;
    uint32_t ticks;
    char summary[96];
};

void science_init(void);
int32_t unit_convert(int32_t value_scaled, const char* from, const char* to);
int unit_check_dimension(const char* left, const char* right);
int constants_find(const char* name, int32_t* value_scaled, char* unit, uint32_t unit_max);
int array_create(const int32_t* values, uint32_t len);
int array_free(uint32_t id);
int array_get(uint32_t id, struct science_array_info* out);
int fit_linear(const int32_t* x, const int32_t* y, uint32_t len, struct science_fit_result* out);
int fit_polynomial(const int32_t* y, uint32_t len, struct science_fit_result* out);
int fit_exponential(const int32_t* y, uint32_t len, struct science_fit_result* out);
uint32_t signal_smooth(const int32_t* in, uint32_t len, int32_t* out, uint32_t max);
uint32_t signal_fft(const int32_t* in, uint32_t len, int32_t* out, uint32_t max);
int spectroscopy_peak_add(const char* domain, int32_t position_scaled, int32_t intensity_scaled, const char* assignment);
int spectroscopy_peak_fit(uint32_t peak_id, struct spectroscopy_peak_info* out);
uint32_t spectroscopy_peak_list(struct spectroscopy_peak_info* out, uint32_t max);
int crystal_create_lattice(const char* name, const char* system,
                           int32_t a_scaled, int32_t b_scaled, int32_t c_scaled,
                           int32_t alpha_scaled, int32_t beta_scaled, int32_t gamma_scaled);
int crystal_lattice_get(uint32_t id, struct crystal_lattice_info* out);
int simulation_job_create(const char* domain, const char* name, uint32_t priority);
int simulation_job_run(uint32_t id);
int simulation_job_status(uint32_t id, struct simulation_job_info* out);
uint32_t simulation_job_list(struct simulation_job_info* out, uint32_t max);
void science_cmd(char* arg);

#endif
