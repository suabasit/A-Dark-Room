# A Dark Room — SFML Final Project

A GUI remake of the survival game **A Dark Room**, built in C++ with the SFML
graphics library for the Programming Fundamentals semester final project.

You start beside a dying fire in a cold, dark room. Keep the fire alive, gather
wood, a mysterious stranger arrives, you build a village, assign workers, trade,
then venture out on dangerous expeditions to fight enemies and explore ruins.

---

## 1. How to build and run

### What you need
* A C++ compiler (`g++`)
* The **SFML** library (version 2.5 or 2.6)

Install SFML:

* **Ubuntu / Debian / WSL:** `sudo apt-get install libsfml-dev`
* **Windows (MSYS2):** `pacman -S mingw-w64-x86_64-sfml`
* **macOS (Homebrew):** `brew install sfml`

### Compile

Open a terminal **inside this folder** (the one that holds `game.cpp` and the
`.txt` files) and run:

```
g++ game.cpp -o adarkroom -lsfml-graphics -lsfml-window -lsfml-system
```

### Run

```
./adarkroom
```

(On Windows the file is `adarkroom.exe`, so just type `adarkroom`.)

> **IMPORTANT:** Run the program from inside this folder. The game reads its
> data files (`buildings.txt`, `enemies.txt`, `locations.txt`, `weapons.txt`)
> and the font (`assets/font.ttf`) using relative paths, so they must be in the
> current working directory when the game starts.

---

## 2. Files in this project

| File | What it is |
|------|------------|
| `game.cpp` | The entire game source code. |
| `buildings.txt` | Building list: name, wood cost, fur cost, iron cost, max count. |
| `enemies.txt` | Enemy list: name, hp, attack, speed, loot. |
| `locations.txt` | Expedition locations: name, distance, map marker. |
| `weapons.txt` | Weapon list: name, damage. |
| `assets/font.ttf` | The font used to draw all on-screen text. |
| `README.md` | This file. |

The game also **creates** two files while you play:
`savegame.txt` (when you save) and `highscores.txt` (when a run ends).

---

## 3. How to play

The whole game is controlled with the **mouse** — click the buttons on the left.
A few keyboard shortcuts exist for the room screen: `F` = stoke fire,
`G` = gather wood, `S` = save, `L` = load.

1. **The Room** — Click *stoke fire* to keep the fire burning (costs 1 wood).
   Click *gather wood* to collect wood. If the fire dies, your villagers start
   to leave, so keep feeding it. After a few stokes a **stranger** arrives and
   the village opens up.
2. **The Village** — Build huts (more population), traps (food), a tannery,
   a trading post, and more. Assign your villagers to jobs (Wood Cutter,
   Trapper, Tanner, …) and they produce resources automatically each turn.
   Use the trading post to buy a **compass** — you need it to explore.
3. **Expeditions** — With a compass and some meat, head out. Move forward toward
   a location, scavenge for loot (sometimes an enemy attacks — fight, use an
   item, or flee), and return home before your water runs out.

---

## 4. Where each rubric requirement is implemented

### Phase 1 — The Fire (25 marks)

| Requirement | Where in `game.cpp` |
|-------------|--------------------|
| Fire starts dead, 5 states with exact text | `fireLabel()`, `g_fireState` starts at 0 |
| Fire degrades 1 state every 5 turns | `advanceTurn()`, `FIRE_DECAY = 5` |
| Stoke costs 1 wood, raises fire (max roaring) | `actionStokeFire()` |
| Fire dies → villagers leave (penalty) | `advanceTurn()` dead-fire block |
| Event log (8 entries, newest on top) | `pushEvent()`, `g_log[8]` |
| Stranger arrives (after 3 stokes or turn 15) | `advanceTurn()`, `STRANGER_*` constants |
| Wood counter shown on screen | `drawResourceBar()` |

### Phase 2 — The Village (35 marks)

| Requirement | Where in `game.cpp` |
|-------------|--------------------|
| Gather wood (+10, capped) | `actionGatherWood()` |
| 6+ resources tracked and capped | `add*()` helpers, `CAP_*` constants |
| Buildings loaded from file into dynamic arrays | `loadBuildings()` (`new[]`) |
| 5+ buildings, costs deducted, max counts | `actionBuild()`, `buildings.txt` |
| Hut raises max population by 4 | `actionBuild()` hut block |
| Population grows every 20 turns if room | `advanceTurn()`, `POP_GROW = 20` |
| Worker roles (assign / unassign, never < 0) | `assignWorker()`, `unassignWorker()` |
| Automatic production each turn, capped | `productionTick()` |
| Trading post (sell 3, buy 4) unlocked when built | `sellResource()`, `buyItem()`, `tradingPostBuilt()` |

### Phase 3 — Expeditions & Combat (25 marks)

| Requirement | Where in `game.cpp` |
|-------------|--------------------|
| Need compass + meat to leave | `startExpedition()` |
| Locations loaded from file (dynamic arrays) | `loadLocations()` (`new[]`) |
| 5 locations with correct distances | `locations.txt` |
| Water drops while travelling, HP penalty at 0 | `expMoveForward()` |
| Enemies loaded from file (dynamic arrays) | `loadEnemies()` (`new[]`) |
| 3+ enemy types, correct hp/attack/speed | `enemies.txt` |
| ~30% chance of combat when scavenging | `expScavenge()` |
| Attack / Use Item / Flee | `combatAttack()`, `combatUseItem()`, `combatFlee()` |
| Fast enemies strike first | `startCombat()` |
| Survive at 1 HP → return to village | combat HP checks |

### Code Quality (15 marks)

| Requirement | Where |
|-------------|-------|
| Dynamic arrays with `new[]` … `delete[]` | `load*()` functions, `freeData()`, end of `main()` |
| No `struct` except provided helper | only `struct Button` (the provided helper) is used; all game state is in **global variables** |
| 3+ data files read with `ifstream` | `buildings.txt`, `enemies.txt`, `locations.txt`, `weapons.txt` |
| Data saved with `ofstream` | `saveGame()`, `saveHighScore()` |
| Meaningful names, comments | throughout |
| Input validation never crashes | `safeParseInt()` (ignores non-digits) |

### Bonus features (extra marks)

| Bonus | Where |
|-------|-------|
| Save / Load game | `saveGame()`, `loadGame()` (`savegame.txt`) |
| ASCII map (10×10) | `g_map`, `initMap()`, `updateMap()` |
| Story events (5+) | `triggerStory()` |
| Difficulty modes | `scaledCost()`, `scaledEnemyStat()`, `applyDifficultyStart()` |
| Multiple weapon slots / switching | `switchWeapon()`, `weapons.txt` |
| Crafting recipes | `craftItem()` |
| High-score leaderboard (top 5) | `saveHighScore()`, `computeScore()` (`highscores.txt`) |

---

## 5. A note on the design

The rubric asks for **no `struct`s** (except the provided `Button` helper) and
for **state managed with global variables**, so every piece of game state — the
fire, resources, population, workers, expedition, combat — lives in a global
variable at the top of `game.cpp`. Because structs are not allowed, the data
loaded from files is stored in **parallel arrays** (for example `eName[i]`,
`eHp[i]`, `eAtk[i]` all describe the same enemy `i`). This keeps the code at a
first-semester level while still meeting every requirement.
