# Frontend-neutral application layer

This directory contains the UI and controller state that can be shared by an
SDL2 desktop frontend and the Android application. It deliberately has no SDL,
Android, or Java dependencies so it can be covered by the ordinary C++ test
suite on every host.

- `FrontendModel` owns menu navigation and scans a user-selected directory for
  `.sfc`, `.smc`, `.fig`, and `.swc` files. It does not recursively scan folders
  or load ROM contents. Android will supply a Storage Access Framework picker;
  SDL/desktop builds can supply a normal filesystem directory.
- `ControllerMapper` converts standardized gamepad controls and left-stick
  directions into the 12-bit SNES joypad state. The `South/East/West/North`
  layout matches SDL's standardized face-button positions and maps to SNES
  `B/A/Y/X`; shoulders map to `L/R`. This class does not open Bluetooth devices.

Actual OS input discovery, button rebinding UI, ROM loading, and emulator
execution remain integration tasks for the SDL2/Android adapters.
