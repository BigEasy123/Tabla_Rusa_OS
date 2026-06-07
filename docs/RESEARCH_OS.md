# Research OS Framework

Tabla Rusa OS now has a small modular research framework in `research.c`/`research.h`. It is intentionally infrastructure-first: GUI apps, Rusa packages, and math/physics tools can call the same APIs later instead of inventing separate project metadata stores.

## API

```c
research_init();
research_project_create("paper", "description", "tag1,tag2");
research_project_open("paper");
research_project_save(project_id);
research_add_note(project_id, RESEARCH_CELL_RUSA, "model", "let x = 1");
research_add_dataset(project_id, "samples", "/research/datasets/samples.csv", "x:int,y:int");
research_register_dataset(project_id, "samples", "/research/datasets/samples.csv", "x:int,y:int");
research_add_citation(project_id, "smith2026", "Paper Title", "Journal or local source");
research_track_experiment(project_id, "trial-a", "math vector dot");
research_add_result(project_id, "trial-a", "result summary");
research_record_result(project_id, "trial-a", "result summary");
research_add_task(project_id, "write-methods", "benji");
research_update_task(task_id, "done");
research_add_timeline(project_id, "milestone", "first draft");
research_add_relation(project_id, "Rusa", "supports", "notebooks");
research_export_latex(project_id, out_path, out_max);
notebook_create(project_id, "lab-notebook");
notebook_add_cell(notebook_id, RESEARCH_CELL_MATH, "fit", "science fit");
notebook_run_cell(notebook_id, cell_id, out, out_max);
notebook_export(notebook_id, out_path, out_max);
research_project_list(out, max);
research_cell_list(project_id, out, max);
research_dataset_list(project_id, out, max);
research_experiment_list(project_id, out, max);
research_citation_list(project_id, out, max);
research_task_list(project_id, out, max);
research_timeline_list(project_id, out, max);
research_relation_list(project_id, out, max);
notebook_list(project_id, out, max);
```

## Model

Projects track:

- name, description, tags
- notebook/result cell counts
- dataset count
- experiment count
- modified tick

Notebook cell types currently include Markdown, Math, Code, Rusa, Proof, Dataset, Result, and Citation. Datasets record a path and schema string. Experiments record a command and status. Tasks track owner/status, timeline entries track tick-stamped logbook notes, and graph relations track simple subject/relation/object triples.

## Filesystem Mirror

`research_init()` creates:

```text
/research
/research/datasets
/research/results
/research/notebooks
/research/citations
/research/tasks
/research/timeline
/research/graph
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
research citation PROJECT KEY TITLE SOURCE
research experiment PROJECT NAME COMMAND
research result PROJECT EXPERIMENT SUMMARY
research task PROJECT TITLE OWNER
research timeline PROJECT LABEL NOTE
research graph PROJECT FROM RELATION TO
research notebook new PROJECT NAME
research notebook export ID
research latex PROJECT
research save PROJECT
```

## Remaining Work

- Rich notebook cell execution and output capture.
- Figure registry and plot artifacts.
- Reproducibility metadata for process/job inputs.
- GUI Research app and Rusa `std.notebook` bindings.
