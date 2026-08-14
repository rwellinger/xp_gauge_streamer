# CLAUDE.md

Project guidance for Claude Code and other AI assistants working in this repository.

## Project Overview

**xp_gauge_streamer** is an X-Plane 12 plugin that captures the default Laminar
GNS430/530 display and streams it to a web frontend, where it can be operated by
click/touch instead of via the cockpit popout. Target platform is macOS Apple
Silicon (arm64) only.

## Code Quality

All implementation in this repo must follow clean-code best practices. This
applies to every change, now and in the future:

- **Single responsibility**: each function does one thing; each module owns one concern.
- **Meaningful names**: variables, functions and types read as plain English (or the
  project's consistent language) — no abbreviations, no cryptic suffixes.
- **Small functions, shallow nesting**: prefer early returns and helpers over deeply
  nested conditionals.
- **DRY**: extract shared logic rather than copying it; but don't abstract
  speculatively without a concrete need.
- **Encapsulation**: keep statics/internals private to their translation unit; expose
  only what the header promises.
- **Separation of concerns**: UI code never touches file I/O directly; data modules
  never draw.
- **Minimal comments**: let the code explain itself. Add a comment only when the *why*
  is non-obvious (invariant, workaround, surprising constraint). Don't comment what the
  code already says.
- **No speculative generality**: don't build abstractions for hypothetical future
  needs — match the existing codebase style.
- **Boundaries only for validation**: trust internal code; validate at the edges (user
  input, external APIs, file parsing).
