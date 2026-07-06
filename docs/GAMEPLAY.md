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
| Up/Down on title | Change difficulty |
| Esc | PLAY -> title; TITLE -> quit; high-score entry -> save and replay |

## Objective

Stellar Assault is an endless arcade score chase. Clear waves, survive boss attacks, graze danger,
build combos, earn medals, and push the high-score table as far as you can.

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
wave 12. Easy reduces those chances by 4 points; Hard adds 5 points.

| Enemy | Normal | Elite |
|-------|--------|-------|
| Scout | 1 HP, fast dive, 100 points | 2 HP, faster, 180 points |
| Weaver | 2 HP, sine weave, 150 points | 3 HP, leaves short bullet trails, 240 points |
| Shooter | 3 HP, aimed shots, 250 points | 4 HP, split-shot burst, 375 points |

Some shooters peel sideways after firing instead of simply falling offscreen.

## Bosses

Bosses enter every fourth wave and change behavior below about 66% and 33% HP. Dense attacks have
a flashing charge tell, and damaged bosses show visible scorch/phase overlays.

Boss order rotates Warship -> Dreadnought -> Hive, so the first boss is the aimed-shot Warship
and the second boss introduces the denser fan pressure.

| Boss | Attack Style |
|------|--------------|
| Warship | Aimed twin bursts with center pressure; Hard adds side pressure |
| Dreadnought | Sweeping bullet fans; Hard adds earlier fast fans; calls escorts at phase changes |
| Hive | Radial spreads; Hard widens the ring and adds crossfire; calls weaver escorts at phase changes |

Defeating a boss drops three pickups — a Life, a Bomb, and a Missile pack. Unused bombs after boss
waves grant +400 each.

## Weapons

Gun levels persist between waves, but losing a ship drops one gun level and resets the weapon type
to Cannon.

| Weapon | Pickup | Role |
|--------|--------|------|
| Cannon | default / G upgrades | All-round spread, damage 1 |
| Laser | Z pickup | Fewer lanes, piercing, damage 2 |
| Wave | W pickup | Wide crowd control, damage 1, slower cooldown |

Rapid fire (`R`) temporarily reduces weapon cooldown.

## Missiles, Bombs, And Pickups

Missiles start at 5 ammo and cap at 30 (two-digit HUD counter). Each missile steers toward the
nearest target ahead of it and explodes with area damage (4 to enemies in a 52 px blast, **18 to
a boss** — missiles are the boss-killer tool). Bosses always drop an M pickup on death.

Smart bombs clear enemy bullets, damage all enemies, and hit the boss. Bombs also score +25 per
bullet cleared and +100 per enemy killed by the bomb.

| Icon | Type | Effect |
|------|------|--------|
| G | Gun | +1 gun level, up to 4 |
| R | Rapid | Timed fast fire |
| H | Shield | Absorbs one hit |
| L | Life | +1 ship, up to 9 |
| M | Missiles | +4 missile ammo, up to 30 |
| Z | Laser | Switches main weapon to Laser |
| W | Wave | Switches main weapon to Wave |
| B | Bomb | +1 smart bomb, up to 10 |
| $ | Score | Risk pickup worth 500 + 50 per wave, plus pickup points |

The score pickup appears near the upper-mid playfield after a 10+ kill combo streak.

## Scoring

Consecutive kills build a combo timer. Every 6 kills increases the combo multiplier by 1, capped at x5.
Taking any hit breaks the combo. Grazing enemy bullets awards +10 and refreshes the combo timer.
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
