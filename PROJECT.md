Project Design Decisions

This document captures the major design choices, constraints, and current direction of the project so future contributors and coding agents can work within the intended architecture.

It is not a strict specification. It is a record of the design decisions made so far.


---

1. Project purpose

This project is a brake tester hub that bridges legacy brake tester hardware with a modern software interface.

Goals:

read live telemetry

display data in a modern UI

persist tests and artifacts

capture and process legacy print output

render modern printable formats

run reliably on low-power hardware



---

2. Architectural direction

The system follows a modular architecture with clear boundaries.

Key principle:

domain decides when

modules decide how

repositories decide where


Avoid large architectural rewrites. Prefer small focused modules.


---

3. Technology direction

Backend (preferred direction)

C++

sqlite / sqlite_orm

libserial

cpp-httplib


Frontend

Svelte (preferred)

Tailwind CSS

dark mode UI


Node/SvelteKit was explored but introduced deployment complexity on Pi Zero.


---

4. Deployment environment

Target:

Raspberry Pi Zero 2 W


Constraints:

low resources

simple deployment

minimal runtime overhead



---

5. Telemetry

Tracked data includes:

brake forces (L/R)

axle weight

wheel detection

roller state

derived efficiency

imbalance


UI must prioritise clarity.


---

6. UI direction

Preferred layout:

two large brake gauges

central imbalance indicator

clear weight display


Design goals:

readable at a glance

responsive

minimal clutter



---

7. Print pipeline

Core workflow:

1. receive raw data


2. patch (optional)


3. render


4. store


5. print/display



Pipeline-style architecture is preferred.


---

8. Serial/LPT capture

Listener requirements:

38400 baud

detect transmission start

end after ~2s inactivity

single-threaded preferred

return full buffer only when complete

support test byte (t)



---

9. Storage strategy

SQLite for structured data

filesystem for artifacts


Prefer storing raw data for future reprocessing.


---

10. Database philosophy

avoid premature complexity

prioritise reliable capture

allow flexibility for future changes



---

11. Print artifacts

Artifacts include:

raw PRN

patched PRN

rendered pages (TIFF/PDF)


Organised by time-based folders (e.g. year/month).


---

12. PDF thumbnail generation

Handled as a small helper module.

isolated responsibility

no architectural impact



---

13. UI workflow integration

UI should support:

current test status

next vehicle selection

compact top bar indicators



---

14. Print status feedback

Frontend should receive status updates:

receiving

converting

printing


Keep this lightweight.


---

15. Performance philosophy

Prefer:

simple

robust

predictable


over complex solutions.


---

16. Contributor guidance

preserve modular design

avoid unnecessary abstraction

keep deployment simple

maintain clarity



---

17. Flexible areas

Still evolving:

backend structure details

repository interfaces

rendering internals

frontend/backend integration



---

18. Summary

The system is:

modular

practical

hardware-aware

C++-leaning backend

Svelte-based UI


Avoid unnecessary architectural churn.
