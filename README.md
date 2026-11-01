# Reverse Engineering whisper.cpp with REA and Ghidra


## 1. Project Overview

This project studies a compiled program (`whisper-cli.exe` from **whisper.cpp**) without its source code. I used **REA** (an AI-assisted reverse-engineering toolkit) with **Ghidra** to find out how the program saves recognized speech into a `.txt` file.

**Feature studied:** saving recognized speech into a `.txt` file.

---

## 2. Task 1 – Understanding REA

**Reverse Engineering** means studying an existing product or software to understand how it works inside.

**REA (Reverse Engineering Anything)** is a helper that does the hard digging for you:

- Investigates how software works, even without source code.
- Connects AI agents to analysis tools using **MCP (Model Context Protocol)**.
- Gives technical findings with proof (code, functions, files).
- Can help rebuild a feature after studying its behaviour.

### Capabilities

| No. | Capability | Description |
|---|---|---|
| 1 | Static Analysis | Checks code and files without running the program |
| 2 | Binary Analysis | Finds functions, instructions and strings in compiled programs |
| 3 | Decompilation | Turns compiled code into readable code |
| 4 | JavaScript Analysis | Finds JS modules, imports and functions |
| 5 | Electron Analysis | Investigates Electron apps |
| 6 | Website Analysis | Checks website structure, scripts and network behaviour |
| 7 | Android Analysis | Investigates APK files, classes and methods |
| 8 | .NET Analysis | Checks .NET assemblies |
| 9 | Runtime Analysis | Watches the app while it runs |
| 10 | Evidence Collection | Collects proof for the findings |

### Prerequisites

1. Operating System (Windows, Linux or macOS)
2. Node.js
3. npm
4. An MCP-compatible AI agent (Claude Code, Codex, Cursor, etc.)
5. A target application to investigate

### Dependencies

MCP, Ghidra, IDA Pro, Hopper, JADX, JDK, Chrome Browser, ADB, Binwalk / Unblob, WABT.

### Flow

```
AI Agent  →  MCP  →  REA Analysis  →  Findings and Evidence
```

---

## 3. Task 2 – Installation and Configuration

### Step 1: Install JDK
Ghidra is written in Java, and REA needs both `java` and `javac`. I used **JDK 21 (Eclipse Adoptium)**.

### Step 2: Install Ghidra
Install **Ghidra 12.1.4**.

### Step 3: Set the paths (PowerShell)

```powershell
$env:GHIDRA_INSTALL_DIR = "C:\tools\ghidra_12.1.4_PUBLIC"
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-21"
```

### Step 4: Check versions

```powershell
node -v
javac -version
```

### Step 5: Install and set up REA

```powershell
npm install --global rea-agents
rea setup
```

### Step 6: Configure REA for Claude

```powershell
rea setup --client claude_desktop
rea doctor --client claude_desktop --json
```

### Issues Faced and Fixes

| Issue | Fix |
|---|---|
| Ghidra 12.2 was not accepted. REA only works with Ghidra 12.1.x. | Installed **Ghidra 12.1.4** |
| REA setup refused to run. Node.js v24.14.0 was older than the required 24.22.1. | Installed **Node.js v26.13.0** |

---

## 4. Task 3 – Application Selected: whisper.cpp

**Whisper** is OpenAI's speech-recognition model. **whisper.cpp** is software that runs the Whisper model in C++.

### Set up whisper.cpp

```powershell
git clone https://github.com/ggml-org/whisper.cpp
cd whisper.cpp
cmake -B build
cmake --build build -j --config Release
.\models\download-ggml-model.cmd base.en
```

### Run it on a sample file

```powershell
.\build\bin\Release\whisper-cli.exe -m ggml-base.en.bin -f samples\jfk.wav
```

### Run the web server (optional)

```powershell
.\build\bin\Release\whisper-server.exe -m ggml-base.en.bin --port 8080
```

Then open: http://127.0.0.1:8080/

I used a `.wav` file and converted it into text.

---

## 5. Investigation

### Prompt given to Claude (with REA tools)

> Use the REA tools with the Ghidra provider on `whisper-cli.exe`. Feature: saving recognized speech into a .txt file. Find the strings `output_txt`, `saving output to`, `failed to open`. Show which functions use them. Explain the flow from "text is ready" to "file written". Create a Word document with the investigation. Name the REA tool behind each fact.

### REA tools used

| Tool | Purpose |
|---|---|
| `open_binary` | Opened `whisper-cli.exe` |
| `binary_overview` | Showed sections, 912 functions, 630 strings |
| `search_strings` | Found the `.txt` output strings |
| `xrefs` | Showed which code uses each string |
| `analyze_function` | Summarized `FUN_140006760` |
| `procedure_pseudo_code` | Showed decompiled code of `FUN_140010c20` and `FUN_1400095a0` |
| `inspect_pe_imports` | Listed DLLs and imported functions |

### Technology stack found

- **File type:** 64-bit Windows program (PE32+, x86-64), 479,744 bytes
- **Image base:** `0x140000000`
- **Language:** C++ with the Microsoft Visual C++ runtime (`MSVCP140.dll`, `VCRUNTIME140.dll`, Universal CRT)
- **Speech engine:** `whisper.dll` (37 imported functions, e.g. `whisper_full_parallel`, `whisper_full_get_segment_text`)
- **Math library:** `ggml.dll` (`ggml_backend_load_all`)
- **Windows functions:** `CreateFileA`, `ReadFile`, `CloseHandle`, `LoadLibraryA`, `GetProcAddress`
- **File writing:** `std::basic_ofstream`, `fopen_s`, `fwrite`, `fclose`, `fflush`
- **Analysis tool:** Ghidra 12.1.4 via the REA provider

### Main features of whisper-cli

- Loads an AI model from a file
- Reads audio files
- Converts speech to text or translates it
- Supports many languages and speaker-separation options
- Saves results as `.txt`, `.vtt`, `.srt`, `.csv`, `.json`
- Can also show results on screen

---

## 6. Findings: Saving to .txt

### Strings found

| Address | Text | Meaning |
|---|---|---|
| `0x140060db8` | `-otxt` | Short command option |
| `0x140060dc0` | `--output-txt` | Long form of the option |
| `0x140061a60` | `-otxt, --output-txt ...` | Help text |
| `0x140063388` | `output_txt` | Label used in the main function |
| `0x140062e98` | `%s: failed to open '%s' for writing` | Error message |
| `0x140062ec0` | `%s: saving output to '%s'` | Success message |
| `0x140063430` | `.score.txt` | Separate confidence-score file (not the transcript) |

### Functions involved

| Function | Role |
|---|---|
| `FUN_140010c20` | **Main function.** Reads options, loads the model, runs `whisper_full_parallel`, calls the save functions |
| `FUN_140006760` | **File opener.** Creates the output file, adds the extension, prints success or error message |
| `FUN_1400095a0` | **Text writer.** Gets each segment, reads its text, writes it to the file with a new line |

*(Ghidra default names are used because the program has no original function names.)*

Whisper DLL functions used: `whisper_full_n_segments`, `whisper_full_get_segment_text`, `whisper_full_get_segment_t0` / `t1`.

### Input and output

- **Writer input:** recognized speech data, an open file, settings, audio channel info
- **Opener input:** file name, `.txt` extension, label `output_txt`
- **Output:** a `.txt` file with one segment per line
- **Messages:** printed to `stderr`
- **Return value:** opener returns `true` if the file opens

### Workflow

1. **Audio processing:** `whisper_full_parallel` converts audio to text. On failure, an error is printed.
2. **Check option:** main checks if `-otxt` is enabled.
3. **Open file:** `FUN_140006760` creates `audio-name.txt`.
4. **Check file:** if it opens, prints "saving output to". Otherwise prints an error.
5. **Write text:** `FUN_1400095a0` writes one segment per line.
6. **Speaker labels:** if diarization is on and there are two channels, a speaker prefix may be added (inferred from analysis).
7. **Close file:** the file closes and the program moves on to the next format or audio file.

---

## 7. Evidence (Screenshots)

Add your screenshots in an `images/` folder and link them here:

1. Opened `whisper-cli.exe` in Ghidra
2. Program sections
3. Strings linked to `.txt` saving
4. `FUN_140006760` (file opener) – listing and decompiler
5. `FUN_1400095a0` (text writer)
6. Xrefs for `output_txt`

```markdown
![Opened binary](images/01-open-binary.png)
```

---

## 8. Tools and Resources

- REA: https://github.com/morluto/rea
- Ghidra 12.1.4 (binary analysis)
- whisper.cpp: https://github.com/ggml-org/whisper.cpp
- Claude Code / ChatGPT for understanding results and organizing the report
