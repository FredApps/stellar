====================================================================
  STELLAR ASSAULT  -  a VGA space shooter for the IBM PC
====================================================================

REQUIREMENTS
  - Intel 80486 (or better)
  - 4 MB RAM
  - MS-DOS 3.3 or later
  - VGA display (320x200 256-colour mode 13h)
  - PC speaker for sound (optional)

  Gameplay is paced near 35 FPS so faster 66 MHz and Pentium-class
  machines do not run the action at double speed.

RUNNING
      A:\> SHOOTER

  High scores are saved to HISCORE.DAT in the current directory.

CONTROLS
  WASD/Arrows ... move your ship
  Shift ......... boost while held (uses BST bar)
  Space ......... fire main weapon
  Ctrl .......... launch homing missile
  B ............. smart bomb
  P ............. pause
  M ............. mute / unmute sound
  H ............. instruction menu (on title screen)
  Up/Down ....... title difficulty / scroll help pages
  Esc ........... return to title / quit

  Help has six pages. Follow the enabled Up/Down arrows at the bottom;
  Space also advances to the next page.

  High-score names are remembered for this session. Empty names are not saved.
  In high-score entry, Esc saves quickly and starts another run.
  On the score table, Ctrl starts another run.

GAMEPLAY
  Survive escalating waves. Enemy waves now have personalities:
  scout rushes, weaver mazes, shooter crossfire, and mixed pressure.
  Formations include columns, rows, V shapes, and alternating weavers.

  Shift boost raises ship speed while the BST bar has charge. The bar
  drains while boosting and recharges after a short delay.

  Fly above a boss to turn the ship and all weapons downward. The ship
  turns upward again after you cross below the boss.
  With a shield active, ramming enemies destroys them. Ramming a boss deals
  one tenth of its maximum health and bounces your ship clear.

  From wave 7, elite enemies can appear:
      Elite Scout   - tougher and faster
      Elite Weaver  - leaves short bullet trails
      Elite Shooter - fires a split shot
  A blinking cyan/white box around a ship marks it as elite.

  Every fourth wave is a boss. The campaign has a unique boss at each
  boss wave from W04 through W60: Gorgon, Reaper, Leviathan, Seeker,
  Mantis, Anvil, Seraph, Nexus, Kraken, Phantom, Citadel, Vortex,
  Basilisk, Titan, and Overlord. Bosses flash before heavy attacks,
  show damage as their health falls, and all bosses call bounded support
  groups. Each squad has a marked supply escort that carries an upgrade.
  Other escorts have improved drop chances, with no more than four support
  drops during one boss encounter.
  The soundtrack changes after every boss.
  Defeating the W60 Overlord opens an animated campaign-complete screen.
  After the six-second celebration, Enter starts Freeplay and Esc saves
  the score and returns. Space is ignored so held fire cannot skip it.
  Freeplay boss health no longer grows beyond the W60 durability level.

WEAPONS
      Cannon  - all-round spread, damage 1
      Laser   - levels 1-4 fire 1/2/3/4 piercing lanes, damage 2
      Wave    - levels 1-4 widen to 5/7/9/11 shots, damage 1

  G upgrades whichever weapon is equipped. Collecting Z while using Wave
  grants a 10-second piercing Wave boost, shown as WAVE* on the HUD.

POWER-UPS
      G  Gun       - +1 level for the equipped weapon (max 4)
      R  Rapid     - faster firing (timed)
      H  Shield    - about 10 seconds of invulnerability
      L  Life      - extra ship
      M  Missiles  - +4 homing missiles (max 30)
      Z  Laser     - switch to Laser; boosts Wave when already equipped
      W  Wave      - switch to Wave
      B  Bomb      - +1 smart bomb (max 10)
      $  Score     - risky bonus pickup

SCORING
  Combos raise score up to x5. Grazing enemy bullets awards +10 and
  refreshes the combo timer. A 10+ combo can spawn a risky $ pickup.
  Difficulty multiplies score: Easy x1.00, Normal x1.25, Hard x1.60.
  Drops favor resources you currently need. Capped pickups convert to score.

  Wave medals:
      NO HIT       +1000
      CLEAN SWEEP  +750
      COMBO HELD   +500

  Boss waves award +400 for each unused bomb. Every 10th wave grants
  an endurance bonus of +2000.

====================================================================
  Built with Open Watcom (16-bit real mode).
  Released under the GNU GPL version 3. Source and releases:
  https://github.com/FredApps/Stellar
====================================================================
