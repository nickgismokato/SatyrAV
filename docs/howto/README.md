# How-to guides

For people who write and run shows with SatyrAV. Start with the first-time guide; the rest are for looking things up.

| Guide | Read it when you want to… |
|---|---|
| [First time](firstime.md) | Get from nothing to a running show |
| [Scene files](scene.md) | Write cues: every command by what it does, what clears what, ready-made examples |
| [Controls](controls.md) | Know the keys during a show, the popups, the project scanner |
| [Projects](project.md) | Set up a project: folders, file names, `schema.toml`, akter and scenes |
| [Options](options.md) | Change how things look: fonts, colours, outline, background, particles, the Options screen |
| [How a show fits together](structure.md) | Understand scene changes, loading, building a show your way, and the show log and subtitles |

**Quick answers**

| Question | Answer |
|---|---|
| The picture should stay while the text changes | `clearText`, not `clear` — [example](scene.md#common-patterns) |
| Something should survive a `clear` or a scene change | Put it in a `group NAME{…}` — [example](scene.md#common-patterns) |
| Lines jump up as more are added | `FixedTextCentre = true` — [options](options.md#text) |
| A translation under the text | `textD "…", "…"` — [text](scene.md#text) |
| A cue shows its own text instead of working | A misspelled command — press `C` |
| A picture is late the first time | List it in `[Preload]` — [loading](structure.md#what-satyrav-loads-and-when) |
| Subtitles for the recording | Options → Show Log & Subtitles — [details](structure.md#the-show-log-and-subtitles) |
| Something went wrong on stage | Press `O`, fix it after the show from `fix-it/` — [controls](controls.md#marking-a-line-to-fix-later) |

Words like *master*, *slave*, *akt* and *cue* are explained in the [docs overview](../README.md#words-used-throughout). The full detail of every command, for developers, is in [dev/commands.md](../dev/commands.md).
