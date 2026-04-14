# Project Design Decisions

This document captures the major design choices, constraints, and current direction of the project so future contributors and coding agents can work within the intended architecture.

It is not a strict specification; it is a record of decisions made so far.

---

## 1) Project purpose

This project is a **Brake Tester Hub** that bridges legacy brake tester hardware with a modern software interface.

### Goals

- Read live telemetry.
- Display data in a modern UI.
- Persist tests and artifacts.
- Capture and process legacy print output.
- Render modern printable formats.
- Run reliably on low-power hardware.

---

## 2) Architectural direction

The system follows a modular architecture with clear boundaries.

### Core principle

> Domain decides **when**
> Modules decide **how**
> Repositories decide **where**

Avoid large architectural rewrites. Prefer small, focused modules.

---

## 3) Technology direction

### Backend (preferred)

- C++
- SQLite / `sqlite_orm`
- `libserial`
- `cpp-httplib`

### Frontend (preferred)

- Svelte
- Tailwind CSS
- Dark-mode-first UI

`Node/SvelteKit` was explored but introduced deployment complexity on Raspberry Pi Zero–class hardware.

---

## 4) Deployment environment

### Target hardware

- Raspberry Pi Zero 2 W

### Constraints

- Low resources.
- Simple deployment.
- Minimal runtime overhead.

---

## 5) Telemetry

Tracked data includes:

- Brake forces (L/R)
- Axle weight
- Wheel detection
- Roller state
- Derived efficiency
- Imbalance

UI presentation must prioritize clarity over density.

---

## 6) UI direction

### Preferred layout

- Two large brake gauges
- Central imbalance indicator
- Clear weight display

### Design goals

- Readable at a glance
- Responsive
- Minimal clutter

---

## 7) Print pipeline

Pipeline-style architecture is preferred.

### Core workflow

1. Receive raw data.
2. Patch (optional).
3. Render.
4. Store.
5. Print/display.

---

## 8) Serial/LPT capture

### Listener requirements

- 38400 baud
- Detect transmission start
- End after ~2 seconds of inactivity
- Single-threaded preferred
- Return full buffer only when complete
- Support test byte (`t`)

---

## 9) Storage strategy

- SQLite for structured data
- Filesystem for artifacts

Prefer storing raw data to support future reprocessing and improved renderers.

---

## 10) Database philosophy

- Avoid premature complexity.
- Prioritize reliable capture.
- Preserve flexibility for future changes.

---

## 11) Print artifacts

Artifacts include:

- Raw PRN
- Patched PRN
- Rendered pages (TIFF/PDF)

---
