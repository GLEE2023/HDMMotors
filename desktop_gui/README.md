# GLEE Motor Studio

Native PySide6 desktop control GUI for HDMMotors firmware. **Prototype; hardware validation required.**

## Install and run

```powershell
cd desktop_gui
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python run.py
```

Build a Windows `.exe` with `build-windows.bat`. No executable is built or signed in this repository.

## Current Nano ASCII protocol

115200 baud, LF-delimited. Commands: `C` status, `H` home, `W/S` lead puck step, `+/-` 1 mm jog, `N/P` barrel step, `I<n>` barrel index, `Y<n>` absolute yaw, `R<n>` relative yaw, `Z` zero yaw, `w` reset servos, `e` arm, `M` pulse servos, `G` deploy one barrel, `Q` deploy entire FMS, `T` SPI check, `?` help. Case matters for `W` vs `w`. At a G/Q prompt, **empty newline fires**; `N` cancels; `Y/R/Z` adjust yaw.

## Safety, UX, and STM32 portability

- Physical emergency-stop circuitry and independent mechanical protection are essential. A USB `x` request is not a guaranteed physical stop.
- Firmware currently performs blocking moves. The GUI guards deployment command sequencing but cannot verify physical positions or completion without additional firmware acknowledgements/feedback.
- During `G/Q`, normal controls are locked to avoid accidentally cancelling at a fire prompt.
- `G` lacks an unequivocal success marker; the recovery path disconnects and requires operator verification rather than assuming completion.
- GUI widgets call a `Coordinator`, which calls a replaceable `MotorProtocol`. Future STM32 firmware should implement the same semantic interface using a framed, versioned transport with request IDs, ACK/ERR, completion events, and independent abort.

Run protocol tests: `python -m pytest -q` (requires pytest). Test serial behavior and physical interlocks on a safe bench before field operation.
