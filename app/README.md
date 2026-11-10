# HarmonyOS application shell

Owner: team lead.

Qt 6.12 for HarmonyOS generates the application module, Ability metadata and
Hvigor packaging project from the root CMake target. CMake overlays
`icons/jimuligou_icon.json` and its PNG layers onto Qt's package template.
`icons/jimuligou_store.png` is the flattened 1024 × 1024 opaque PNG for the
AppGallery listing.

Do not create a second manual ArkUI application shell around the Qt window. Do
not commit signing keys, passwords, local SDK paths or generated packages.

The Qt UI remains the product UI. The application shell should only provide lifecycle, packaging and platform services required by the port.
