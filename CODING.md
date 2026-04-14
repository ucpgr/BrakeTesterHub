# Coding Conventions

This document defines coding standards and architectural conventions for the project.

The goal is to keep code consistent, readable, maintainable, and aligned with the system design in `PROJECT.md`.

---

## 1) General coding goals

Code should be:

- Readable first
- Modular
- Easy to test
- Explicit rather than clever
- Maintainable under real-world conditions

Avoid overly complex or abstract designs unless they clearly improve the system.

---

## 2) Naming conventions

### Types

Use **PascalCase** for:

- Classes
- Structs
- Concepts
- Enums

Examples:

- `LptListener`
- `PrintArtifactRepository`
- `RenderedPage`

### Functions

Use **camelCase**:

- `getRawBytes()`
- `generateThumbnail()`
- `writeArtifacts()`

Functions should describe an action.

### Variables

Use **camelCase**:

- `pdfPath`
- `vehicleData`
- `maxWidth`

### Constants

Prefer `kPascalCase` (or clear `constexpr` naming):

- `kDefaultTimeoutMs`
- `kSerialBaudRate`

### Private members (mandatory)

All private member variables **must** use the `m_` prefix.

Examples:

- `m_port`
- `m_status`
- `m_activePortId`

### Interfaces and concepts

- Interfaces may use the `I` prefix (for example, `ILptSource`).
- Concepts should use clean names (for example, `LptSource`).

### Files

Match file names to main types:

- `LptListener.h`
- `PrintArtifactRepository.cpp`

Avoid vague names like `utils.h` unless there is a clear, documented reason.

---

## 3) Design patterns

### Preferred

- **Composition over inheritance**: combine small classes rather than creating deep hierarchies.
- **Manager/orchestrator**: managers coordinate workflows but should not implement everything.
- **Repository pattern**: encapsulate persistence logic (for example, `SettingsRepository`, `PrintArtifactRepository`).
- **Store pattern**: use small, focused state holders for UI-facing state.
- **Pipeline pattern**: staged data processing (`source → patcher → renderer → writer`).

### Avoid

- Deep inheritance
- God objects
- Global mutable state
- Over-engineering
- Large generic utility files

---

## 4) Function design

Functions should:

- Do one thing
- Be easy to test
- Have clear inputs and outputs

Prefer explicit error handling over hidden failures.

---

## 5) Class design

Each class should have one responsibility.

Split classes if they begin to handle hardware, transformation, persistence, and UI concerns all at once.

---

## 6) Headers and source files

- Headers = contracts
- Source files = implementation

Keep headers lightweight.

---

## 7) Templates and concepts

Use these carefully:

- Default to simple classes.
- Use templates where they clearly reduce duplication.
- Use concepts where they improve clarity.

Avoid complexity for its own sake.

---

## 8) Practical review checklist (recommended)

Before opening a PR, quickly confirm:

- Naming follows this document.
- New classes/functions have a single clear responsibility.
- Errors are surfaced explicitly.
- Persistence access stays in repositories.
- New utilities are scoped and justified.
- Headers do not pull in unnecessary dependencies.

---
