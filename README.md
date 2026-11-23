# Contextual Wrench and Fire for Going Commando

Throw the OmniWrench while running or jumping, inspired by A Crack in Time. With a weapon drawn, R2 fires through the game's normal weapon controls.

Supports **Windows x86-64**, the **OpenRAC 1.1.0** game build, and **Ratchet & Clank: Going Commando USA v1.01** (`SCUS-97268`). The plugin uses OpenRAC mod API 1.12.

## Install

1. Select **Going Commando** in OpenRAC Launcher.
2. Open **Mods → Add from a link** and paste this repository address.

   ```text
   https://github.com/Joebannerlord/openRAC-RC2-contextual-wrench-throw
   ```

3. Choose **Install**, enable the mod, and start the game.

You can also download the Windows ZIP from [Releases](https://github.com/Joebannerlord/openRAC-RC2-contextual-wrench-throw/releases) and import it through **Mods → Add a mod**. [OpenRAC installation guide](https://openrac.dev/mods/docs/install-from-link)

## Controls

| Action | Input |
| --- | --- |
| Throw the drawn OmniWrench | Hold **R2**, then tap **Square** while standing, running, or jumping. Press Square again during flight to queue one throw after the catch. |
| Fire the drawn weapon | **R2** or **Circle**. |
| Strafe | Hold **L2**. |
| Lock on and fire | Hold **L2 + R2** with Slim Cognito's Lock-On upgrade enabled for the drawn weapon. |
| Stock wrench attacks | **Square** for the normal attacks. **R1 + Square** for the stock Comet Strike. |

Vehicle R2 controls keep their normal behavior. If you use RAC2 Auto Gadgets, place it before this mod in the Mods list so it can read physical Circle before the R2 fire mapping.

## Build on Windows

Use an x64 Visual Studio developer prompt with MSVC, CMake 3.24 or newer, and Python 3. The fetcher downloads and verifies the OpenRAC Mod SDK pinned in [sdk.lock](sdk.lock).

```powershell
python tools/fetch_sdk.py
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target release_zip
```

The Windows ZIP is written to `build/dist/`. The build and tests use synthetic inputs and the public SDK.

## Credits and license

- **Joebannerlord** maintains the Going Commando port.
- **Christian** created the [original RC1 wrench-throw mod](https://github.com/chistianh/wrench-throw). Its wrench-throw design, R2 fire mapping, and arm-pose approach informed this port.
- The [ReRAC2 project](https://github.com/re-rac/rerac2) provided a reference for Going Commando's controls and wrench behavior.

The mod is licensed under [GPL-3.0-or-later](LICENSE). The SDK template and workflow carry the [OpenRAC Mod SDK MIT notice](LICENSE-OPENRAC-SDK).
