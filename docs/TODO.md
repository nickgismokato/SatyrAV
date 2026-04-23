# Roadmap

Future work, newest first. Shipped features live in [CHANGELOG.md](CHANGELOG.md).

---

## Planned for 1.6.0

- Transparent-layer handling for `.png` files. Today the alpha channel in a PNG is composited onto the slave background as normal. Add a per-project `[Options]` toggle (default **off**) that switches the slave to a transparent composite path — the underlying pixels show through PNG alpha instead of being filled by the scene background. Needs investigation of how this interacts with the targeted-display black-fill and with SDL's window transparency flags.

---

## Known bugs

Most critical first:

- None reported.
