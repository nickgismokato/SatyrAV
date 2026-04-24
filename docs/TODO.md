# Roadmap

Future work, newest first. Shipped features live in [CHANGELOG.md](CHANGELOG.md).

The lists, with the exception of the bug list, is in no particular order. Some have the (**Critical**) on them which means they are important and should be looked at first in the update version, before the others, if it makes sense.

Bugs aren't tied to specific versions. It is just a list of all known bugs which we currently have to fix at some point. Elements on the bug lists might not be bugs but specific things we have to check such that we know this won't become a bug later on, or may be a current bug we haven't tested for.

---

## Planned for 2.1.0

- OSC Receiver, i.e. listen for OSC messages from lightning desk to control cues. Mainly interested in QLab and maybe GrandMA. 
- Expose HTTP/WebSocket endpoint to send out message of which Scene and akt we are currently on.
- Create full physics model for particle systems, accelerated with hardware
- A full scanner of a project to see if any scenes inside its `.ngk` files have wrong options/notations/commands. A full scanner to check for existence of assets should also be implemented.

---

## Planned for 2.0.0

**General notes about this major update:** This update will essentially be a big backend update, which means the functionality of the software shouldn't change. By upgrading to `SDL3` we will get a backend that can handle more complex backend tasks, be more optimised and also add in features we wants but is limited in `SDL2`. Like full hardware encoding/decoding. 

- Update `SDL2`&rarr;`SDL3`.
	- Utilize `SLD3 GPU API` to more efficiently utilize D3D11 and also add support for Vulkan and OpenGL. Furthermore, it is the hope that there is better support for integrated GPU's.
	- Utilize `SLD3 Filysystem API`. This will probably mainstream the file pipeline and also make it more maintainable. More research for this specific `API` is needed.
	- Utilize `SDL3 Audio Streams` to have a proper audio buffer and a gaining option for audio playback.
	- Utilize `SDL3 Logical Audio Devices` to handle audio devices.
	- Utilize `SDL3 Process API` to handle child processes simple and within the `SDL` environment.
	- Maybe utilize `SDL3 Async Windowing` to handle all other displayes better. Require `SDL3.2.0`.
		-An Async video is something we already have, essentially, but this will be a dedicated version which should remain thread safe if called on the main thread.
	- Utilize `SDL3 System Tray API` to create custom system and tray icons/menus.
	- Utilize `SDL3 Time API` to handle time different from ticks and and performance counters.
		- This means we can have a clock inside our project for easier tell of time. Also add a stopwatch command for scenes, to time individual scenes.
	- All new `SDL3` features can be found here: https://wiki.libsdl.org/SDL3/NewFeatures. Will have to investigate if more features is needed or some shouldn't be used at all.
- Redo hardware playback of videos for full hardware encoding.
	- Remove the copy of VRAM&rarr;RAM in the final step, making it essential zero copy when the GPU have gotten the data.
- Make it clear in the debug if Hardware encoding/decoding is enabled or if it has fallen back to Software encoding/decoding

---

## Planned for 1.7.0

- Make a hierarchy for pictures such that you can command what pictures should be in front and what shouldn't.
	- Currently the last displayed image will automatically be in front of all previous pictures.
- Make a fastforward-goback scroller for videos.
	- Probably should just go forward or backward with 5 seconds interval using key `,`/`.`.
	- A restart from the beginning function is also needed.
- Add `pathMove` command to dictate multiple movements of objects.
	- A option for repeating movement will also be added.
- (**Critical**) Add **UnitTesting** to more reliable testing of backend.
- Deal with the many bugs of the Windowed version of the `Slave Display`
- Create a better first time `example.ngk` when creating a new project
	- This should have two "akter".
	- This should have four scenes:
		1. One for text showcase
		2. One for picture showcase
		3. One for particle showcase
		4. One for group/macro showcase
- (**Critical**) Make sure all created `schema.toml` include all project options and also are at default value.
	- Remove some bloated comments
- Create a true folder chooser for folder picker inside option.
	- This means when pressing the folder choose for where projects should be, a folder "pop-up" should be opened and from here the user should be able to choose which folder they want to use for storage of their projects.
- In `docs/building.md` create a troubleshoot section for most common problems.
- (**Critical**) Add simple $\LaTeX$ symbols for Greek letters.
- (**Critical**) Add a fourth windows inside our main display when you have chosen a scene. This will be a `Cue Notes` screen.
	- It should essentially look like our command list but we cannot interact with it.
	- It should follow the command screen as we scroll up or down.
	- Inside it, there is specific notes to a specific command and it should always be on the right of the command.
	- A new `[Cue]` option should be given inside the `.ngk` files, where you can write notes. Here is an example:
	```txt
	[Cue]
	Cue1 Play audio when Actor leaves the scene
	Cue2 Stop audio when Actor begin screaming from outside the scene

	[Commands]
	show backgroundImage.png
	play Music.mp3 \Cue1
	stop \Cue2
	```
	- The Cue will have to makes "newlines" if the cue notes is to big.
- Add typewriter effect to write text to secondary display one character at a time.
	- Command: `typewrite(MS_PER_CHAR, "text")`
- Add a countdown display option that begins a text countdown looking like this: `Countdown: M:s`
	- Option for danish language
	- Always show minutes and seconds. Maximum `59:59`.
	- Command: `countdown(SECONDS)`
---

## Planned for 1.6.0

- (**Critical**) Add wordwrap/newline for very large commands. Currently it just cut through the window and cannot be read. 
- (**Critical**) Transparent-layer handling for `.png` files. Today the alpha channel in a PNG is composited onto the slave background as normal. Add a per-project `[Options]` toggle (default **off**) that switches the slave to a transparent composite path — the underlying pixels show through PNG alpha instead of being filled by the scene background. Needs investigation of how this interacts with the targeted-display black-fill and with SDL's window transparency flags.
- (**Critical**) Redo the `Load Project` page to be more organized, having dedicated filters and search feature.
- Overview Debug:
	- This should give an overview for a project if it is missing files, a scene has an invalid command or option.
	- Use the key `O` inside the debug menu to open it.
- Make debug menu have a clear discription that you can use `S` and `O` to open other menus.
- (**Critical**) Make the pausing of a video with `K`, freeze the last frame and hold it instead of just shutting down completely.
- Add a global logger for `SatyrAV` and put it in a `log` folder parallel to the `project` folder
- Create a custom extension for VSCode for `.ngk`.
- (**Critical**) Add a bottom text + middle text `textD` which add a italic text at the bottom of the screen. This should take two inputs. The actual text you would normally write with the command `text "<text>"` and a secondary text.
	- This will be used to write english subtitles. For example:
	```txt
	textD "Vi har started nu!", "We have started now!"
	```
	- The secondary text should be italic and also be at the bottom of the screen. I will figure out how low from the bottom.
	- This will be used to display english text at the bottom of the screen to have "translations" for our AV for the few english students in the audience. 
	- If only the first text is given, it should just works like the normal `text` command.
	- The bottom text should be in italic as default (*can be undone in project options*) and be in a grey colour, maybe at 95% transparency. The latter is not important.
- Add option for *Italic* and **Bold** text by using `textbf` and `textit` commands.
- (**Critical**) Add option to choose which audio device you want to output sound from.
	- A list of all audio devices should be in the `Options` page. It should essentially look like how our Secondary Display option looks like currently.
- Add option to make your function keys `F1-F12` works as a audio controller to play audio.
	- Essentially in you project file `Schema.toml` there should be an option to assign function keys to play audio. Like if you want to have a door knock/bell as an audio to easily play with a single button throughout all acts and scenes.

---

## Known bugs

Most critical first:

- Scaling via the Debug menu will make the third output screen/monitor have less resolution when zooming in too much.
	- Essentially, making the secondary target display too small, will make the output for the third monitor more "pixelated". This, I think, is because of how the resolution is rendered from the second to the third screen. The optimal rendering would be that the third screen display the secondary screen "pre" target display, essentially the resolution of the secondary display.
- Maybe only one image can be shown at a time. This need to be tested.
- Not a bug, but we have to check what bit our movies is compatible with. 8/10 and 16 bit should all be suported.
- Windowed mode of the `Slave Display` isn't truly a scale of one to one with the full screen.
	- When pressing `C`, it does default but doesn't actually fully exand. It depends on the scale of the window.
	- Some pictures will not fill out the window.
	- The `pos` command gets a little "*wonky*" when having the windows not in a $x:y$ aspect ratio where $x\geq y$.
- `docs/building.md` need to be updated. We don't need to move our files anymore, but should still be there as a troubleshoot step.
 - `Shift+ENTER` doesn't undo last command. It simply just jump back to the last command.