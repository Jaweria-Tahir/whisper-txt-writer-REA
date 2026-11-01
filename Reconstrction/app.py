import os
import subprocess
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox

BASE_DIR = Path(__file__).resolve().parent

# Find the executable
EXE_OPTIONS = [
    BASE_DIR / "build" / "Release" / "speech_txt.exe",
    BASE_DIR / "build" / "speech_txt.exe",
]
EXE = next((p for p in EXE_OPTIONS if p.is_file()), EXE_OPTIONS[0])

# Find the model
MODEL_OPTIONS = [
    BASE_DIR / "models" / "ggml-base.en.bin",
    BASE_DIR / "ggml-base.en.bin",
    BASE_DIR / "whisper.cpp" / "models" / "ggml-base.en.bin",
]
MODEL = next((p for p in MODEL_OPTIONS if p.is_file()), None)

# Folders that may hold whisper.dll and ggml.dll
DLL_DIRS = [
    EXE.parent,
    BASE_DIR / "build" / "whisper_build" / "bin" / "Release",
    BASE_DIR / "build" / "whisper_build" / "src" / "Release",
    BASE_DIR / "build" / "whisper_build" / "ggml" / "src" / "Release",
    BASE_DIR / "build" / "bin" / "Release",
]

EXIT_HINTS = {
    2: "Bad or unsupported WAV file. Use a 16-bit PCM WAV.",
    3: "The model could not be loaded. Check the model file.",
    4: "Whisper failed to process the audio.",
    6: "No speech was recognized. The audio may be silent, too short, or in another language.",
    3221225781: "A DLL is missing next to the .exe (whisper.dll or ggml*.dll).",
}


def browse_audio():
    path = filedialog.askopenfilename(filetypes=[("WAV Audio", "*.wav")])
    if path:
        audio_entry.delete(0, tk.END)
        audio_entry.insert(0, path)
        # Suggest an output file next to the audio
        if not output_entry.get().strip():
            output_entry.insert(0, str(Path(path).with_suffix(".txt")))


def browse_output():
    path = filedialog.asksaveasfilename(
        defaultextension=".txt", filetypes=[("Text Files", "*.txt")]
    )
    if path:
        output_entry.delete(0, tk.END)
        output_entry.insert(0, path)


def transcribe():
    audio = audio_entry.get().strip()
    output = output_entry.get().strip()
    language = language_entry.get().strip() or "en"

    if not EXE.is_file():
        messagebox.showerror("Error", f"Executable not found:\n{EXE}")
        return

    if MODEL is None:
        messagebox.showerror(
            "Model not found", "Place ggml-base.en.bin inside the models folder."
        )
        return

    if not Path(audio).is_file():
        messagebox.showerror("Error", "Please select a valid WAV file.")
        return

    if not output:
        messagebox.showerror("Error", "Please select an output TXT file.")
        return

    output_base = str(Path(output).with_suffix(""))
    output_file = Path(output_base + ".txt")

    # Delete any old (possibly empty) output file
    try:
        if output_file.exists():
            output_file.unlink()
    except OSError as e:
        messagebox.showerror("Error", f"Cannot replace the old file:\n{e}")
        return

    transcribe_button.config(state="disabled")
    status_label.config(text="Transcribing... Please wait.", fg="orange")
    result_box.delete("1.0", tk.END)

    def run_process():
        try:
            env = os.environ.copy()
            extra = [str(d) for d in DLL_DIRS if d.is_dir()]
            env["PATH"] = os.pathsep.join(extra + [env.get("PATH", "")])

            command = [
                str(EXE),
                "-m", str(MODEL),
                "-f", audio,
                "-of", output_base,
                "-l", language,
            ]

            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                cwd=str(EXE.parent),
                env=env,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )

            transcription = ""
            if output_file.is_file():
                transcription = output_file.read_text(
                    encoding="utf-8", errors="replace"
                )

            def finish():
                transcribe_button.config(state="normal")

                if result.returncode == 0 and transcription.strip():
                    result_box.insert(tk.END, transcription)
                    status_label.config(text="Transcription completed!", fg="green")
                    messagebox.showinfo(
                        "Success", f"Text saved successfully:\n{output_file}"
                    )
                else:
                    log = (result.stderr or "") + (result.stdout or "")
                    hint = EXIT_HINTS.get(result.returncode, "Unknown error.")
                    if result.returncode == 0:
                        hint = "The output file is empty. No text was recognized."
                    msg = (
                        f"Exit code: {result.returncode}\n"
                        f"{hint}\n\n"
                        f"Details:\n{log.strip() or '(no output)'}"
                    )
                    result_box.insert(tk.END, msg)
                    status_label.config(text="Transcription failed.", fg="red")
                    messagebox.showerror("Transcription Error", msg[-2500:])

            root.after(0, finish)

        except Exception as e:
            err = str(e)
            root.after(0, lambda: show_error(err))

    threading.Thread(target=run_process, daemon=True).start()


def show_error(error):
    transcribe_button.config(state="normal")
    status_label.config(text="Error occurred.", fg="red")
    result_box.insert(tk.END, error)
    messagebox.showerror("Application Error", error)


# GUI window
root = tk.Tk()
root.title("Speech to Text Converter")
root.geometry("680x520")
root.configure(bg="#f2f4f7")
root.resizable(False, False)

tk.Label(
    root,
    text="Speech to Text Converter",
    font=("Arial", 21, "bold"),
    bg="#f2f4f7",
).pack(pady=20)


def create_row(label):
    frame = tk.Frame(root, bg="#f2f4f7")
    frame.pack(fill="x", padx=25, pady=9)

    tk.Label(frame, text=label, width=13, anchor="w", bg="#f2f4f7").pack(side="left")

    entry = tk.Entry(frame, width=48)
    entry.pack(side="left", padx=5)

    return entry, frame


audio_entry, audio_frame = create_row("Audio File:")
tk.Button(audio_frame, text="Browse", command=browse_audio).pack(side="left")

model_entry, model_frame = create_row("Whisper Model:")
model_entry.insert(0, str(MODEL) if MODEL else "Model not found")
model_entry.config(state="readonly")

output_entry, output_frame = create_row("Save TXT:")
tk.Button(output_frame, text="Browse", command=browse_output).pack(side="left")

lang_frame = tk.Frame(root, bg="#f2f4f7")
lang_frame.pack(pady=10)

tk.Label(lang_frame, text="Language:", bg="#f2f4f7").pack(side="left")

language_entry = tk.Entry(lang_frame, width=15)
language_entry.insert(0, "en")
language_entry.pack(side="left", padx=10)

transcribe_button = tk.Button(
    root,
    text="TRANSCRIBE",
    command=transcribe,
    bg="#2563eb",
    fg="white",
    font=("Arial", 12, "bold"),
    width=22,
    height=2,
)
transcribe_button.pack(pady=15)

status_label = tk.Label(root, text="Ready", bg="#f2f4f7", fg="green")
status_label.pack()

result_box = tk.Text(root, height=9, width=78)
result_box.pack(padx=20, pady=15)

root.mainloop()