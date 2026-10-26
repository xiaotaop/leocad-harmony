# Contributing

## Branches

Create focused branches from the current `main` branch. Keep each pull request independently buildable and revertible.

Suggested ownership:

| Branch | Owner | Scope |
|---|---|---|
| `feature/leader-platform` | Team lead | DevEco, CMake, HAP/APP, lifecycle, paths, physical device and release |
| `feature/a-render` | Member A | OpenGL context, view, rendering, picking and input |
| `feature/b-model` | Member B | Model editing, undo/redo, LDraw I/O and model tests |
| `feature/c-ui-library` | Member C | Qt Widgets UI, parts library, resources, localization and release material |

## Pull requests

Every pull request must include:

1. Purpose and affected modules.
2. Build instructions and result.
3. Automated or desktop test result.
4. Physical-device checks still required.
5. A safe fallback or disable switch for risky platform behavior.

At least one non-author must approve. The team lead merges to `main`. AI-generated changes must be understood and reviewed by the module owner; an AI response is not a substitute for a build or test result.

## Physical-device requests

The team lead runs one device batch at 21:00 each day. Requests must arrive before 20:30 with a commit hash, an artifact, no more than ten reproduction steps, the expected result, and required log keywords. The module owner analyzes the returned evidence and owns the code fix.

## Upstream code style

Keep upstream modifications small and attributable to a HarmonyOS requirement. Keep file lists alphabetically sorted. Preserve translatable UI strings and existing source formatting. Nontrivial commits need a concise subject and a body explaining the problem, reason and resulting behavior.
