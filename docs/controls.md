# Controls

## General (all screens)

| Key | Action |
|-----|--------|
| H | Toggle help popup |
| D | Toggle debug popup |
| ESC | Go back / quit |
| ENTER | Select / confirm |
| TAB | Next field (in forms) |
| UP / DOWN | Navigate |

## In menus

| Key | Action |
|-----|--------|
| UP / DOWN | Select option |
| ENTER | Confirm selection |
| ESC | Return to main menu |

## In a project (Performance screen)

### Navigation

| Key | Action |
|-----|--------|
| RIGHT | Enter akt → scenes → commands |
| LEFT | Go back one column |
| UP / DOWN | Move selection |
| SHIFT+UP | Jump 5 items up |
| SHIFT+DOWN | Jump 5 items down |
| CTRL+UP | Jump to top |
| CTRL+DOWN | Jump to bottom |

### Execution

| Key | Action |
|-----|--------|
| ENTER | Execute current command |
| SHIFT+ENTER | Go to previous command |

### Scene transitions

At the last command in a scene, pressing ENTER executes the command. A status bar appears: `>> Press ENTER for next scene: SceneName >>`. Pressing ENTER again advances to the next scene where the first command is primed. Pressing ENTER a third time executes the first command of the new scene.

### Media control

| Key | Action |
|-----|--------|
| K | Toggle video pause |
| M | Toggle audio/music pause |

### Scene files

| Key | Action |
|-----|--------|
| U | Reload every `.ngk` scene file in the current project in place — no need to exit and reload the project |

Useful when iterating on scene content from an external editor: save the `.ngk`, switch back to SatyrAV, press `U`.

## Popups

Both the help and debug popups are draggable — click and drag the title bar. The program continues running underneath. Press H or D again to close, or press the same key to toggle.
