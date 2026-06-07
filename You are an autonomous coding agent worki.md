You are an autonomous coding agent working on this OS/GUI project.

Goal: Work through the entire roadmap below in one continuous run, in dependency order.

Execution rules:

* Do not stop after one item.
* Do not ask for confirmation between roadmap sections.
* First inspect the codebase and identify the current architecture.
* Create a full dependency-aware plan.
* Implement the roadmap in phases.
* After each phase, build and run relevant tests.
* If a phase is too large, land the smallest complete infrastructure slice, then continue.
* If blocked, document the blocker and continue with the next unblocked item.
* Keep changes modular and avoid dumping everything into kmain.c.
* Prefer reusable APIs for windows, input, files, processes, terminal buffers, and GUI apps.
* End with a clear summary of completed work, files changed, tests run, and remaining blockers.

Phase 0 — Rendering Stability & Flicker Elimination

* Fix the display blinking/flickering issue that occurs during typing, mouse clicks, focus changes, window movement, or GUI updates.
* Identify the root cause of the blink.
* Check framebuffer clearing, full-screen redraws, buffer swaps, focus events, cursor rendering, text rendering, window invalidation, and repaint logic.
* Replace unnecessary full-screen redraws with dirty-rectangle or partial-window updates where possible.
* Ensure typing text does not cause the desktop/background to flash.
* Ensure mouse clicks do not trigger visible screen flicker.
* Ensure dragging windows remains smooth and stable.
* Ensure wallpaper, desktop, and applications do not redraw unnecessarily.
* Validate the fix across Terminal, Editor, Settings, File Manager, Rusa Workbench, and desktop interactions.

Acceptance criteria for Phase 0:

* No visible blink during typing.
* No visible blink during clicking.
* No visible blink during window focus changes.
* No visible blink during window dragging or resizing.
* Rendering remains smooth and stable.

Phase 1 — True Interactive Windows

* Make every GUI app render relative to its own window rectangle.
* Let inactive windows receive clicks when focused.
* Add proper z-order, drag-to-front, and resize handles.

Phase 2 — Desktop Shell Foundation

* Add taskbar/window list.
* Add start/app launcher menu.
* Add desktop icons that open apps/files reliably.
* Add keyboard shortcuts: Alt+Tab, Super/Menu, Ctrl+Q.

Phase 3 — Real GUI Terminal

* Capture command output into a scrollable terminal buffer.
* Add cursor, selection, copy/paste, history, and smooth scrolling.
* Let terminal launch GUI apps and open files.

Phase 4 — Editor File Workflow

* Add New/Open/Save/Save As.
* Add file picker integration.
* Support .txt, .md, .rusa, and math notes.
* Keep Word/code/math modes as real editing modes.

Phase 5 — Filesystem GUI

* Add folders, breadcrumbs, open-with actions.
* Add create/rename/delete/copy/move.
* Add file metadata and permissions display.
* Let files open in Editor, Terminal, Rusa Workbench, or Math Lab.

Phase 6 — Rusa Workbench

* Add a proper source editor panel.
* Run .rusa files directly from the GUI.
* Show friendly diagnostics with clickable file/line links.
* Add package browser and import visualization.

Phase 7 — Settings App Expansion

* Hardware page: CPU, memory, GPU/framebuffer, keyboard, storage.
* Privacy page: master privacy switch, network visibility, app permissions.
* Display page: wallpaper, screensaver, cursor, refresh/safety mode.
* Input page: keyboard layout, repeat rate, mouse speed.

Phase 8 — Dynamic Wallpaper/Screensaver Polish

* Make lava lamp/rain/background modes smooth and low-flicker.
* Add calm mode to prevent seizure-risk flashing.
* Allow preview/apply from Settings.
* Separate screensaver from wallpaper behavior.

Phase 9 — Task Manager

* Show real process table, CPU ticks, memory estimate, app/window mapping.
* Add kill/restart/focus buttons.
* Show job queues for math/physics workloads.

Phase 10 — Math/Physics Lab

* Make linear algebra, calculus, group theory, graph theory, statistics, and physics tools more interactive.
* Add input forms and result panels.
* Add LaTeX export and plot/vector drawing.
* Tie heavy computations into process/job accounting.

Phase 11 — Security Center

* Show ports, services, device permissions, app capabilities.
* Add malicious-code scan results.
* Add plain-English explanations and disconnect/disable buttons.
* Track every app/network/device connection.

Phase 12 — Networking Stack

* Move beyond surface commands into real socket-like handles.
* Add connection table, buffers, listening ports, loopback messaging.
* Add automatic port shutoff and rate limiting.
* Show all connections in GUI Settings/Security.

Phase 13 — Kernel Cleanup

* Continue splitting shell/editor/gui/language/runtime out of kmain.c.
* Strengthen process tables and file descriptor tables.
* Add better module boundaries for kernel, GUI, FS, net, language, math.

Phase 14 — Boot Experience

* Boot directly into GUI reliably.
* Add recovery terminal mode.
* Add startup logs viewer.
* Add safe graphics mode if wallpaper/GUI rendering misbehaves.

Phase 15 — Developer Quality

* Add focused tests per module.
* Add GUI smoke tests for every app.
* Add Rusa parser test files.
* Add docs/ folder with architecture notes, Rusa syntax, GUI app docs, and kernel subsystem docs.

Start now by inspecting the repository, then fix Phase 0 before moving on to Phase 1.
