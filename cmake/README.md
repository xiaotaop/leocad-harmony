# CMake integration

Owner: team lead, reviewed by the affected module owners.

`LeoCADSources.cmake` mirrors the upstream `leocad.pro` source, form and
translation lists. Every added or removed compilation unit must be reflected in
both build systems until the CMake port replaces qmake upstream-wide.

Host tools such as `moc`, `uic` and `rcc` must be distinguished from target ARM64 libraries and compilers.
