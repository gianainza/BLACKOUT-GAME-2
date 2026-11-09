# T10 - State machine  (R4)

States: `Title`, `Playing`, `Paused`, `Won`, `Lost` (enum `GameState`, `Game.h`). Code: `Game::Update`, `Game.cpp`.

| from | event / condition | to | in code |
|---|---|---|---|
| Title | Enter | Playing (Reset) | `Game::Update` case Title |
| Playing | P | Paused | `Update`, "if (in.pause)" |
| Paused | P | Playing | case Paused |
| Playing | R | Playing (Reset, same level) | "if (in.restart)" |
| Playing | droid within `exit_zone.radius` of the exit **and the door is unlocked** (`!doorLocked`) | Won | end of `Update` |
| Playing | a strike lands within `strike.radius` of the droid (0.8 s after lock-on) | Lost | `UpdateScanners`, state Locked -> Striking |
| Won | R or Enter | Playing (level + 1, lights `speed_step` faster) | case Won |
| Lost | R or Enter | Playing (same level) | case Lost |

A light's **detection reaching 1.0 does not lose the game**; it LOCKS that light on. The strike lands `strike.delay` seconds later (0.8 s, shortened per level down to `min_strike_delay`), and only then is the droid tested. (The brief says the meter filling is the loss; see README "Deviations".)

Light state machine (per drone / scanner): `Scanning -> Locked -> Striking -> Cooling -> Scanning`.

Transitions that are **impossible**: Title -> Won/Lost; Paused -> Won/Lost (the world is frozen); Won <-> Lost; Playing -> Title (there is no way back to the title without quitting).

**The door keypad is not a game state.** Typing digits, Backspace and Enter at a locked door happen *inside* `Playing` (`Game::KeypadActive()`), and the world keeps running, so no transition was added and the table above is unchanged. A right code sets `doorLocked = false`; a wrong code clears the entry, raises every light's detection and sends the nearest drone to the door.
