# GLEE HDM Mission Control — React prototype

Native **Tauri 2 + React/TypeScript + React Three Fiber** desktop frontend for the existing HDMMotors firmware. Uses the GLEE blue/white/gray brand and the uploaded emblem. The 3D reference assembly was converted from `Full+Assembly.stp`, with enclosure walls and plates excluded.

> **Status: unverified frontend prototype.** The original Arduino firmware and `desktop_gui/` are not changed. No physical actuator or firing testing was performed. A USB stop is NOT a substitute for a hardwired emergency stop.

## Build on Windows

Install Node.js LTS, Rust stable (`rustup`), and the Microsoft C++ Build Tools for Tauri. WebView2 is included on most modern Windows 10/11 installations; install it if absent. Then:

```powershell
cd react_frontend
npm install
npm run desktop:dev
```

To create a Windows installer:

```powershell
npm run desktop:build
```

Result: `src-tauri/target/release/bundle/nsis/`. A Windows executable also builds in `src-tauri/target/release/`. This archive does not contain a compiled or signed `.exe`.

For UI-only browser preview: `npm run dev`, then open `http://127.0.0.1:1420`. **Browser preview has no USB access; controls cannot operate hardware.**

## Visual design

- React Three Fiber/Three.js renders the actual STEP-derived mechanism as a GLB. The source CAD's enclosure walls were not included.
- Drag to orbit and scroll to zoom. Use *Explode*, *Preview motion* and *Guides* as presentation tools.
- The illustration's upper-section rotation is an illustrative preview, **not calibrated kinematic motion or encoder-reported state**. The mechanism is the user's older reference assembly; some production pieces differ.
- Real-time values are displayed separately from CAD animation, with explicit connection and source labels.
- Libraries.dev `border-beam` and `metal-fx` provide limited ornamental accents. GPU effects stop when unnecessary; 3D uses on-demand rendering when static.
- UI is offline-first, with all images/fonts/model assets bundled locally.

## Security and firmware compatibility

The desktop uses **Tauri native IPC**, not a listening REST server, no remote resources, and no frontend HTTP polling. A Rust actor exclusively owns the serial connection. It exposes only enumerated semantic commands, checks state and numeric bounds, and emits status/log events. It does not expose arbitrary serial writes or shell execution.

Current 115200-baud Nano serial contract: `C` status, `?` help, `T` SPI test; `W`, `S`, `+`, `-`, `H` lead controls; `N`, `P`, `I<n>` barrel; `Y<n>`, `R<n>`, `Z` yaw; lowercase `w` reset, lowercase `e` arm, uppercase `M` fire; `G` full barrel deployment; `Q` full FMS; `x` stop request during motion; empty Enter fires at a staged prompt and `N` cancels there. The backend never exposes the burnwire command.

**Important firmware limitations:** Nano motor movement is blocking. The Rust actor intentionally avoids status polling during motion, because queued commands ahead of `x` can defeat a software interrupt. The GUI requests status only at startup, on explicit refresh, and after recognized terminal firmware messages. Status is considered complete only after the final `Barrel Solenoid:` line. Status is still *software-tracked*, not verified encoder telemetry. `G` currently has no reliable completion acknowledgement, so it must be reconciled manually after physical verification. An independent physical emergency stop is required.

There is no STM32 coupling in React. When the STM32 firmware adds a versioned protocol, implement a backend adapter with the same semantic API: status/telemetry events, validated motor commands, start/complete/error acknowledgements, and sequence-specific confirmations.

## CAD asset

`public/models/glee-mechanism.glb` comes from the user's older `Full+Assembly.stp`. The included `tools/convert_step.py` can regenerate the model when the new CAD is available. It uses `OCP` (as provided by CadQuery) and `trimesh`; these are only needed for CAD conversion, **not at runtime**. The source STEP is not committed; preserve that original separately.

For proper animated joints, provide separate barrel, lead nut/elevator, yaw stage, and servo geometry with axis pivots, parent/child hierarchy, zero references, and travel ranges. Current group separation is limited to STEP's available `ActiveMotor` and `ASSEMBLY V1` solids. Do not treat the illustrative motion as physically correct.

## Testing

JSON configs and TypeScript syntax can be checked with standard tooling. Full type-check, Tauri build, runtime rendering, and USB hardware tests need a Windows development environment with npm/cargo dependencies installed.

The `tests/check_assets.py` script inspects the bundled GLB, checks enclosure naming filters, and ensures expected model assets are present. It does not substitute for physical tests.
