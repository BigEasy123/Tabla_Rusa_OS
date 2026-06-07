# Science And Physics Engine

`science.c` / `science.h` provide the first reusable scientific-computing subsystem outside the older `mathlib.c` command surface. The goal is to give Rusa, Math Lab, notebooks, and simulation jobs stable APIs for first-principles and materials-research workflows.

## API Surface

```c
science_init();
unit_convert(2 * 1000, "m", "cm");
unit_check_dimension("m", "km");
constants_find("c", &value, unit, unit_max);
array_create(values, len);
array_get(array_id, &info);
array_free(array_id);
fit_linear(x, y, len, &result);
fit_polynomial(y, len, &result);
fit_exponential(y, len, &result);
signal_smooth(in, len, out, max);
signal_fft(in, len, out, max);
spectroscopy_peak_add("raman", 520000, 900, "silicon optical phonon");
spectroscopy_peak_fit(peak_id, &peak);
crystal_create_lattice("silicon", "cubic", 5431, 5431, 5431, 90000, 90000, 90000);
simulation_job_create("raman", "peak-fit", 91);
simulation_job_run(job_id);
simulation_job_status(job_id, &job);
```

Values are fixed-point where useful, generally scaled by 1000. This avoids depending on a floating-point runtime inside the freestanding kernel.

## Current Domains

- Units and dimensional checks.
- Physical constants table.
- Small numerical arrays.
- Linear, polynomial, and exponential fitting scaffolds.
- Smoothing and FFT scaffold outputs.
- Raman/IR spectroscopy peak records.
- Crystal lattice records.
- Simulation job records tied into process/job accounting.

## Terminal Commands

```text
science status
science unit 2 m cm
science constant c
science smooth 1 2 3 4
science fft 1 2 3 4
science peaks
science sim new raman peak-fit
science sim run 1
science sim
```

## Integration

Science simulation runs account work to the shared job table and update the `compute` process workload. This is the first step toward making physics/materials jobs visible in Task Manager and schedulable alongside GUI and Rusa workloads.

## Remaining Work

- Real FFT, nonlinear fitting, and uncertainty propagation.
- Real units algebra for compound units such as `m/s^2`.
- ODE/PDE solver scaffolds.
- Quantum, thermodynamics, solid-state, crystallography, Raman/IR analysis objects.
- Plot/export hooks and notebook cell execution.
