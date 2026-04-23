# Roadmap

Future work, newest first. Shipped features live in [CHANGELOG.md](CHANGELOG.md).

---

## Planned for 2.0.0

- Support for OpenGL/Vulkan.
- Redo hardware playback of videos for full hardware encoding.
	- Remove the copy of VRAM&rarr;RAM in the final step, making it essential zero copy when the GPU have gotten the data.
	- Still have software as a fallback if no GPU is on the target machine.
- Make it clear in the debug if Hardware encoding/decoding is enabled or if it has fallen back to Software encoding/decoding
- Create full physics model for particle systems, accelerated with hardware
- Upgrade to SDL3

---

## Planned for 1.7.0

- Make a hierarchy for pictures such that you can command what pictures should be in front and what shouldn't.
	- Currently the last displayed image will automatically be in front of all previous pictures.
- Make a fastforward-goback scroller for videos.
	- Probably should just go forward or backward with 5 seconds interval using key `,`/`.`.
	- A restart from the beginning function is also needed.
- Add `pathMove` command to dictate multiple movements of objects.
	- A option for repeating movement will also be added.
- Add **UnitTesting** to more reliable testing of backend.
- Deal with the many bugs of the Windowed version of the `Slave Display`
- Create a better first time `example.ngk` when creating a new project
	- This should have two "akter".
	- This should have four scenes:
		1. One for text showcase
		2. One for picture showcase
		3. One for particle showcase
		4. One for group/macro showcase
- Make sure all created `schema.toml` include all project options and also are at default value.
	- Remove some bloated comments
- Create a true folder chooser for folder picker inside option.
	- This means when pressing the folder choose for where projects should be, a folder "pop-up" should be opened and from here the user should be able to choose which folder they want to use for storage of their projects.
- In `docs/building.md` create a troubleshoot section for most common problems.
---

## Planned for 1.6.0

- Transparent-layer handling for `.png` files. Today the alpha channel in a PNG is composited onto the slave background as normal. Add a per-project `[Options]` toggle (default **off**) that switches the slave to a transparent composite path — the underlying pixels show through PNG alpha instead of being filled by the scene background. Needs investigation of how this interacts with the targeted-display black-fill and with SDL's window transparency flags.
- Redo the `Load Project` page to be more organized, having dedicated filters and search feature.
- Overview Debug:
	- This should give an overview for a project if it is missing files, a scene has an invalid command or option.
	- Use the key `O` inside the debug menu to open it.
- Make debug menu have a clear discription that you can use `S` and `O` to open other menus.
- Make the pausing of a video with `K`, freeze the last frame and hold it instead of just shutting down completely.
- Add a global logger for `SatyrAV` and put it in a `log` folder parallel to the `project` folder
- Create a custom extension for VSCode for `.ngk`.

---

## Known bugs

Most critical first:

- Scaling via the Debug menu will make the third output screen/monitor have less resolution when zooming in too much.
- Maybe only one image can be shown at a time. This need to be tested.
- Windowed mode of the `Slave Display` isn't truly a scale of one to one with the full screen.
	- When pressing `C`, it does default but doesn't actually fully exand. It depends on the scale of the window.
	- Some pictures will not fill out the window.
	- The `pos` command gets a little "*wonky*" when having the windows not in a $x:y$ aspect ratio where $x\geq y$.
- `docs/building.md` need to be updated. We don't need to move our files anymore, but should still be there as a troubleshoot step.
 