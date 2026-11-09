# Bot ablation: which new rule makes the level hard

`tests/bot.cpp` plays the real game through `Game::Update` only (walk / sprint / sneak in 8 directions, hold E, type the code,
Enter). It looks 1.2 s ahead by simulating 28 candidate actions and keeps the best one. It switches the lamp off at the start.
Run: `cd SkyboxLab && ./bot --seed N [--set name=value]`. (It tries up to 28 actions per step: 9 directions x walk / sprint / sneak, plus "stand still without holding E" while hacking.) Results are **levels escaped out of 12** (seeds 1, 2, 3 x levels 1-4).

| variant (one change from the shipped level) | escaped |
|---|---|
| **shipped rules** (strike 0.8 s, noise 0.5 / 1.5 / 4.5 / 9 / 5 m, hack 2 s) | **5 / 12** |
| no per-level shortening of the strike delay (`minstrike=0.8`) | 6 / 12 |
| slower level scaling (`step=1.1`) | 6 / 12 |
| no light slowdown (`lit=1.0`) | 6 / 12 |
| hack time 1.0 s | 10 / 12 |
| drone lights 40 % dimmer (`droneR=0.6`) | 11 / 12 |
| **all noise radii halved** (`noise=0.5`) | **11 / 12** |
| **strike delay 1.2 s** (`strike=1.2`) | **11 / 12** |
| only hacking noise 3 m | 6 / 12 |
| only walking noise 3 m | 4 / 12 |
| hacking 3.5 m + sprinting 6 m | 8 / 12 |
| no lamp noise bonus | 5 / 12 |

## What this says
* **The deciding rules are noise (drones flying to where you are) and the 0.8 s strike.** Removing the level scaling or the light slowdown changes almost nothing.
* A single noise number is not enough (the best single change, hacking 3 m, gives 6 / 12); halving **all** the noise radii, or a strike delay of 1.2 s, each give 11 / 12.
* **Why 0.8 s is so tight** (measured with the real `Game::Update` on open floor, scanner locked on while the droid stands under it, droid then runs straight away): a **sprint** ends **+0.57 m** outside the 2.0 m blast; **walking ends 0.28 m inside it** and is incinerated. 0.57 m is only **about 0.09 s of slack** (0.57 m / 6.08 m/s). A human reaction time of 0.2-0.3 s is two to three times that, so in practice only a droid that is already sprinting, or that never gets locked on in the first place, survives.
* A reasonable minimal tuning is **one number**: `gameplay.strike.delay` 0.8 -> 1.2 in `level01.json` (everything else stays as specified). The alternative that keeps 0.8 s is to scale the noise radii by about 0.5.

The shipped level keeps the values you asked for. Nothing was relaxed to make the bot pass.

## Caveat
The bot is one policy, not a measure of human skill. It sees exact light positions and strike timing, but it also cannot read the situation like a person, and its results vary by seed (the table sums three seeds). Treat the table as a ranking of which rule matters, not as a difficulty rating.
