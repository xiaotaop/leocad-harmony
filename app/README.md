# HarmonyOS application shell

Owner: team lead.

Qt 6.12 for HarmonyOS generates the application module, Ability metadata and
Hvigor packaging project from the root CMake target. Port-owned package source
overrides (icon, label, permissions and localized metadata) will live here only
when `QT_HARMONYOS_PACKAGE_SOURCE_DIR` is enabled.

Do not create a second manual ArkUI application shell around the Qt window. Do
not commit signing keys, passwords, local SDK paths or generated packages.

The Qt UI remains the product UI. The application shell should only provide lifecycle, packaging and platform services required by the port.
