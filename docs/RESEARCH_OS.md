# Research OS Framework

Tabla Rusa OS now has a small modular research framework in `research.c`/`research.h`. It is intentionally infrastructure-first: GUI apps, Rusa packages, and math/physics tools can call the same APIs later instead of inventing separate project metadata stores.

## API

```c
research_init();
research_project_create("paper", "description", "tag1,tag2");
research_project_open("paper");
research_project_save(project_id);
research_add_note(project_id, RESEARCH_CELL_RUSA, "model", "let x = 1");
research_register_dataset(project_id, "samples", "/research/datasets/samples.csv", "x:int,y:int");
research_track_experiment(project_id, "trial-a", "math vector dot");
research_record_result(project_id, "trial-a", "result summary");
research_project_list(out, max);
research_cell_list(project_id, out, max);
research_dataset_list(project_id, out, max);
research_experiment_list(project_id, out, max);
```

## Model

Projects track:

- name, description, tags
- notebook/result cell counts
- dataset count
- experiment count
- modified tick

Notebook cell types currently include Markdown, Math, Code, Rusa, Proof, Dataset, Result, and Citation. Datasets record a path and schema string. Experiments record a command and status.

## Filesystem Mirror

`research_init()` creates:

```text
/research
/research/datasets
/research/results
```

Saving a project writes a Markdown summary like:

```text
/research/project-1.md
```

This is not a full notebook file format yet. It is a stable manifest slice so the editor, file manager, and Rusa imports can discover research projects later.

## Terminal Commands

```text
research list
research new NAME DESCRIPTION
research note PROJECT TITLE BODY
research dataset PROJECT NAME PATH SCHEMA
research experiment PROJECT NAME COMMAND
research save PROJECT
```

## Remaining Work

- Notebook cell execution and output capture.
- Citation database and bibliography export.
- Figure registry and plot artifacts.
- Reproducibility metadata for process/job inputs.
- GUI Research app and Rusa `std.notebook` bindings.
