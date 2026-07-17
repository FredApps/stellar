# Gameplay Reference

All numbers below are taken from `src/game.c` / `src/defs.h`. Frame counts assume the
native gameplay pacing of about 35 FPS.

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrows | Move, including diagonals |
| Shift | Boost while held; drains and recharges the boost bar |
| Space | Fire the current main weapon |
| Ctrl | Launch a homing missile; on scores, quick replay |
| B | Fire a smart bomb |
| P | Pause / unpause |
| M | Mute / unmute all sound |
| H | Open / close Help from the title screen |
| Up/Down | Change title difficulty; scroll Help pages |
| Esc | PLAY -> title; TITLE -> quit; high-score entry -> save and replay |

Help is a six-page manual for controls, pickups, weapons, survival, enemies and bosses, and scoring
and difficulty. Enabled Up/Down arrows at the bottom show which direction can be scrolled; Space also
advances to the next page. The browser port provides matching touch buttons outside the playfield.

## Objective

Ayrien Assault is a 60-wave arcade campaign with optional freeplay after victory. Clear waves,
survive boss attacks, graze danger, build combos, earn medals, and push the high-score table.

After game over, the title/scores flow shows last-run stats: wave reached, max combo, and bosses
defeated.

## Difficulty

| Mode | Player Start | Wave Pressure | Score |
|------|--------------|---------------|-------|
| Easy | 5 ships, starting shield, 2 bombs | Slower spawns, slower enemy fire, lower boss HP | x1.00 |
| Normal | 3 ships, 2 bombs | Baseline | x1.25 |
| Hard | 2 ships, 1 bomb | Larger waves, faster spawns/fire, more elites, higher boss HP | x1.60 |

On Easy/Normal, quick repeat deaths grant a short shield as a recovery aid. Easy grants the
longer recovery shield. Hard disables that pity shield.

## Boost

Hold Shift to raise ship speed from 3 to 5 pixels per frame. Boost starts full at 140 energy,
drains 2 per frame, and recharges 1 per frame after a 25-frame delay. It cannot restart until
at least 12 energy is available. The HUD shows the `BST` bar beside missiles and bombs.

During a boss fight, crossing above the boss turns the ship downward. Cannon, Laser, Wave, and
homing missiles all follow the new facing; the ship turns upward again after crossing below the boss.

While Shield is active, body-checking a small enemy destroys it normally. Ramming a boss deals
one tenth of its maximum HP and bounces the ship away; a short cooldown prevents repeated contact
from applying the damage every frame.

## Waves And Enemies

Normal waves spawn `5 + wave` enemies, capped at 18. Hard adds 4 more. Enemies can arrive as a
staggered column, row, shallow V, or alternating weaver formation.

Every four-wave block changes palette and wave personality:

| Block Style | Feel |
|-------------|------|
| Scout rush | More fast divers |
| Weaver maze | More horizontal movement |
| Shooter crossfire | More aimed fire |
| Mixed pressure | Balanced enemy mix |

From wave 7 onward, rare elite enemies appear. Normal elite chance is 12%, rising to 18% after
wave 12 and 22% from wave 20. Easy reduces those chances by 4 points; Hard adds 5 points.
Elite enemies are identified in play by a blinking cyan/white box around their ship.
Enemy fall speed gains a step after wave 6 and another after wave 24.

Enemies never stack: spawn positions avoid fresh spawns, and a light separation pass nudges
overlapping ships apart so formations stay readable.

| Enemy | Normal | Elite |
|-------|--------|-------|
| Scout | 1 HP, fast dive, 100 points | 2 HP, faster, 180 points |
| Weaver | 2 HP, sine weave, 150 points | 3 HP, leaves short bullet trails, 240 points |
| Shooter | 3 HP, aimed shots, 250 points | 4 HP, split-shot burst, 375 points |

Some shooters peel sideways after firing instead of simply falling offscreen.

## Bosses

Bosses enter every fourth wave behind a flashing WARNING klaxon, and change behavior below about
66% and 33% HP — the phase change alters the movement pattern itself, not just the fire rate.
Every move that invades the player's zone (dive, slam, lunge, teleport) is telegraphed with a
flashing charge frame first. Bosses flash white when hit and go down in a chained-explosion
death sequence before the final blast.

The authored campaign has a unique boss at each boss wave from W04 through W60, each with its own
silhouette, movement language, and attack script:

| Wave | Boss | Movement language |
|------|------|-------------------|
| W04 | Gorgon | Rampart crawl; slams down and grinds through the dodge zone |
| W08 | Reaper | Deep dives through the player band; chains dives when enraged |
| W12 | Leviathan | High carrier; telegraphed full-width bombing trawls + fighter squads |
| W16 | Seeker | Orbits the player's position; the orbit tightens per phase |
| W20 | Mantis | Clings to a wall, then lunges across at the player's height |
| W24 | Anvil | Floor crush with a horizontal bullet shockwave; double-crushes enraged |
| W28 | Seraph | Full-width pendulum sweep that dips through mid-screen |
| W32 | Nexus | Anchored core; two orbiting fire-pods own the crossfire |
| W36 | Kraken | Advancing tentacle wall; recoils to the top at each phase change |
| W40 | Phantom | True teleports: dematerialises, blinks, arrival burst |
| W44 | Citadel | Marches a perimeter rectangle that shrinks per phase |
| W48 | Vortex | Breathing spiral whose centre drifts over the player |
| W52 | Basilisk | Stalks the player's column, then guillotines straight down it |
| W56 | Titan | Quake slams with bullet columns; steamrolls when enraged |
| W60 | Overlord | Finale: figure-eight, then Phantom blinks, then Seeker orbit + Reaper dives |

The opening sector and each of the fifteen post-boss sectors have distinct music. The W60 Overlord
ends the campaign with an animated campaign-complete scene and custom win theme. The celebration
runs for six seconds before accepting a choice so held fire cannot skip it: Enter starts Freeplay,
while Esc saves and returns. Freeplay boss HP is capped at the wave-60 durability level while attack
and support pressure continue.

Bosses call bounded support groups, with Leviathan and Kraken launching larger carrier squads.
The marked supply escort in each squad guarantees an upgrade when destroyed; the remaining escorts
have Easy 55%, Normal 50%, and Hard 45% drop chances. Each boss encounter still allows at most four
support drops.
Defeating a boss always drops Missiles plus two need-aware rewards: recovery/weapon help and a Bomb
or score gem. Unused bombs after boss waves grant +400 each.

## Weapons

Gun levels persist between waves, but losing a ship drops one gun level and resets the weapon type
to Cannon.

| Weapon | Pickup | Role |
|--------|--------|------|
| Cannon | default / G upgrades | All-round spread, damage 1 |
| Laser | Z pickup | Levels 1-4 fire 1/2/3/4 piercing lanes, damage 2 per beam |
| Wave | W pickup | Levels 1-4 widen to 5/7/9/11 shots, damage 1, slower cooldown |

Rapid fire (`R`) temporarily reduces weapon cooldown.
The G pickup upgrades whichever weapon is currently equipped. Collecting Z while Wave is equipped
keeps Wave active and grants a ten-second piercing Wave boost, shown as `WAVE*` on the HUD.

## Missiles, Bombs, And Pickups

Missiles start at 5 ammo and cap at 30 (two-digit HUD counter). Each missile steers toward the
nearest target ahead of it and explodes with area damage (4 to enemies in a 52 px blast, and
**1/10 of a boss's max HP**). Bosses always drop an M pickup on death.

Smart bombs clear enemy bullets, damage all enemies, and deal 1/3 of a boss's max HP. Bombs also
score +25 per bullet cleared and +100 per enemy killed by the bomb.

| Icon | Type | Effect |
|------|------|--------|
| G | Gun | +1 level for the equipped Cannon, Laser, or Wave, up to 4 |
| R | Rapid | Timed fast fire |
| H | Shield | ~10 seconds of full invulnerability; blinks before expiring |
| L | Life | +1 ship, up to 9 |
| M | Missiles | +4 missile ammo, up to 30 |
| Z | Laser | Switches to Laser, or boosts an equipped Wave for ten seconds |
| W | Wave | Switches main weapon to Wave |
| B | Bomb | +1 smart bomb, up to 10 |
| $ | Score | Risk pickup worth 500 + 50 per wave, plus pickup points |

The score pickup appears near the upper-mid playfield after a 10+ kill combo streak.
Ordinary drops are weighted toward current shortages. Capped Gun, Life, Missile, or Bomb pickups
convert into +300 points instead of being wasted.

## Scoring

Consecutive kills build a combo timer. Every 6 kills increases the combo multiplier by 1, capped at x5.
Taking any hit breaks the combo. Grazing enemy bullets awards +10 and refreshes the combo timer.
Combo tier-ups, elite bounties, and boss kills show floating score popups at the kill site.
Difficulty then multiplies scoring: Easy x1.00, Normal x1.25, Hard x1.60.

| Event | Points |
|-------|--------|
| Scout / Elite Scout | 100 / 180 |
| Weaver / Elite Weaver | 150 / 240 |
| Shooter / Elite Shooter | 250 / 375 |
| Boss | 5000 |
| Power-up pickup | 50 |
| Graze | 10 |
| Endurance milestone | 2000 every 10 waves |

Wave medals are awarded when the playfield clears:

| Medal | Bonus |
|-------|-------|
| NO HIT | 1000 |
| CLEAN SWEEP | 750 |
| COMBO HELD | 500 |

Difficulty score multipliers apply to medals, kills, pickups, graze, bombs, boss kills, and
endurance bonuses.
