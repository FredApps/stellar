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
  Up/Down ....... change difficulty on title screen
  Esc ........... return to title / quit

  In high-score entry, Esc saves quickly and starts another run.
  On the score table, Ctrl starts another run.

GAMEPLAY
  Survive escalating waves. Enemy waves now have personalities:
  scout rushes, weaver mazes, shooter crossfire, and mixed pressure.
  Formations include columns, rows, V shapes, and alternating weavers.

  Shift boost raises ship speed while the BST bar has charge. The bar
  drains while boosting and recharges after a short delay.

  From wave 7, elite enemies can appear:
      Elite Scout   - tougher and faster
      Elite Weaver  - leaves short bullet trails
      Elite Shooter - fires a split shot

  Every fourth wave is a boss. Boss order is Warship, Dreadnought,
  then Hive. Bosses flash before heavy attacks, show damage as their
  health falls, and some call escorts when entering new phases.

WEAPONS
      Cannon  - all-round spread, damage 1
      Laser   - fewer lanes, piercing, damage 2
      Wave    - wide crowd control, damage 1, slower fire

POWER-UPS
      G  Gun       - +1 gun level (max 4)
      R  Rapid     - faster firing (timed)
      H  Shield    - absorbs one hit
      L  Life      - extra ship
      M  Missiles  - +4 homing missiles (max 30)
      Z  Laser     - switch to Laser
      W  Wave      - switch to Wave
      B  Bomb      - +1 smart bomb (max 10)
      $  Score     - risky bonus pickup

SCORING
  Combos raise score up to x5. Grazing enemy bullets awards +10 and
  refreshes the combo timer. A 10+ combo can spawn a risky $ pickup.
  Difficulty multiplies score: Easy x1.00, Normal x1.25, Hard x1.60.

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
