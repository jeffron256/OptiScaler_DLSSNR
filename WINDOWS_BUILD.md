# DLSS-NR Windows build: brief for Claude Code

**How to use this file:** save it on the Windows PC as `C:\dev\WINDOWS_BUILD.md`, open a terminal in `C:\dev`, run `claude`, and say: *Follow WINDOWS_BUILD.md.* Everything below is written to Claude Code.

---

## Task

Clone and build the DLSS-NR multi-pass + model-switch build of OptiScaler on this PC, fix any compile errors, and hand me the built files. **I will copy the files into Euro Truck Simulator 2 and test it myself. Do not touch the game folder.**

You are picking this up from a Claude Code session on my Mac that wrote the code but could not compile it (no Windows SDK there). Nothing from that conversation is available to you, so everything you need is below. Work through the phases in order, and stop and ask me wherever it says ASK. Shell commands below are PowerShell.

## Background

- Repo: https://github.com/jeffron256/OptiScaler_DLSSNR (my fork of Dagherbou/OptiScaler_DLSSNR, which is a fork of OptiScaler). Branch `dlss-neural-rendering`. Expected tip: `777d8756` "DLSS-NR: add the end-user readme the Release package copies". The commit before it, `dad981d5` "DLSS-NR: multi-pass (1-3x) and a live model-file switch", is the change being built.
- The fork runs NVIDIA's DLSS 5 Neural Rendering model (`nvngx_dlssnr.dll`, NGX feature 18) over the upscaler's output. All calls into the model go through a small forwarder DLL, `nvngx.dll_dlssnr.dll`, because the model only accepts callers whose module path contains "nvngx.dll".
- What `dad981d5` added:
  1. Multi-pass, 1 to 3 (`[DlssNr] Passes`, menu "Cost > Passes"). Every extra pass has its own NGX feature with its own history. On D3D12 a pass's feature is built on a frame that evaluates nothing; on native Vulkan it is first run on the frame after it was built. Passes alternate between `output` and `passOutput`, and the last pass always lands in `output`, so the resolve composes the final answer against the original proxy. Local tone applies to pass 1 only.
  2. Model file switch (`[DlssNr] Model`, menu "Model > Model file"). `OptiScaler/dlssnr/DlssNr_Models.{h,cpp}` lists `nvngx_dlssnr.dll`, renamed `nvngx_dlssnr*.dll` copies, and `DlssNrModels\<name>\nvngx_dlssnr*.dll` beside OptiScaler or the game, with each file's version. An empty choice means the old default lookup.
  3. The forwarder (`OptiScaler/dlssnr/forwarder/dlssnr_forwarder.cpp`) now holds up to 8 model DLLs by path, records which DLL created each feature so evaluate and release reach the right one, and exports `dlssnr_multi_model`. Without that export, OptiScaler only applies a model choice after a restart.
  4. Native Vulkan now rebuilds the feature when a create-time setting (preset, style, intensity, local structure/tone, skin, auto mask) changes; before, those changes waited for a resize. Vulkan failures now show in the menu with a Retry button.
- This code has NOT been compiled with MSVC yet. On the Mac, only the forwarder (against stub Windows headers) and the model discovery were compiled with clang and unit-tested with fakes. Expect possible compile errors, most likely in `OptiScaler/shaders/dlssnr/DlssNr_Dx12.cpp`, `OptiScaler/dlssnr/DlssNrFeature_Vk.cpp`, `OptiScaler/dlssnr/DlssNr_Menu.cpp` and `OptiScaler/dlssnr/DlssNr_Models.cpp`.
- Read before changing anything: `OptiScaler/dlssnr/README.md` (sections "Choosing a model" and "Multi-pass") and `OptiScaler/dlssnr/design/DEVELOPMENT.md` (the invariants, and section 2.2, the menu spec).

## Rules

- Fix build errors with the smallest change that keeps the design above. Do not redesign, refactor, reformat whole files, or simplify the pass logic.
- Default-identical: with Passes=1 and no model chosen, behaviour must stay exactly what it was before `dad981d5`.
- Never edit `OptiScaler/shaders/dlssnr/precompile/dlssnr.hlsl` or the precompiled `DlssNr_Shader*.h` headers.
- Never commit build output. `x64/` is gitignored, and so are `OptiScaler/resource_build_date.h` and `OptiScaler/resource_build_commit.h`, which the pre-build step writes.
- Commit fixes on `dlss-neural-rendering`, in the repo's message style (`DLSS-NR: ...`). Never push unless I ask.
- Do not read, write or copy anything in the game folder. Installing and testing are mine.
- ASK before installing software and before deleting anything.

## Phase 1: tools

Check these and tell me what is missing. ASK before installing anything.

- `git --version`
- Visual Studio 2022 with the "Desktop development with C++" workload (a newer Visual Studio also works if its "MSVC v143 build tools" component is installed). Find MSBuild with:

  `$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"`

- If Visual Studio is missing, the install is `winget install Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive"`. ASK first.
- No Vulkan SDK and no NVIDIA SDK are needed: the headers and libraries come with the repo and its submodules.

## Phase 2: clone

Use a short path; the submodules are deep and can hit the 260-character path limit.

```
git -c core.longpaths=true clone --recursive -b dlss-neural-rendering https://github.com/jeffron256/OptiScaler_DLSSNR.git C:\dev\OptiScaler_DLSSNR
cd C:\dev\OptiScaler_DLSSNR
git config core.longpaths true
```

Verify:
- `git log -1 --oneline` shows `777d8756` (or a later commit of mine on that branch).
- `git submodule status` has no line starting with `-`. If any does, run `git submodule update --init --recursive`.
- The repo root contains `READ ME - DLSS Neural Rendering.txt` (the Release post-build step copies it).

## Phase 3: build

```
& $msbuild OptiScaler.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /fl "/flp:logfile=build.log;verbosity=normal"
```

The solution builds two projects: `OptiScaler` (OptiScaler.dll) and `dlssnr_forwarder` (nvngx.dll_dlssnr.dll).

If it fails: read `build.log`, fix, rebuild, and repeat until it is clean. Then commit all the fixes as one commit and tell me what you changed and why.

It succeeded when `x64\Release\a\` contains `OptiScaler.dll`, `nvngx.dll_dlssnr.dll`, `OptiScaler.ini`, `READ ME - DLSS Neural Rendering.txt`, `setup_windows.bat`, `Licenses\` and `OptiScaler\` (the XeSS and FidelityFX DLLs and `D3D12_OptiScaler\`). Also confirm the new forwarder carries the multi-model export, e.g. `Select-String -Path x64\Release\a\nvngx.dll_dlssnr.dll -Pattern dlssnr_multi_model -SimpleMatch -Quiet` returns True (or use `dumpbin /exports` from a Developer PowerShell).

## Phase 4: hand over

Stop here and give me:

- the full paths of `x64\Release\a\OptiScaler.dll` and `x64\Release\a\nvngx.dll_dlssnr.dll`, with their sizes and timestamps;
- the commit hash the build came from, and any fix commits you made;
- this reminder of where they go, which I will do myself:
  - `OptiScaler.dll` goes over the OptiScaler DLL already in `...\Euro Truck Simulator 2\bin\win_x64`, keeping that file's current name (usually `dxgi.dll`);
  - `nvngx.dll_dlssnr.dll` goes over the old one in `bin\win_x64`. It must be the new one, or model switching only applies after a restart;
  - the 310.9.16 model goes to `bin\win_x64\DlssNrModels\310.9.16\nvngx_dlssnr.dll`, and the current `nvngx_dlssnr.dll` stays where it is as the default;
  - my existing `OptiScaler.ini` stays; the new `Passes` and `Model` keys fall back to their defaults when missing.

Then wait. I will come back with results, a log, or an error.

## When I come back with results

I will paste lines from `OptiScaler.log` (I enable it with `LogToFile=true` and `LogLevel=2` under `[Log]`) or describe what I saw. Use my test plan below to judge them. Vulkan log lines start with `DLSS-NR Vulkan:`.

- Crash or freeze: ask for the last DLSS-NR lines in `OptiScaler.log`, plus `Documents\Euro Truck Simulator 2\game.log.txt`. A quick way for me to keep playing meanwhile: `Passes=1` and `Model=auto` under `[DlssNr]`.
- Menu says "Takes effect after a restart: this nvngx.dll_dlssnr.dll is an older build": the old forwarder is still in the game folder.
- Menu says "Off for this session: the chosen model would not initialise ...": that DLL failed to start. The log has a `create failed: init 0x... create 0x...` line; I can switch the model back and press Retry.
- Most likely places for a runtime bug: model switching (two same-named DLLs loaded side by side; `loadSnippet` and `FeatureOwners` in the forwarder), and raising Passes (`BuildPassFeatures` and the pass loop in `DlssNr_Dx12::Dispatch`; on Vulkan, `EvaluateAfterUpscaleVk` in `DlssNrFeature_Vk.cpp`).
- Fix only with my agreement: commit locally, rebuild, and give me the new files the same way as in Phase 4.

## My test plan (I run this; for reference)

1. Launch, press Insert, open "DLSS Neural Rendering", tick "Enable Neural Rendering". Expect a green line such as "Running - x.xx ms per frame" (or "Running natively on Vulkan ..."). Log: `DLSS-NR forwarder loaded from ...` with no "(an older build" note (D3D12 path); one `DLSS-NR model available:` line per model, after the menu section has been opened; `DLSS-NR running at ... model ... (310.8...)` or `DLSS-NR Vulkan: feature up at ...`.
2. Cost > Passes: 2, then 3 (the slider applies when released). Expect "..., 2 passes" / "..., 3 passes" in the green line, roughly twice and three times the ms, and one frame without the effect each time it goes up. Log: `feature for pass 2 built`, `2 model passes per frame`. Back to 1: `releasing the feature for pass ...`.
3. Model > Model file: pick `DlssNrModels\310.9.16\nvngx_dlssnr.dll`. Expect "In use: 310.9.16..." and lower ms. Log: `switching model: ... -> ...`. Switch back and forth twice.
4. Passes 3 with each model, then change Style once.
5. Alt-Tab out and back in, and change the game's resolution once.
6. Note the ms for each model and pass count.
