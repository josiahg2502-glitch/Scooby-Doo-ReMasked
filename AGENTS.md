# Scooby-Doo: Re-Masked - Scooby-Doo! Unmasked (GameCube, G5DE78) PC recomp - agent notes

The port's name is **Scooby-Doo: Re-Masked** (user, 2026-10-16). Text naming the original disc keeps "Scooby-Doo! Unmasked".

Read this first. Same toolchain as the Super Paper Mario port (C:\Users\josia\Desktop\Recomps\SuperPaperMarioRecomp),
copied here and adapted for the GameCube. The user pays per tool call: batch work, never take desktop input.

## Layout
- `Disc\G5DE78.iso` (from the user's .ciso), `Disc\DATA\{sys,files}` extracted (tools in the scratchpad were
  ciso2iso.py / gcextract.py; the runtime only needs Disc\DATA).
- `game\main.dol` + `game\engine_ret.elf`: THE DISC SHIPS AN UNSTRIPPED RELEASE ELF whose sections are byte-identical
  to main.dol -> real names for all 6221 functions and 15887 data symbols:
  `wiicompiled\projects\scooby\elf_symbols.txt`, `elf_data_symbols.txt`, `MAP.txt` (translator function map).
- `wiicompiled\projects\scooby\recomp.yml`: entry 0x80011C1C, r13 0x8027E0C0, r2 0x80282020, 24 MB MEM1.
- Build: `tools\build-scooby.ps1 [-SkipTools] [-SkipTranslate]` -> `Play\ScoobyDoo.exe`. A new PPC_NATIVE_OVERRIDE needs a
  run WITHOUT -SkipTranslate (shards). Translator rebuild: toolchain\dotnet\dotnet.exe build translator\src\Translator.Cli -c Release.
- Disassembler: `python wiicompiled\projects\scooby\tools\scooby_dis.py <symbol|addr> [count]` (needs user-site capstone,
  so no `-I`). Profiler: `NSMBW_PROFILE=<start s>:<len s>` (also starts from OSGetResetButtonState, before the first frame).
- Boot test: run Play\ScoobyDoo.exe with stdout/stderr redirected for ~45 s, kill, read the .err log
  (autotest\bootN.err.log). Config: Play\UserData\Config.toml (windowed for tests).

## Port work done (2026-10-16)
- HLE hooks retargeted SPM -> Scooby by symbol name: `tools\apply_scooby_remap.py` (235 rewritten, 108 disabled: Wii-only
  IOS/NAND/WPAD/SC/ES, plus SDK calls this game never links - no OS threads, no GXBeginDisplayList). Log: remap_actions.csv.
- SDK data addresses retargeted by the variable name in their comments: `apply_data_remap.py` (50), function literals:
  `apply_func_literal_remap.py`. Remaining SPM literals listed in literals.csv / data_actions.csv ("review"); the live
  boot-path ones were fixed by hand (RunQueue, DVD WaitingQueue, HorVer fields, AX task/flags, arena, game ID).
- Old GameCube SDK differences fixed: gxData is 0x5B0 (no Wii DL fields), CPUFifo/GPFifo are pointers, no
  CPUFifoReady; no Wii dvdContexts / __VIDimming; AlarmQueue = r13-0x44D8; AX has no request callback / vector vars.
- GameCube-only HLE added: `hle/os/gc_hw.cpp` (reset button, 16 MB host ARAM: ARInit/ARStartDMA/ARQPostRequest...),
  `hle/storage/card_gc.cpp` (all 19 CARD calls -> Aurora's GCI-folder memory card in UserData\Saves\MemoryCard; aurora_card
  now linked), dvd.cpp GC section (DVDInquiryAsync/ReadDiskID/CheckDisk/GetDriveStatus complete at once;
  DVDGetDriveStatus also drains queued completions because the game busy-waits on its read callback).
- GameCube FST units: byte offsets (Wii: >>2). Changed in dvd.cpp (fst.bin parse, DVDReadPrio startAddr) and
  dvd_contract.h (published FST). Internally the runtime still keeps word offsets.
- Translator: added bso/bns (Bink audio uses them). emit-base-manifest step dropped (needs a REL; runtime never reads it).
- Branding: product/title strings say Scooby-Doo! Unmasked; game_folder.cpp accepts G5DE78; exe icon = disc icon.tga
  (tools\tga_to_ico.py; SPM's kept as runtime\assets\icon\spm_tippi.ico.bak).
- `_runtime_spm_backup\` = the runtime before any Scooby change.

## Logic modules (runtime-loaded game code) - READ BEFORE TOUCHING THE BUILD
The levels' game logic is compiled PowerPC in relocatable ELF objects inside each .ghr (2632 objects, 1557 unique,
18 MB). EFLogicCode::LoadFile (0x800E799C) MMMallocs, memcpys and relocates them (types 1/4/6/10/26 only; memcpy/memset
the only external symbols), then calls them. Heap addresses vary per run, so:
- `projects\scooby\tools\build_logic_overlay.py` (run by the build) places every unique module at a fixed address in an
  overlay above MEM1 (0x81800000..0x83000000, runtime memory.cpp region now 24 MiB), relocates it exactly like the game,
  writes `game\main_logic.dol` (main.dol + text section 2 + two patched calls: 0x800E7AE8 bl MMMalloc -> LogicAlloc stub
  0x81800000, 0x800E796C bl MMFree -> LogicFree 0x81800008), `MAP_logic.txt` (STT_FUNC symbols) and
  `runtime\src\hle\scooby_logic_table.inc`. memcpy/memset calls go through veneers at 0x81800010/18 (bl range).
  recomp.yml now points at main_logic.dol / MAP_all.txt, memory.size 0x03000000; the build refreshes the sha256 pin.
- `runtime\src\hle\scooby_logic.cpp`: LogicAlloc hashes the module being loaded (FNV-1a 64 of .text + .rela.text) and
  returns its fixed address, so the game relocates into the bytes that were translated. Log: `[logic] module ...`;
  `[logic] UNKNOWN` = not on the disc scan (would run untranslated).
- `build-scooby.ps1 -LogicLevels MAINMENU` (default) translates only the main menu's 34 modules (fast builds).
  EVERY OTHER LEVEL NEEDS `-LogicLevels all` (18 MB more code: long translate/compile, much bigger exe) or a list
  like `MAINMENU,W1L1`. Addresses never change with the subset.
- Translator changes for them (Program.cs Attempt, JumpTableDetector.cs): in the overlay only MAP symbols are function
  starts (strings live in .text); relative jump tables whose base is re-materialised in another register, and lwzx with
  index/base swapped, are recognised.
- Tools: `logic_unique.py`, `find_logic_elfs.py`, `logic_module_syms.py <files> <key> [offset]`.

## Where boot stands (2026-10-16 21:17)
THE MAIN MENU RENDERS (haunted mansion 3D scene, autotest\window.png) with menu music, at 60 fps, all 1557 logic
modules translated (-LogicLevels all, exe 199 MB = Play\Scooby-Doo Re-Masked.exe). Fixed today, in order:
- lfs of single denormals flushed to zero under FPSCR NI (memory_access.h PpcSingleBitsToDouble).
- Runtime-loaded logic modules (section above), incl. translator jump-table and overlay fixes.
- Zero-length DVD read at EOF succeeds.
- Black picture: GXSetCurrentMtx(GX_IDENTITY = 60) selects the identity "texture" matrix as the position matrix
  (HUD/2D draws); Aurora clamped it to TEXMTX9 (zeros). aurora shader_info.cpp stages identity for slot 20.
- Startup cover card removed (settings_overlay.cpp), PAD hooks enabled at Scooby addresses (pad.cpp: PADInit/Read/
  Reset/Recalibrate/ControlMotor -> Aurora pads, keyboard included) - input NOT yet verified by hand.
- "Fatal Error! Current operation aborted!" = memory-card result codes: Aurora's CARDUnmount stub answered NOCARD and
  its GCI-folder open answers NOCARD for a missing file; card_gc.cpp maps them to READY / NOFILE. [card] lines log
  every CARD result.
- Saving: Aurora's GCI-folder backend registered a newly created file as closed, so the write right after CARDCreate
  failed and saves stayed all zeros (aurora lib\card\CardGciFolder.cpp createFile now opens it). USER VERIFIED 21:25:
  menu, input, music, save and reload all work.
- Display (22:20): VERIFIED widescreen Fill (Hor+ 3D, natural proportions), 4K (resolution_multiplier 4.5 -> 2160 lines,
  60 fps, ~3000 draws, GX ~1 ms) and 120 fps (frame_interpolation_fps 120 -> "displayed 120.0 fps", draws matched).
  Fixed on the way: game switches VI to 640x448 after __GXInitGX set copy src/dst 640x480 -> vi.cpp VIConfigure HLE
  sets both to the mode; aurora vi.cpp resizes the target when the VISIBLE size changes; gpu.cpp re-maps viewport/
  scissor after every render-target resize ([vpmap] log). Intro Bink videos play (WB logo etc.).
  FIXED 22:30 (user asked): the black band (~30 of 448 lines) under 3D scenes. Cause: the game calls VIConfigure
  (448-line mode) BEFORE GXInit, and GXInit resets the display copy to 640x480; the console's VI just drops the extra
  lines, the presenter showed them (proved with SCOOBY_RED_CLEAR=1: the band was the clear colour). vi.cpp
  VI_HLE_ApplyDisplayCopyForMode() applies the mode's copy src/dst on VIConfigure AND at the end of GX__Init.
  Diagnostics left in (capped logs): [band] CopyDisp/GXSetProjection/TexCopy, [gpuvp] GPU viewports, [vpmap].
- BUILD GOTCHA: generated\build_shards\base_common_shard_map.json freezes shard boundaries. After the function set
  grows a lot (e.g. -LogicLevels change) DELETE IT, or every new function lands in one giant shard (465 MB, hours).
Next: a black strip along the bottom of the picture (viewport/aspect), verify pad input and menu navigation, enter a
level, check widescreen/4K/120 fps on this game, Bink videos (VIDEO\*.BIK) are not played yet.
Diagnostics: SCOOBY_TRACE_IO=1 ([io] completions, [aram] transfers); Play\drawdump.txt = frame number to dump GX draws
([spmext] per-draw NDC extents); SPM_FLAT_TEST=4 draws everything flat white; tools\capture-window.ps1 = one window PNG.
## Known gaps
- GameCube AX (ucode CRC E2136399) is ported from Dolphin AX.cpp/AXVoice.h: ax_mix.cpp `ExecuteGcCommandList` (all 20
  GC commands, AXPBGc, 5x1 ms voice steps with PB updates, ADPCM/PCM8/PCM16 from host ARAM via gc_hw.cpp
  `GcAramHostBase`, polyphase/linear SRC, signed envelope, LPF, 9-bus mixer, AUX A/B, compressor, 0x280-byte AI frame).
  `kGameCubeAx` (ax_internal.h) selects it; the Wii path is dead code. It mixes inline on the guest thread (no worker),
  sends no sync mails. Evidence lines: `[ax-gc] ... running voices` (lists 1, 100, then every 1000) and `[meter]`.
  Verified 2026-10-16 (autotest\axgc2.err.log, 75 s): main-menu music = 2 voices, [meter] output peak 12-15k of
  32767, 12000 lists in ~60 s of audio (keeps real time). User-checked in W0L1 (music, SFX, streamed voice lines).
  ITD and DPL2 AUXB routing are skipped, as in Dolphin.
- 48 KHZ OUTPUT (default; env SCOOBY_AX_32K=1 = the DSP's own 32 kHz for A/B): the 32 kHz mix stays exact for all
  guest-visible state; each voice is ALSO rendered from its decoded source samples straight to 48 kHz (16-tap
  Blackman sinc, HiSrc in ax_mix.cpp, ~1 ms latency), and MAIN L/R minus the voices (reverb returns, surround fold) is
  upsampled once and added; audio.cpp pushes that frame (OutputSampleRate / HiRateFrame). Log: `[ax-hirate] ... ok`.
  Compressor gain (CMD 0x12) is not applied to the 48 kHz voices.
- Voice-line streams (STREAM\*.DSP, SOUND_ProcessStreams) need ARQPostRequest_Gc (gc_hw.cpp) to fill the ARQRequest:
  SOUND_UpdateStreamCB picks its stream by request->owner. Unfilled, every refill went to stream 0 and Shaggy's
  W0L1 line looped its first 25.6 KB (fixed 2026-10-16).
- DVD streaming audio (DVDLowAudioStream, STREAM folder) and Bink video (VIDEO folder) untested.
- Wii-only code paths (NAND, IOS, WPAD, netplay, SPM map sweep) are still compiled in; their hooks are disabled.
- Pads: Player 1 input path (virtual_remote.cpp) was built for SPM's Wii-remote layout; GameCube games read PADRead -
  verify once the game reaches the menu.
- Widescreen/4K/120 fps: the runtime features exist (Config.toml [video] aspect_mode, resolution_multiplier,
  frame_interpolation_fps) but nothing is verified on this game yet.
