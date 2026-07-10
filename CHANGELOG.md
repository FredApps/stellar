# Changelog

All notable release changes are recorded here. GitHub release tags must have a
matching section in this file.

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

