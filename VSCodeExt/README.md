# SatyrAV NGK — VSCode Extension

Syntax highlighting for SatyrAV scene files (`.ngk`).

Highlights:

- Section headers — `[Options]`, `[Preload]`, `[Commands]`, `[Hotkeys]`, `[RevyData]`, `[Structure]`, `[Display]`, `[Particle.<TYPE>]`.
- Command keywords — `text`, `textCont`, `textbf`, `textit`, `textbfCont`, `textitCont`, `textD`, `clear`, `clearText`, `clearImages`, `clearAll`, `play`, `stop`, `stopParticleCont`, `show`, `comment`.
- Modifier wrappers — `color(...)`, `trans(...)`, `rotate(...)`, `move(...)`, `spin(...)`, `pos(...)`, `size(...)`, `particle(...)`, `randomImages(...)`, `randomSize(...)`, `bold(...)`, `it(...)`, `trail(...)`, `trailGlow(...)`, `bounce(...)`.
- Block keywords — `group NAME{...}`, `loop(count, every){...}`.
- Particle / distribution constants — `RAIN`, `SNOW`, `CONFETTI`, `RISE`, `SCATTER`, `BROWNIAN`, `OSCILLATION`, `ORBIT`, `NOISE`, `LIFECURVE`, `GHOST`, `STRETCH`, `GLOW`, `UNIFORM`, `NORMAL`, `LOGNORMAL`, `BINOMIAL`, `BERNOULLI`, `GAMMA`.
- String literals (`"..."`), numbers, line comments (`# ...`).

This is the 1.6.3 baseline — syntax highlighting only. Auto-completion, diagnostics, and snippets are intentionally deferred to a later cycle.

## Install — option 1: copy folder (no tooling needed) ★ recommended

VSCode auto-loads any folder dropped into its `extensions` directory. No npm, no `vsce`, no Node version requirements.

**Windows (cmd or PowerShell):**

```cmd
xcopy /E /I /Y VSCodeExt %USERPROFILE%\.vscode\extensions\satyrav-ngk-0.1.0
```

**Linux / WSL / macOS:**

```bash
cp -r VSCodeExt ~/.vscode/extensions/satyrav-ngk-0.1.0
```

Restart VSCode. Open any `.ngk` file — the grammar will be active.

> Running this command from WSL? VSCode on Windows reads from `%USERPROFILE%\.vscode\extensions` (the Windows side), not your WSL `~/.vscode`. Use the Windows command above, or copy from inside Windows: the `VSCodeExt/` folder lives at `C:\Users\<you>\Documents\projects\satyr\SatyrAV\VSCodeExt`.

## Install — option 2: F5 dev-load (one-shot, debugging)

1. Open the `VSCodeExt/` folder in VSCode (`File → Open Folder…`).
2. Press `F5` to launch a new Extension Development Host window.
3. In that window, open any `.ngk` file. The grammar is active for that session only.

Useful while editing the grammar — `Ctrl+R` reloads the host window so you see changes immediately.

## Install — option 3: vsix package (advanced)

`@vscode/vsce` (the packaging CLI) requires Node 20+ as of v3.x. If you're on Node 18 you'll hit `ReferenceError: File is not defined` when running `vsce`. Two ways out:

**Upgrade Node to 20+** (best long-term — installer at https://nodejs.org/), then:

```bash
cd VSCodeExt
npm install -g @vscode/vsce
vsce package          # produces satyrav-ngk-0.1.0.vsix in this folder
code --install-extension ./satyrav-ngk-0.1.0.vsix
```

**Stick with Node 18** by pinning `vsce` to the last Node-18-compatible release:

```bash
cd VSCodeExt
sudo npm install -g @vscode/vsce@2.32.0
vsce package
code --install-extension ./satyrav-ngk-0.1.0.vsix
```

> If the `code --install-extension` step prints `ENOENT: no such file or directory`, it means `vsce package` didn't actually produce the `.vsix` — check the previous command's output. From WSL, prefix the path with `./` (or use the absolute Windows path: `code --install-extension "C:\Users\you\…\satyrav-ngk-0.1.0.vsix"`).

For most people, **option 1 is the path of least resistance** — it's a single copy with no toolchain. Use option 3 only if you want a redistributable `.vsix` to share.

## Files

- `package.json` — extension manifest (language registration).
- `language-configuration.json` — comment / bracket / auto-closing pair config.
- `syntaxes/ngk.tmLanguage.json` — TextMate grammar (the actual highlighting rules).

Edit any of these and reload (`Ctrl+R` in the Extension Development Host, or restart VSCode for option-1 installs) to see the change.
