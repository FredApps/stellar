# Changelog

All notable release changes are recorded here. GitHub release tags must have a
matching section in this file.

## Unreleased

- Renamed the game to Ayrien Assault across the DOS build, browser port, sound bank,
  configuration, floppy image, release archives, documentation, and generated captures.
- Made browser music unlock on the first ordinary pointer, touch, click, or key gesture;
  mobile windowed play no longer depends on entering fullscreen to start the title cue.
- Added the 18-cue generated soundtrack bank and native PC speaker, AdLib OPL2,
  Roland MT-32, and Sound Blaster playback backends, plus the bounded browser title arrangement.
- Added sixteen distinct gameplay chapter themes: the soundtrack changes after every campaign boss.
- Player ships now turn and fire downward when they fly above a boss; cannon, Laser, Wave, missiles,
  exhaust, boost trails, and sprites all follow the facing direction.
- Smoothed the opening Gorgon fight with lower HP, a longer slam tell, and difficulty-specific curtains.
- Restored the browser Wave-Laser boost behavior and corrected difficulty-scaled score popups.
- Removed duplicate mobile control rails and kept one fullscreen control outside the playfield.
- Made Laser levels visibly progress from one to four piercing lanes and clarified how G upgrades
  Laser, Wave, and Cannon in the help screen and pickup feedback.
- Boss support squads now identify a supply escort with a guaranteed upgrade drop; remaining escorts
  have higher difficulty-scaled drop odds while the four-drop encounter cap remains in place.
- Game Over now ignores Space so held fire cannot skip the result screen, and help explains that
  boxed elites are tougher, use stronger attacks, and award bonus score.
- Restored ordinary-enemy upgrade drops to Easy 18%, Normal 15%, and Hard 12%; increased drop odds
  remain exclusive to support escorts spawned during boss battles.
- Expanded Help into six readable pages covering controls, pickups, weapons, survival, enemies,
  bosses, scoring, and difficulty; Up/Down arrows and mobile side controls scroll the manual.
- Extended the Overlord victory celebration with denser fireworks, confetti, staged messages, and
  a six-second input lock; Space is ignored, Enter starts Freeplay, and Esc saves and returns.
- Kept the red Game Over flash while clearing combat colors and shake before returning to menus.
- Improved mobile steering with a floating left-thumb joystick, a smaller dead zone, faster response,
  and relative playfield dragging that no longer jumps when switching control surfaces.
- Added mobile Replay and Title controls to the high-score table so a finished run never requires a
  page refresh.

## v0.1.3 - 2026-07-10

- Redesigned all 15 campaign boss sprites (up to 72x52) with distinct silhouettes and
  run-time animation: Kraken's writhing tentacles, Vortex's orbiting orbs, Seeker's and
  Basilisk's player-tracking pupils, Nexus's tethered fire-pods, Titan's per-phase
  armour-break, Overlord's rotating crown spokes, Phantom's dematerialise mask.
- Rewrote every boss movement pattern: bosses now dive, slam, lunge, teleport, patrol, and
  orbit through the player's zone instead of hovering in a strip near the top. Each
  zone-invading move is telegraphed; phase changes alter the pattern itself.
- Added a flashing WARNING klaxon before each boss entrance, a white boss hit-flash, and a
  staged chained-explosion boss death.
- Enemies no longer stack: spawn placement avoids fresh spawns and a per-frame separation
  pass nudges overlapping ships apart (covered by a new self-test).
- Balance: missiles now deal 1/12 of boss max HP (was 1/10) and bombs 1/4 (was 1/3);
  Reaper's HP penalty removed; divers/lungers fire slightly slower; a third enemy speed
  tier lands after wave 24 and elites reach 22% from wave 20; life drops are slightly
  rarer in favour of gun upgrades.
- Added floating score popups for combo tier-ups, elite bounties, and boss kills.
- Shield pickups now grant ~10 seconds of full invulnerability in both builds, with an
  expiry warning blink; the browser port previously popped its shield on the first hit.
- Mirrored the full boss, separation, and balance overhaul into the browser port and
  deployed the updated web build.
- `/shot` now renders the boss roster across `BOSSES1.BMP`/`BOSSES2.BMP` and writes
  headless logic checks (separation + all 45 boss movement envelopes) to `SELFTEST.TXT`.

## v0.1.2 - 2026-07-07

- Added distinct movement and attack identities for each native boss.
- Added the wave 60 Overlord final boss, victory screen, freeplay continuation, and win theme.
- Changed boss bomb and missile damage to scale by boss maximum health.
- Mirrored the new boss campaign state in the browser port and deployed the updated web build.

## v0.1.1 - 2026-07-06

- Added WASD movement controls in addition to arrow-key movement.
- Updated the browser port so WASD works in the web build.
- Refreshed DOS release artifacts through the GitHub Actions release pipeline.

## v0.1.0 - 2026-07-06

- Published the first public GPLv3 release of Stellar.
- Added the MS-DOS/Open Watcom release workflow.
- Added packaged MS-DOS artifacts: `STELLAR.EXE`, `STELLAR.IMG`, `README.TXT`, and `Stellar-MS-DOS.zip`.
- Documented the 35 FPS native pacing fix for 33 MHz, 66 MHz, and Pentium-class machines.
- Linked the browser start page to GitHub and the MS-DOS release downloads.

