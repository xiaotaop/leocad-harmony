# CMake integration

Owner: team lead, reviewed by the affected module owners.

This directory will hold Qt for HarmonyOS toolchain integration and imported third-party targets. The upstream `leocad.pro` remains the reference source list until the CMake wrapper is complete.

Host tools such as `moc`, `uic` and `rcc` must be distinguished from target ARM64 libraries and compilers.
