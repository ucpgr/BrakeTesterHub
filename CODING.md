Coding Conventions

This document defines coding standards and architectural conventions for the project.

The goal is to ensure all code is consistent, readable, maintainable, and aligned with the system design.


---

1. General coding goals

Code should be:

readable first

modular

easy to test

explicit rather than clever

maintainable under real-world conditions


Avoid overly complex or abstract designs unless they clearly improve the system.


---

2. Naming conventions

Types

Use PascalCase:

classes

structs

concepts

enums


Examples:

LptListener

PrintArtifactRepository

RenderedPage



---

Functions

Use camelCase:

getRawBytes()

generateThumbnail()

writeArtifacts()


Functions should describe an action.


---

Variables

Use camelCase:

pdfPath

vehicleData

maxWidth



---

Constants

Prefer kPascalCase or clear constexpr naming:

kDefaultTimeoutMs

kSerialBaudRate



---

Private members

All private member variables must use the m_ prefix.

Examples:

m_port

m_status

m_activePortId


This rule is mandatory and must be consistent across the codebase.


---

Interfaces and concepts

Interfaces may use I prefix:

ILptSource


Concepts should use clean names:

LptSource



---

Files

Match file names to main types:

LptListener.h

PrintArtifactRepository.cpp


Avoid vague names like utils.h unless justified.


---

3. Design patterns

Preferred

Composition over inheritance

Prefer combining small classes rather than deep hierarchies.

Manager / orchestrator

Managers coordinate workflows but do not implement everything.

Repository pattern

Encapsulate persistence logic:

SettingsRepository

PrintArtifactRepository


Store pattern

Use small state holders for UI-facing state.

Pipeline pattern

Use pipeline flow for staged data processing:

source → patcher → renderer → writer


---

4. Patterns to avoid

Avoid:

deep inheritance

god objects

global mutable state

over-engineering

large generic utility files



---

5. Function design

Functions should:

do one thing

be easy to test

have clear inputs/outputs


Prefer explicit error handling over hidden failures.


---

6. Class design

Each class should have one responsibility.

Split classes if they start handling:

hardware

transformation

persistence

UI


all at once.


---

7. Headers and sources

headers = contracts

source files = implementation


Keep headers lightweight.


---

8. Templates and concepts

Use carefully:

default to simple classes

use templates where helpful

use concepts for clarity


Avoid complexity for its own sake.


---

9. Dependencies

Keep dependencies at system edges.

Avoid leaking implementation details across modules.


---

10. Comments

Write comments for:

non-obvious behaviour

hardware quirks

protocol details


Avoid redundant comments.


---

11. Testing

Prefer:

small testable units

deterministic logic


Test critical components like:

protocol parsing

rendering logic

repositories



---

12. Guidance for contributors

follow naming rules strictly

keep modules small and focused

avoid unnecessary abstraction

maintain pipeline clarity

write debuggable code



---

13. Summary

The coding style is:

clear

modular

practical

consistent


Code should feel like it belongs in a hardware-integrated, maintainable C++ system.
