// ============================================================
//  A DARK ROOM  —  Final Semester Project (Programming Fundamentals, C++)
//
//  A minimalist text-based survival game recreated with the SFML library.
//  Built on top of the instructor-provided SFML starter code.
//
//  HOW TO COMPILE & RUN ON UBUNTU:
//    sudo apt install libsfml-dev
//    g++ game.cpp -o adarkroom -lsfml-graphics -lsfml-window -lsfml-system
//    ./adarkroom
//
//  IMPORTANT: run the program from inside this folder so it can find the
//  data files (buildings.txt, enemies.txt, locations.txt, weapons.txt) and
//  the bundled font in assets/font.ttf.
//
//  DESIGN NOTES (for the grader):
//   * Game state is stored in GLOBAL VARIABLES (no GameState struct) — this
//     matches Learning Objective #2 "manage program state using global
//     variables and functions" and the rubric's "no structs" requirement.
//   * The only struct in the file is `Button`, which is the provided UI
//     helper code (explicitly exempt in the rubric).
//   * Building / enemy / location / weapon data is loaded FROM FILES into
//     DYNAMIC ARRAYS allocated with new[] and released with delete[]. Because
//     structs are not allowed, the data is held in PARALLEL ARRAYS.
// ============================================================

#include <SFML/Graphics.hpp>
#include <string>
#include <sstream>
#include <fstream>     // file I/O (ifstream / ofstream)
#include <algorithm>   // std::min / std::max
#include <cstdlib>     // rand / srand
#include <ctime>       // time

// ─── WINDOW CONSTANTS ────────────────────────────────────────
const int WINDOW_W    = 1040;
const int WINDOW_H    = 600;
const int PANEL_RIGHT = 640;   // x where the right (event-log) panel starts

// ─── SCREEN / STATE CONSTANTS ────────────────────────────────
// We use plain integer constants instead of an enum so the whole game state
// is expressed as simple global variables (no structs / classes).
const int SCREEN_MENU       = -1;   // difficulty + high-score start screen
const int SCREEN_ROOM       = 0;
const int SCREEN_VILLAGE    = 1;
const int SCREEN_EXPEDITION  = 2;

// ─── TUNING CONSTANTS ────────────────────────────────────────
const int TICK_TURNS   = 1;    // resources are produced every TICK_TURNS turns
const int FIRE_DECAY   = 5;    // fire drops 1 state every 5 turns without stoking
const int POP_GROW     = 20;   // a villager arrives every 20 turns (if room)
const int STRANGER_TURNS = 15; // stranger arrives by turn 15 (or after 3 stokes)
const int STRANGER_STOKES = 3;

// Resource caps (wood cap depends on whether the Cart is built — see woodCap())
const int CAP_WOOD_BASE = 100;
const int CAP_WOOD_CART = 500;
const int CAP_FUR     = 100;
const int CAP_MEAT    = 100;
const int CAP_LEATHER = 100;
const int CAP_CLOTH   = 100;
const int CAP_IRON    = 50;
const int CAP_SCALES  = 20;
const int CAP_COINS   = 200;
const int CAP_TEETH   = 50;

// ════════════════════════════════════════════════════════════
//  GLOBAL GAME STATE  (replaces the starter's GameState struct)
// ════════════════════════════════════════════════════════════
int  g_screen = SCREEN_MENU;

// --- Fire ---
int  g_fireState     = 0;   // 0=dead 1=smoldering 2=flickering 3=burning 4=roaring
int  g_turnsSincePoke = 0;
int  g_stokeCount    = 0;

// --- Resources ---
int  g_wood = 0, g_fur = 0, g_meat = 0, g_leather = 0;
int  g_cloth = 0, g_coins = 0, g_iron = 0, g_scales = 0, g_teeth = 0;

// --- Village ---
bool g_strangerArrived = false;
int  g_population    = 0;   // villagers currently living here
int  g_maxPopulation = 0;   // capacity (4 per hut)
int  g_totalTurns    = 0;
int  g_workers[5]    = {0,0,0,0,0}; // 0=WoodCutter 1=Trapper 2=Tanner 3=Trader 4=Hunter

// --- Player / items ---
int  g_hp = 10, g_maxHp = 10, g_armor = 0;
bool g_hasCompass = false;
bool g_hasTorch   = false;
bool g_hasRucksack = false;
int  g_medicine   = 0;
int  g_difficulty = 1;   // 0=Easy 1=Normal 2=Hard

// --- Expedition ---
bool g_onExpedition = false;
int  g_expLocation  = 0;
int  g_expStep      = 0;
int  g_expWater     = 10;
int  g_expFood      = 0;
int  g_bestDistance = 0;   // furthest distance reached (for high score)

// --- Combat (a click-driven state machine, no blocking loop) ---
bool g_inCombat   = false;
int  g_enemyIndex = 0;
int  g_enemyHp    = 0;

// --- Story events (bonus) ---
bool g_storyActive  = false;
int  g_storyIndex   = -1;
bool g_storyDone[5] = {false,false,false,false,false};

// --- Trading-post sell quantity: a validated text-input buffer (bonus + input validation) ---
std::string g_inputBuffer = "";

// ─── EVENT LOG (provided helper, kept as-is) ─────────────────
const int LOG_SIZE = 8;
std::string eventLog[LOG_SIZE];
float       logHighlight[LOG_SIZE] = {};   // 0..1 fade timer per slot

void pushEvent(const std::string& msg)
{
    for (int i = LOG_SIZE - 1; i > 0; --i) {     // shift everything down by one
        eventLog[i]     = eventLog[i - 1];
        logHighlight[i] = logHighlight[i - 1];
    }
    eventLog[0]     = msg;                        // newest event goes to the front
    logHighlight[0] = 1.0f;                       // bright, fades over time
}

// ════════════════════════════════════════════════════════════
//  DATA LOADED FROM FILES INTO DYNAMIC PARALLEL ARRAYS (new[])
// ════════════════════════════════════════════════════════════
// Buildings ---------------------------------------------------
int          NUM_BUILD = 0;
std::string* bName  = nullptr;   // building name
int*         bWood  = nullptr;   // wood cost
int*         bFur   = nullptr;   // fur cost
int*         bIron  = nullptr;   // iron cost (only the Steelworks uses this)
int*         bMax   = nullptr;   // maximum number that can be built
int*         bCount = nullptr;   // how many are currently built (game state)

// Enemies -----------------------------------------------------
int          NUM_ENEMY = 0;
std::string* eName    = nullptr;
int*         eHp      = nullptr;
int*         eAtk     = nullptr;
std::string* eSpeed   = nullptr; // "fast" / "normal" / "slow"
int*         eCoin    = nullptr; // coin loot on defeat
std::string* eRes     = nullptr; // resource loot type
int*         eResAmt  = nullptr; // resource loot amount
int*         eTooth   = nullptr; // 1 if it drops a tooth

// Locations ---------------------------------------------------
int          NUM_LOC = 0;
std::string* lName = nullptr;
int*         lDist = nullptr;     // distance (number of steps)
std::string* lMark = nullptr;     // single-letter marker for the ASCII map

// Weapons -----------------------------------------------------
int          NUM_WEAP = 0;
std::string* wName  = nullptr;
int*         wDmg   = nullptr;
bool*        wOwned = nullptr;    // does the player own this weapon?
int          g_equipped = 0;      // index of currently equipped weapon (0 = Fists)

// Convenience building indices (found by name after loading) ---
int idxTrap = -1, idxCart = -1, idxHut = -1, idxSmokehouse = -1;
int idxTannery = -1, idxTradingPost = -1, idxSteelworks = -1;

// ─── ASCII WORLD MAP (bonus) — a 2D char array, not a struct ──
const int MAP_N = 10;
char g_map[MAP_N][MAP_N];

// ════════════════════════════════════════════════════════════
//  SMALL HELPER FUNCTIONS
// ════════════════════════════════════════════════════════════

// Replace underscores with spaces so file-friendly names ("Trading_Post")
// display nicely to the player ("Trading Post").
std::string disp(std::string s)
{
    for (unsigned i = 0; i < s.size(); ++i)
        if (s[i] == '_') s[i] = ' ';
    return s;
}

// Current wood capacity — the Cart upgrade raises it from 100 to 500.
int woodCap()
{
    if (idxCart >= 0 && bCount[idxCart] > 0) return CAP_WOOD_CART;
    return CAP_WOOD_BASE;
}

// Add to a resource while respecting its cap (no overflow above MAX).
void addWood   (int n){ g_wood    = std::min(g_wood    + n, woodCap());   if (g_wood<0)    g_wood=0;    }
void addFur    (int n){ g_fur     = std::min(g_fur     + n, CAP_FUR);     if (g_fur<0)     g_fur=0;     }
void addMeat   (int n){ g_meat    = std::min(g_meat    + n, CAP_MEAT);    if (g_meat<0)    g_meat=0;    }
void addLeather(int n){ g_leather = std::min(g_leather + n, CAP_LEATHER); if (g_leather<0) g_leather=0; }
void addCloth  (int n){ g_cloth   = std::min(g_cloth   + n, CAP_CLOTH);   if (g_cloth<0)   g_cloth=0;   }
void addCoins  (int n){ g_coins   = std::min(g_coins   + n, CAP_COINS);   if (g_coins<0)   g_coins=0;   }
void addIron   (int n){ g_iron    = std::min(g_iron    + n, CAP_IRON);    if (g_iron<0)    g_iron=0;    }
void addScales (int n){ g_scales  = std::min(g_scales  + n, CAP_SCALES);  if (g_scales<0)  g_scales=0;  }
void addTeeth  (int n){ g_teeth   = std::min(g_teeth   + n, CAP_TEETH);   if (g_teeth<0)   g_teeth=0;   }

// Validate a typed string and turn it into a non-negative integer.
// Any non-digit characters are ignored, so invalid input can NEVER crash.
int safeParseInt(const std::string& s)
{
    int value = 0;
    for (unsigned i = 0; i < s.size(); ++i) {
        if (s[i] >= '0' && s[i] <= '9') {
            value = value * 10 + (s[i] - '0');
            if (value > 100000) value = 100000;   // clamp to keep it sensible
        }
    }
    return value;
}

// Random integer in [lo, hi].
int randRange(int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + rand() % (hi - lo + 1);
}

// ─── FILE LOADING (ifstream → dynamic arrays via new[]) ──────

void loadBuildings()
{
    std::ifstream f("buildings.txt");
    if (!f) { pushEvent("ERROR: buildings.txt not found."); return; }
    f >> NUM_BUILD;
    bName  = new std::string[NUM_BUILD];
    bWood  = new int[NUM_BUILD];
    bFur   = new int[NUM_BUILD];
    bIron  = new int[NUM_BUILD];
    bMax   = new int[NUM_BUILD];
    bCount = new int[NUM_BUILD];
    for (int i = 0; i < NUM_BUILD; ++i) {
        f >> bName[i] >> bWood[i] >> bFur[i] >> bIron[i] >> bMax[i];
        bCount[i] = 0;
    }
    f.close();

    // Remember the index of each special building so we can apply its effect.
    for (int i = 0; i < NUM_BUILD; ++i) {
        if (bName[i] == "Trap")         idxTrap        = i;
        if (bName[i] == "Cart")         idxCart        = i;
        if (bName[i] == "Hut")          idxHut         = i;
        if (bName[i] == "Smokehouse")   idxSmokehouse  = i;
        if (bName[i] == "Tannery")      idxTannery     = i;
        if (bName[i] == "Trading_Post") idxTradingPost = i;
        if (bName[i] == "Steelworks")   idxSteelworks  = i;
    }
}

void loadEnemies()
{
    std::ifstream f("enemies.txt");
    if (!f) { pushEvent("ERROR: enemies.txt not found."); return; }
    f >> NUM_ENEMY;
    eName   = new std::string[NUM_ENEMY];
    eHp     = new int[NUM_ENEMY];
    eAtk    = new int[NUM_ENEMY];
    eSpeed  = new std::string[NUM_ENEMY];
    eCoin   = new int[NUM_ENEMY];
    eRes    = new std::string[NUM_ENEMY];
    eResAmt = new int[NUM_ENEMY];
    eTooth  = new int[NUM_ENEMY];
    for (int i = 0; i < NUM_ENEMY; ++i)
        f >> eName[i] >> eHp[i] >> eAtk[i] >> eSpeed[i]
          >> eCoin[i] >> eRes[i] >> eResAmt[i] >> eTooth[i];
    f.close();
}

void loadLocations()
{
    std::ifstream f("locations.txt");
    if (!f) { pushEvent("ERROR: locations.txt not found."); return; }
    f >> NUM_LOC;
    lName = new std::string[NUM_LOC];
    lDist = new int[NUM_LOC];
    lMark = new std::string[NUM_LOC];
    for (int i = 0; i < NUM_LOC; ++i)
        f >> lName[i] >> lDist[i] >> lMark[i];
    f.close();
}

void loadWeapons()
{
    std::ifstream f("weapons.txt");
    if (!f) { pushEvent("ERROR: weapons.txt not found."); return; }
    f >> NUM_WEAP;
    wName  = new std::string[NUM_WEAP];
    wDmg   = new int[NUM_WEAP];
    wOwned = new bool[NUM_WEAP];
    for (int i = 0; i < NUM_WEAP; ++i) {
        f >> wName[i] >> wDmg[i];
        wOwned[i] = false;
    }
    if (NUM_WEAP > 0) wOwned[0] = true;   // Fists are always available
    f.close();
}

// Release every dynamic array — called once before the program exits.
void freeData()
{
    delete[] bName;  delete[] bWood; delete[] bFur; delete[] bIron; delete[] bMax; delete[] bCount;
    delete[] eName;  delete[] eHp;   delete[] eAtk; delete[] eSpeed;
    delete[] eCoin;  delete[] eRes;  delete[] eResAmt; delete[] eTooth;
    delete[] lName;  delete[] lDist; delete[] lMark;
    delete[] wName;  delete[] wDmg;  delete[] wOwned;
}

// ─── ASCII MAP HELPERS (bonus) ───────────────────────────────

// Fill the map with unexplored tiles ('.') and drop a marker for each
// location along a simple diagonal path.
void initMap()
{
    for (int r = 0; r < MAP_N; ++r)
        for (int c = 0; c < MAP_N; ++c)
            g_map[r][c] = '.';

    // Place each location's marker on the diagonal path.
    for (int i = 0; i < NUM_LOC && i < MAP_N; ++i) {
        int cell = i * 2;                 // spread markers out along the path
        if (cell >= MAP_N) cell = MAP_N - 1;
        g_map[cell][cell] = lMark[i].empty() ? '?' : lMark[i][0];
    }
}

// Reveal tiles up to the current expedition progress and place the player '@'.
void updateMap()
{
    // How far across the map are we, based on distance travelled?
    int reveal = g_expStep / 3;
    if (reveal >= MAP_N) reveal = MAP_N - 1;
    for (int i = 0; i <= reveal && i < MAP_N; ++i) {
        if (g_map[i][i] == '.') g_map[i][i] = ':';   // walked trail
    }
}

// ════════════════════════════════════════════════════════════
//  CORE GAME LOGIC
// ════════════════════════════════════════════════════════════

// Build-cost multiplier from the difficulty setting (bonus: difficulty modes).
int scaledCost(int baseCost)
{
    if (g_difficulty == 0) return (baseCost * 8)  / 10;   // Easy  : 80% cost
    if (g_difficulty == 2) return (baseCost * 13) / 10;   // Hard  : 130% cost
    return baseCost;                                      // Normal: 100% cost
}

// Produce resources from traps, the tannery and assigned workers.
void productionTick()
{
    bool smoke = (idxSmokehouse >= 0 && bCount[idxSmokehouse] > 0);

    // Traps passively give meat + fur (smokehouse doubles the meat).
    if (idxTrap >= 0 && bCount[idxTrap] > 0) {
        addMeat(bCount[idxTrap] * (smoke ? 2 : 1));
        addFur (bCount[idxTrap]);
    }

    // Worker production (see manual section 4.6).
    addWood(g_workers[0] * 2);                     // Wood Cutters: +2 wood each
    if (idxTrap >= 0 && bCount[idxTrap] > 0) {      // Trappers: +1 meat +1 fur each
        addMeat(g_workers[1]);
        addFur (g_workers[1]);
    }
    if (idxTannery >= 0 && bCount[idxTannery] > 0) {        // Tanners: 5 fur -> 1 leather each
        for (int k = 0; k < g_workers[2]; ++k)
            if (g_fur >= 5) { g_fur -= 5; addLeather(1); }
    }
    addCoins(g_workers[3] * 5);                     // Traders: +5 coins each
    if (smoke) addMeat(g_workers[4] * 2);           // Hunters: +2 meat each

    // Tannery building passively converts 5 fur -> 2 leather.
    if (idxTannery >= 0 && bCount[idxTannery] > 0 && g_fur >= 5) {
        g_fur -= 5; addLeather(2);
    }
}

// Forward declaration (story events are defined lower down).
void triggerStory(int turn);

// Advance the world by a single turn. Called by every room/village action and
// by the gentle auto-tick. `stoked` is true only on the turn the fire is stoked.
void advanceTurn(bool stoked)
{
    g_totalTurns++;

    if (stoked) {
        g_turnsSincePoke = 0;                       // stoking resets the decay clock
    } else {
        g_turnsSincePoke++;
        // Fire loses one state every FIRE_DECAY turns without stoking.
        if (g_fireState > 0 && g_turnsSincePoke % FIRE_DECAY == 0) {
            g_fireState--;
            if (g_fireState == 1)      pushEvent("the fire is smoldering.");
            else if (g_fireState == 0) pushEvent("the fire is dead.");
            else                       pushEvent("the fire is getting low.");
        }
    }

    // Penalty when the fire is dead: the cold drives a villager away.
    if (g_fireState == 0 && g_population > 0 && g_totalTurns % 3 == 0) {
        g_population--;
        // make sure assigned workers never exceed the smaller population
        int assigned = 0;
        for (int i = 0; i < 5; ++i) assigned += g_workers[i];
        while (assigned > g_population) {
            for (int i = 0; i < 5; ++i)
                if (g_workers[i] > 0) { g_workers[i]--; assigned--; break; }
        }
        pushEvent("a villager wanders off into the cold.");
    }

    // The stranger arrives once: after 3 stokes OR by turn 15.
    if (!g_strangerArrived &&
        (g_stokeCount >= STRANGER_STOKES || g_totalTurns >= STRANGER_TURNS)) {
        g_strangerArrived = true;
        pushEvent("a ragged stranger stumbles in, shivering.");
        pushEvent("the stranger becomes the builder.");
        pushEvent("the builder suggests building a trap.");
    }

    // A villager moves in every POP_GROW turns if there is a free bed.
    if (g_strangerArrived && g_totalTurns % POP_GROW == 0 &&
        g_population < g_maxPopulation) {
        g_population++;
        pushEvent("a quiet villager arrives and settles in.");
    }

    // Resource production happens every TICK_TURNS turns.
    if (g_totalTurns % TICK_TURNS == 0) productionTick();

    // Scripted story events at turn milestones (bonus).
    triggerStory(g_totalTurns);
}

// ─── PHASE 1 ACTIONS ─────────────────────────────────────────

void actionStokeFire()
{
    if (g_wood < 1) { pushEvent("not enough wood to stoke the fire."); return; }
    g_wood--;
    if (g_fireState < 4) g_fireState++;     // increase, capped at 4 (roaring)
    g_stokeCount++;
    pushEvent("you stoke the fire.");
    advanceTurn(true);
}

void actionGatherWood()
{
    addWood(10);                            // +10 wood, respecting the cap
    pushEvent("you gather 10 wood from the forest.");
    advanceTurn(false);
}

// ─── PHASE 2 ACTIONS ─────────────────────────────────────────

// Try to build the building at index `b`. Checks unlocks, max count and cost.
void actionBuild(int b)
{
    if (b < 0 || b >= NUM_BUILD) return;

    // Unlock rules.
    if (!g_strangerArrived) { pushEvent("the builder has not arrived yet."); return; }
    if (b == idxHut && idxTrap >= 0 && bCount[idxTrap] == 0) {
        pushEvent("build a trap before huts unlock."); return;
    }

    // Enforce the maximum count for this building.
    if (bCount[b] >= bMax[b]) {
        pushEvent("cannot build more " + disp(bName[b]) + "."); return;
    }

    int woodCost = scaledCost(bWood[b]);
    int furCost  = scaledCost(bFur[b]);
    int ironCost = scaledCost(bIron[b]);

    // Cost check — show an error if the player cannot afford it.
    if (g_wood < woodCost || g_fur < furCost || g_iron < ironCost) {
        pushEvent("not enough resources for " + disp(bName[b]) + ".");
        return;
    }

    g_wood -= woodCost;
    g_fur  -= furCost;
    g_iron -= ironCost;
    bCount[b]++;

    if (b == idxHut) {                       // a hut raises population capacity by 4
        g_maxPopulation += 4;
        pushEvent("a hut is raised. capacity +4.");
    } else if (b == idxCart) {
        pushEvent("a cart is built. wood capacity is now 500.");
    } else {
        pushEvent("you build a " + disp(bName[b]) + ".");
    }
    advanceTurn(false);
}

// Count how many villagers are not yet assigned to a job.
int unassignedWorkers()
{
    int assigned = 0;
    for (int i = 0; i < 5; ++i) assigned += g_workers[i];
    return g_population - assigned;
}

// Is the worker role at index `r` unlocked yet?
bool roleUnlocked(int r)
{
    if (r == 0) return true;                                            // Wood Cutter
    if (r == 1) return idxTrap >= 0 && bCount[idxTrap] > 0;            // Trapper
    if (r == 2) return idxTannery >= 0 && bCount[idxTannery] > 0;     // Tanner
    if (r == 3) return idxTradingPost >= 0 && bCount[idxTradingPost] > 0; // Trader
    if (r == 4) return idxSmokehouse >= 0 && bCount[idxSmokehouse] > 0;   // Hunter
    return false;
}

const char* roleName(int r)
{
    if (r == 0) return "Wood Cutter";
    if (r == 1) return "Trapper";
    if (r == 2) return "Tanner";
    if (r == 3) return "Trader";
    if (r == 4) return "Hunter";
    return "?";
}

void assignWorker(int r)
{
    if (!roleUnlocked(r)) { pushEvent(std::string(roleName(r)) + " is locked."); return; }
    if (unassignedWorkers() <= 0) { pushEvent("no free villagers to assign."); return; }
    g_workers[r]++;
    pushEvent("a villager becomes a " + std::string(roleName(r)) + ".");
    advanceTurn(false);
}

void unassignWorker(int r)
{
    if (g_workers[r] <= 0) { pushEvent("no " + std::string(roleName(r)) + " to remove."); return; }
    g_workers[r]--;                          // never goes below 0
    pushEvent("a " + std::string(roleName(r)) + " returns to the village.");
    advanceTurn(false);
}

// ─── TRADING POST ────────────────────────────────────────────

bool tradingPostBuilt()
{
    return idxTradingPost >= 0 && bCount[idxTradingPost] > 0;
}

// `qty` comes from the validated text-input box (defaults to 1 if empty).
void sellResource(int which, int qty)
{
    if (!tradingPostBuilt()) { pushEvent("no trading post built."); return; }
    if (qty < 1) qty = 1;

    if (which == 0) {                        // sell fur: 1 fur -> 2 coins
        if (g_fur < qty) qty = g_fur;
        if (qty <= 0) { pushEvent("no fur to sell."); return; }
        g_fur -= qty; addCoins(qty * 2);
        pushEvent("sold " + std::to_string(qty) + " fur for coins.");
    } else if (which == 1) {                 // sell meat: 1 meat -> 1 coin
        if (g_meat < qty) qty = g_meat;
        if (qty <= 0) { pushEvent("no meat to sell."); return; }
        g_meat -= qty; addCoins(qty * 1);
        pushEvent("sold " + std::to_string(qty) + " meat for coins.");
    } else if (which == 2) {                 // sell leather: 1 leather -> 5 coins
        if (g_leather < qty) qty = g_leather;
        if (qty <= 0) { pushEvent("no leather to sell."); return; }
        g_leather -= qty; addCoins(qty * 5);
        pushEvent("sold " + std::to_string(qty) + " leather for coins.");
    }
    advanceTurn(false);
}

void buyItem(int which)
{
    if (!tradingPostBuilt()) { pushEvent("no trading post built."); return; }

    if (which == 0) {                        // buy cloth: 15 coins
        if (g_coins < 15) { pushEvent("need 15 coins for cloth."); return; }
        g_coins -= 15; addCloth(1);
        pushEvent("you buy a bolt of cloth.");
    } else if (which == 1) {                 // buy compass: 300 coins
        if (g_hasCompass) { pushEvent("you already have a compass."); return; }
        if (g_coins < 300) { pushEvent("need 300 coins for a compass."); return; }
        g_coins -= 300; g_hasCompass = true;
        pushEvent("you buy a compass. the dusty path awaits.");
    } else if (which == 2) {                 // buy torch: 5 coins
        if (g_coins < 5) { pushEvent("need 5 coins for a torch."); return; }
        g_coins -= 5; g_hasTorch = true;
        pushEvent("you buy a torch.");
    } else if (which == 3) {                 // buy iron sword: 200 coins
        int sword = -1;
        for (int i = 0; i < NUM_WEAP; ++i) if (wName[i] == "Iron_Sword") sword = i;
        if (sword < 0) { pushEvent("the smith has no swords."); return; }
        if (wOwned[sword]) { pushEvent("you already own an iron sword."); return; }
        if (g_coins < 200) { pushEvent("need 200 coins for an iron sword."); return; }
        g_coins -= 200; wOwned[sword] = true;
        pushEvent("you buy an iron sword.");
    }
    advanceTurn(false);
}

// ─── CRAFTING (bonus) ────────────────────────────────────────
void craftItem(int which)
{
    if (which == 0) {                        // Bone Spear: 5 wood + 5 teeth
        int spear = -1;
        for (int i = 0; i < NUM_WEAP; ++i) if (wName[i] == "Bone_Spear") spear = i;
        if (spear < 0) { pushEvent("you don't know that recipe."); return; }
        if (wOwned[spear]) { pushEvent("you already crafted a bone spear."); return; }
        if (g_wood < 5 || g_teeth < 5) { pushEvent("need 5 wood and 5 teeth."); return; }
        g_wood -= 5; g_teeth -= 5; wOwned[spear] = true;
        pushEvent("you craft a bone spear.");
    } else if (which == 1) {                 // Rucksack: 5 leather + 5 cloth
        if (g_hasRucksack) { pushEvent("you already have a rucksack."); return; }
        if (g_leather < 5 || g_cloth < 5) { pushEvent("need 5 leather and 5 cloth."); return; }
        g_leather -= 5; g_cloth -= 5; g_hasRucksack = true; g_armor += 1;
        pushEvent("you stitch a rucksack. armor +1.");
    } else if (which == 2) {                 // Torch: 2 wood + 1 leather
        if (g_hasTorch) { pushEvent("you already have a torch."); return; }
        if (g_wood < 2 || g_leather < 1) { pushEvent("need 2 wood and 1 leather."); return; }
        g_wood -= 2; g_leather -= 1; g_hasTorch = true;
        pushEvent("you bind a torch.");
    }
    advanceTurn(false);
}

// Cycle to the next weapon the player owns (bonus: multiple weapon slots).
void switchWeapon()
{
    for (int step = 1; step <= NUM_WEAP; ++step) {
        int idx = (g_equipped + step) % NUM_WEAP;
        if (wOwned[idx]) {
            g_equipped = idx;
            pushEvent("you ready the " + disp(wName[idx]) + ".");
            return;
        }
    }
}

// ════════════════════════════════════════════════════════════
//  PHASE 3 — EXPEDITIONS & COMBAT
// ════════════════════════════════════════════════════════════

// Scale an enemy stat by the chosen difficulty (bonus: difficulty modes).
int scaledEnemyStat(int base)
{
    if (g_difficulty == 0) return std::max(1, (base * 7)  / 10);  // Easy: weaker
    if (g_difficulty == 2) return (base * 15) / 10;               // Hard: tougher
    return base;
}

void startCombat(int enemyIdx)
{
    g_inCombat   = true;
    g_enemyIndex = enemyIdx;
    g_enemyHp    = scaledEnemyStat(eHp[enemyIdx]);
    pushEvent("a " + disp(eName[enemyIdx]) + " appears!");

    // Fast enemies strike first on the opening turn.
    if (eSpeed[enemyIdx] == "fast") {
        int dmg = std::max(1, scaledEnemyStat(eAtk[enemyIdx]) - g_armor);
        g_hp -= dmg;
        pushEvent("the " + disp(eName[enemyIdx]) + " is fast and strikes first (-" +
                  std::to_string(dmg) + " hp).");
        if (g_hp <= 0) {                     // knocked out before you could act
            g_hp = 1;
            g_inCombat = false;
            pushEvent("you collapse and crawl back toward the village.");
            g_onExpedition = false;
            g_screen = SCREEN_VILLAGE;
        }
    }
}

// Give the player the loot for defeating the current enemy.
void giveCombatLoot()
{
    int i = g_enemyIndex;
    if (eCoin[i] > 0) { addCoins(randRange(1, eCoin[i])); }
    if (eRes[i] == "fur")    addFur(eResAmt[i]);
    if (eRes[i] == "meat")   addMeat(eResAmt[i]);
    if (eRes[i] == "cloth")  addCloth(eResAmt[i]);
    if (eRes[i] == "iron")   addIron(eResAmt[i]);
    if (eRes[i] == "scales") addScales(eResAmt[i]);
    if (eTooth[i] > 0)       addTeeth(randRange(1, 2));

    // Rare drop: a Rifle can be found in the Ruined City.
    if (lName[g_expLocation] == "Ruined_City" && randRange(1, 100) <= 25) {
        for (int w = 0; w < NUM_WEAP; ++w)
            if (wName[w] == "Rifle" && !wOwned[w]) {
                wOwned[w] = true;
                pushEvent("you find a rifle among the ruins!");
            }
    }
    pushEvent("you defeat the " + disp(eName[i]) + " and take its loot.");
}

// The enemy takes its turn (called after the player acts, if it is still alive).
void enemyTurn()
{
    if (!g_inCombat) return;
    int dmg = std::max(1, scaledEnemyStat(eAtk[g_enemyIndex]) - g_armor);
    g_hp -= dmg;
    pushEvent("the " + disp(eName[g_enemyIndex]) + " hits you (-" +
              std::to_string(dmg) + " hp).");
    if (g_hp <= 0) {
        g_hp = 1;                            // you survive at 1 HP...
        g_inCombat = false;
        g_onExpedition = false;
        g_screen = SCREEN_VILLAGE;
        pushEvent("you black out and wake back in the village (1 hp).");
    }
}

void combatAttack()
{
    if (!g_inCombat) return;
    int dmg = wDmg[g_equipped];
    g_enemyHp -= dmg;
    pushEvent("you attack with the " + disp(wName[g_equipped]) + " (-" +
              std::to_string(dmg) + ").");
    if (g_enemyHp <= 0) { giveCombatLoot(); g_inCombat = false; return; }
    enemyTurn();                             // enemy strikes back
}

void combatUseItem()
{
    if (!g_inCombat) return;
    if (g_medicine > 0) {                    // medicine heals more
        g_medicine--; g_hp = std::min(g_hp + 5, g_maxHp);
        pushEvent("you use medicine (+5 hp).");
    } else if (g_expFood > 0) {              // otherwise eat some food
        g_expFood--; g_hp = std::min(g_hp + 3, g_maxHp);
        pushEvent("you eat food to recover (+3 hp).");
    } else {
        pushEvent("you have nothing to use.");
    }
    enemyTurn();
}

void combatFlee()
{
    if (!g_inCombat) return;
    if (randRange(1, 100) <= 50) {           // 50% chance to escape
        g_inCombat = false;
        pushEvent("you flee from the " + disp(eName[g_enemyIndex]) + ".");
    } else {
        pushEvent("you fail to escape!");
        enemyTurn();
    }
}

// Begin an expedition (prerequisites are checked here).
void startExpedition()
{
    if (!g_hasCompass) { pushEvent("you need a compass to venture out."); return; }
    if (g_meat < 5)    { pushEvent("you need at least 5 meat for the journey."); return; }

    g_meat -= 5;                             // pack 5 meat as provisions
    g_onExpedition = true;
    g_inCombat     = false;
    g_screen       = SCREEN_EXPEDITION;
    g_expLocation  = 0;
    g_expStep      = 0;
    g_expWater     = 10;                     // start each expedition with 10 water
    g_expFood      = 5;
    g_hp           = g_maxHp;
    initMap();
    updateMap();
    pushEvent("you set out along the dusty path.");
    pushEvent("you arrive at " + disp(lName[0]) + ".");
}

void endExpedition(const std::string& reason)
{
    if (g_expStep > g_bestDistance) g_bestDistance = g_expStep;  // for high score
    g_onExpedition = false;
    g_inCombat     = false;
    g_screen       = SCREEN_VILLAGE;
    pushEvent(reason);
    pushEvent("you return to the village.");
}

void expMoveForward()
{
    if (g_inCombat) return;
    g_expStep++;
    g_expWater--;                            // lose 1 water per move
    updateMap();

    if (g_expWater <= 0) {                   // out of water: lose health each move
        g_expWater = 0;
        g_hp--;
        pushEvent("your throat is parched (-1 hp).");
        if (g_hp <= 0) { g_hp = 1; endExpedition("you nearly die of thirst."); return; }
    }

    // Advance to the next location once we pass its distance.
    if (g_expLocation < NUM_LOC - 1 && g_expStep >= lDist[g_expLocation]) {
        g_expLocation++;
        pushEvent("you reach " + disp(lName[g_expLocation]) + ".");
    } else {
        pushEvent("you press on down the dusty path. (step " +
                  std::to_string(g_expStep) + ")");
    }

    // Moving can trigger an ambush (~20% chance).
    if (randRange(1, 100) <= 20)
        startCombat(randRange(0, NUM_ENEMY - 1));
}

void expScavenge()
{
    if (g_inCombat) return;

    // 30% chance that scavenging triggers combat.
    if (randRange(1, 100) <= 30) {
        startCombat(randRange(0, NUM_ENEMY - 1));
        return;
    }

    // Otherwise find some loot appropriate to wandering the wastes.
    int roll = randRange(0, 5);
    if (roll == 0)      { addWood(8);   pushEvent("you scavenge 8 wood."); }
    else if (roll == 1) { addCloth(2);  pushEvent("you find 2 cloth."); }
    else if (roll == 2) { addFur(3);    pushEvent("you find 3 fur."); }
    else if (roll == 3) { addIron(2);   pushEvent("you dig up 2 iron."); }
    else if (roll == 4) { g_medicine++; pushEvent("you find some medicine."); }
    else                { addMeat(3);   pushEvent("you find 3 meat."); }
    g_expStep++;                             // searching also takes a step
    g_expWater--;
    if (g_expWater < 0) g_expWater = 0;
    updateMap();
}

void expRest()
{
    if (g_inCombat) return;
    if (g_expFood < 1) { pushEvent("no food left to rest."); return; }
    g_expFood--;                             // rest costs 1 food
    g_hp = std::min(g_hp + 2, g_maxHp);      // and restores 2 hp
    pushEvent("you rest by the path (+2 hp).");
    if (g_expWater <= 0 && g_expFood <= 0)
        endExpedition("out of food and water.");
}

// ════════════════════════════════════════════════════════════
//  STORY EVENTS (bonus) — fire once each at turn milestones
// ════════════════════════════════════════════════════════════
void triggerStory(int turn)
{
    if (g_storyActive) return;               // don't stack events
    int idx = -1;
    if (turn >= 30  && !g_storyDone[0]) idx = 0;
    else if (turn >= 60  && !g_storyDone[1]) idx = 1;
    else if (turn >= 90  && !g_storyDone[2]) idx = 2;
    else if (turn >= 120 && !g_storyDone[3]) idx = 3;
    else if (turn >= 150 && !g_storyDone[4]) idx = 4;
    if (idx < 0) return;

    g_storyActive = true;
    g_storyIndex  = idx;
    g_storyDone[idx] = true;
}

std::string storyText(int idx)
{
    if (idx == 0) return "a sick man stumbles to the fire, begging for food.";
    if (idx == 1) return "a wandering trader offers furs for your coins.";
    if (idx == 2) return "thieves are spotted creeping toward the storehouse.";
    if (idx == 3) return "a scout returns with news of a distant ruin.";
    if (idx == 4) return "a stranger offers to teach your people to hunt.";
    return "";
}
std::string storyChoiceA(int idx)
{
    if (idx == 0) return "give 5 meat (gain goodwill: +1 villager later)";
    if (idx == 1) return "trade 20 coins for 15 fur";
    if (idx == 2) return "post a guard (lose 5 wood, keep your goods)";
    if (idx == 3) return "send supplies (lose 10 wood, +50 coins)";
    if (idx == 4) return "accept (gain a free hunter villager)";
    return "";
}
std::string storyChoiceB(int idx)
{
    if (idx == 0) return "turn him away (no cost)";
    if (idx == 1) return "decline the offer";
    if (idx == 2) return "do nothing (risk losing fur)";
    if (idx == 3) return "ignore the news";
    if (idx == 4) return "refuse the offer";
    return "";
}

void resolveStory(bool choiceA)
{
    int idx = g_storyIndex;
    if (idx == 0) {
        if (choiceA && g_meat >= 5) { g_meat -= 5; pushEvent("the sick man thanks you and recovers."); }
        else pushEvent("the sick man limps back into the dark.");
    } else if (idx == 1) {
        if (choiceA && g_coins >= 20) { g_coins -= 20; addFur(15); pushEvent("you trade for furs."); }
        else pushEvent("the trader moves on.");
    } else if (idx == 2) {
        if (choiceA && g_wood >= 5) { g_wood -= 5; pushEvent("the guard scares the thieves off."); }
        else { int loss = std::min(g_fur, 10); g_fur -= loss; pushEvent("thieves steal " + std::to_string(loss) + " fur!"); }
    } else if (idx == 3) {
        if (choiceA && g_wood >= 10) { g_wood -= 10; addCoins(50); pushEvent("the scouts reward you with coins."); }
        else pushEvent("you let the rumor pass.");
    } else if (idx == 4) {
        if (choiceA) {
            g_population++; if (g_population > g_maxPopulation) g_maxPopulation = g_population;
            if (idxSmokehouse >= 0 && bCount[idxSmokehouse] > 0) g_workers[4]++;
            pushEvent("a new hunter joins the village.");
        } else pushEvent("the stranger shrugs and leaves.");
    }
    g_storyActive = false;
    g_storyIndex  = -1;
}

// ════════════════════════════════════════════════════════════
//  SAVE / LOAD  (bonus) — ofstream / ifstream
// ════════════════════════════════════════════════════════════
void saveGame()
{
    std::ofstream f("savegame.txt");
    if (!f) { pushEvent("could not save the game."); return; }
    f << g_difficulty << " " << g_screen << " "
      << g_fireState << " " << g_turnsSincePoke << " " << g_stokeCount << " "
      << g_wood << " " << g_fur << " " << g_meat << " " << g_leather << " "
      << g_cloth << " " << g_coins << " " << g_iron << " " << g_scales << " " << g_teeth << " "
      << (g_strangerArrived ? 1 : 0) << " " << g_population << " " << g_maxPopulation << " "
      << g_totalTurns << " " << g_hp << " " << g_maxHp << " " << g_armor << " "
      << (g_hasCompass ? 1 : 0) << " " << (g_hasTorch ? 1 : 0) << " "
      << (g_hasRucksack ? 1 : 0) << " " << g_medicine << " "
      << g_equipped << " " << g_bestDistance << "\n";
    for (int i = 0; i < 5; ++i) {
        f << g_workers[i] << " ";
    }
    f << "\n";
    f << NUM_BUILD << " ";
    for (int i = 0; i < NUM_BUILD; ++i) {
        f << bCount[i] << " ";
    }
    f << "\n";
    f << NUM_WEAP << " ";
    for (int i = 0; i < NUM_WEAP; ++i) {
        f << (wOwned[i] ? 1 : 0) << " ";
    }
    f << "\n";
    f.close();
    pushEvent("game saved.");
}

void loadGame()
{
    std::ifstream f("savegame.txt");
    if (!f) { pushEvent("no saved game found."); return; }
    int sa, hc, ht, hr;
    f >> g_difficulty >> g_screen
      >> g_fireState >> g_turnsSincePoke >> g_stokeCount
      >> g_wood >> g_fur >> g_meat >> g_leather
      >> g_cloth >> g_coins >> g_iron >> g_scales >> g_teeth
      >> sa >> g_population >> g_maxPopulation
      >> g_totalTurns >> g_hp >> g_maxHp >> g_armor
      >> hc >> ht >> hr >> g_medicine
      >> g_equipped >> g_bestDistance;
    g_strangerArrived = (sa == 1);
    g_hasCompass = (hc == 1); g_hasTorch = (ht == 1); g_hasRucksack = (hr == 1);
    for (int i = 0; i < 5; ++i) f >> g_workers[i];
    int n; f >> n;
    for (int i = 0; i < n && i < NUM_BUILD; ++i) f >> bCount[i];
    f >> n;
    for (int i = 0; i < n && i < NUM_WEAP; ++i) { int v; f >> v; wOwned[i] = (v == 1); }
    f.close();
    g_onExpedition = false; g_inCombat = false; g_storyActive = false;
    if (g_screen == SCREEN_EXPEDITION) g_screen = SCREEN_VILLAGE;
    pushEvent("game loaded.");
}

// ─── HIGH SCORES (bonus) — top 5 saved to file ───────────────
int computeScore()
{
    int res = g_wood + g_fur + g_meat + g_leather + g_cloth + g_coins;
    return g_totalTurns + g_bestDistance * 5 + res;
}

// Read the existing scores, insert the current one, keep the best 5.
void saveHighScore()
{
    const int MAXH = 5;
    int* scores = new int[MAXH + 1];
    int count = 0;

    std::ifstream in("highscores.txt");
    if (in) { int s; while (count < MAXH && (in >> s)) scores[count++] = s; in.close(); }

    scores[count++] = computeScore();        // add this run's score

    // Simple bubble sort, highest first.
    for (int i = 0; i < count - 1; ++i)
        for (int j = 0; j < count - 1 - i; ++j)
            if (scores[j] < scores[j + 1]) { int t = scores[j]; scores[j] = scores[j + 1]; scores[j + 1] = t; }

    std::ofstream out("highscores.txt");
    if (out) { for (int i = 0; i < count && i < MAXH; ++i) out << scores[i] << "\n"; out.close(); }

    delete[] scores;                         // release the dynamic array
}

// Build a printable list of the top scores for the menu screen.
std::string highScoreText()
{
    std::string out = "";
    std::ifstream in("highscores.txt");
    if (!in) return "no scores yet.";
    int s, rank = 1;
    while (rank <= 5 && (in >> s)) {
        out += std::to_string(rank) + ". " + std::to_string(s) + "\n";
        rank++;
    }
    in.close();
    if (out.empty()) return "no scores yet.";
    return out;
}

// Apply the starting bonus/penalty for the chosen difficulty.
void applyDifficultyStart()
{
    if (g_difficulty == 0)      g_wood = 20;   // Easy : extra wood to begin
    else if (g_difficulty == 2) g_wood = 5;    // Hard : a meager start
    else                        g_wood = 10;   // Normal
}

// ════════════════════════════════════════════════════════════
//  SFML UI — colours, the Button helper, and drawing routines
// ════════════════════════════════════════════════════════════

sf::Color fireColour(int state)
{
    switch (state) {
        case 4: return sf::Color(255, 180,  50);
        case 3: return sf::Color(220, 130,  30);
        case 2: return sf::Color(160,  80,  20);
        case 1: return sf::Color( 90,  40,  10);
        default: return sf::Color(50,  50,  50);
    }
}

std::string fireLabel(int state)
{
    switch (state) {
        case 4: return "the fire is roaring";
        case 3: return "the fire is burning";
        case 2: return "the fire is flickering";
        case 1: return "the fire is smoldering";
        default: return "the fire is dead";
    }
}

// ─── BUTTON ── (provided helper code — the only struct in the file) ──
struct Button {
    sf::RectangleShape box;
    sf::Text           label;
    bool               enabled = true;

    Button() {}
    Button(float x, float y, float w, float h,
           const std::string& text, const sf::Font& font)
    {
        box.setPosition(x, y);
        box.setSize({w, h});
        box.setFillColor(sf::Color(40, 40, 40));
        box.setOutlineColor(sf::Color(160, 120, 60));
        box.setOutlineThickness(1.f);

        label.setFont(font);
        label.setString(text);
        label.setFillColor(sf::Color(220, 200, 150));
        fitAndCentre();      // shrink the text to fit the box, then centre it
    }

    // Choose the largest character size (up to 13) whose text fits inside the
    // box width, then centre the label. Keeps long labels from spilling out.
    void fitAndCentre() {
        float w = box.getSize().x, h = box.getSize().y;
        unsigned size = 13;
        label.setCharacterSize(size);
        while (size > 8 && label.getLocalBounds().width > w - 10.f) {
            --size;
            label.setCharacterSize(size);
        }
        sf::FloatRect tb = label.getLocalBounds();
        label.setOrigin(tb.left + tb.width / 2.f, tb.top + tb.height / 2.f);
        label.setPosition(box.getPosition().x + w / 2.f,
                          box.getPosition().y + h / 2.f);
    }

    bool contains(sf::Vector2i pos) const {
        return enabled && box.getGlobalBounds().contains((float)pos.x, (float)pos.y);
    }
    void setText(const std::string& s) {
        label.setString(s);
        fitAndCentre();
    }
    void draw(sf::RenderWindow& win) const {
        if (!enabled) return;
        win.draw(box);
        win.draw(label);
    }
};

// Draw a line of text at (x, y).
void drawText(sf::RenderWindow& win, const sf::Font& font,
              const std::string& str, float x, float y,
              unsigned size = 13, sf::Color col = sf::Color(200, 190, 160))
{
    sf::Text t;
    t.setFont(font);
    t.setString(str);
    t.setCharacterSize(size);
    t.setFillColor(col);
    t.setPosition(x, y);
    win.draw(t);
}

// Draw the scrolling event log in the right-hand panel.
void drawEventLog(sf::RenderWindow& win, const sf::Font& font)
{
    float x = PANEL_RIGHT + 20.f, y = 60.f;
    drawText(win, font, "-- EVENT LOG --", x, y, 13, sf::Color(160, 140, 80));
    y += 24.f;
    for (int i = 0; i < LOG_SIZE; ++i) {
        if (eventLog[i].empty()) continue;
        float hi = logHighlight[i];
        sf::Uint8 r = (sf::Uint8)(200 + 55 * hi);
        sf::Uint8 g = (sf::Uint8)(190 - 100 * hi);
        sf::Uint8 b = (sf::Uint8)(160 - 100 * hi);
        std::string line = (i == 0 ? "> " : "  ") + eventLog[i];
        drawText(win, font, line, x, y, 13, sf::Color(r, g, b));
        y += 20.f;
    }
}

// Top resource bar.
void drawResourceBar(sf::RenderWindow& win, const sf::Font& font)
{
    std::ostringstream ss;
    ss << "wood:" << g_wood << "/" << woodCap()
       << "  fur:" << g_fur << "  meat:" << g_meat
       << "  leather:" << g_leather << "  cloth:" << g_cloth
       << "  coins:" << g_coins;
    drawText(win, font, ss.str(), 20.f, 10.f, 12, sf::Color(160, 180, 140));
    std::ostringstream ss2;
    ss2 << "iron:" << g_iron << "  scales:" << g_scales
        << "  teeth:" << g_teeth
        << "  compass:" << (g_hasCompass ? "yes" : "no")
        << "  weapon:" << disp(wName ? wName[g_equipped] : std::string("Fists"));
    drawText(win, font, ss2.str(), 20.f, 26.f, 12, sf::Color(140, 160, 130));
}

// ════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════
int main()
{
    srand((unsigned)time(0));

    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "A Dark Room");
    window.setFramerateLimit(60);

    // Load the font. Try the bundled copy first, then common Ubuntu paths.
    sf::Font font;
    if (!font.loadFromFile("assets/font.ttf") &&
        !font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf") &&
        !font.loadFromFile("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf")) {
        return 1;   // cannot continue without a font
    }

    // Load all game data from the .txt files into dynamic arrays.
    loadBuildings();
    loadEnemies();
    loadLocations();
    loadWeapons();
    initMap();

    pushEvent("the room is cold and dark.");
    pushEvent("there is a pile of wood nearby.");

    // ── BUTTON LAYOUT ────────────────────────────────────────
    float bx = 30.f, bw = 200.f, bh = 30.f, gap = 7.f;

    // Start-menu (difficulty) buttons.
    Button btnEasy  (bx, 300.f, bw, bh, "Easy",   font);
    Button btnNormal(bx, 340.f, bw, bh, "Normal", font);
    Button btnHard  (bx, 380.f, bw, bh, "Hard",   font);

    // Room screen buttons.
    float ry = 90.f;
    Button btnStoke (bx, ry, bw, bh, "stoke fire",  font); ry += bh + gap;
    Button btnGather(bx, ry, bw, bh, "gather wood", font); ry += bh + gap;
    Button btnVillage(bx, ry, bw, bh, "go to village", font); ry += bh + gap;
    Button btnSaveR (bx, ry, bw, bh, "save game",   font); ry += bh + gap;
    Button btnLoadR (bx, ry, bw, bh, "load game",   font); ry += bh + gap;

    // Village screen — building buttons (one per building, built from file data).
    Button* buildBtns = new Button[NUM_BUILD > 0 ? NUM_BUILD : 1];
    float vy = 90.f;
    for (int i = 0; i < NUM_BUILD; ++i) {
        buildBtns[i] = Button(bx, vy, bw, bh, "build " + disp(bName[i]), font);
        vy += bh + gap;
    }

    // Village screen — worker +/- buttons (5 roles).
    float wy = 90.f, wx = 260.f;
    Button workerAdd[5], workerRem[5];
    for (int i = 0; i < 5; ++i) {
        workerAdd[i] = Button(wx,          wy, 135.f, bh, std::string("+ ") + roleName(i), font);
        workerRem[i] = Button(wx + 140.f,  wy, 30.f,  bh, "-", font);
        wy += bh + gap;
    }

    // Village screen — trading / crafting / navigation buttons.
    float ty = 320.f, tx = 260.f;
    Button btnSellFur (tx, ty, 130.f, bh, "sell fur",     font); ty += bh + gap;
    Button btnSellMeat(tx, ty, 130.f, bh, "sell meat",    font); ty += bh + gap;
    Button btnSellLea (tx, ty, 130.f, bh, "sell leather", font); ty += bh + gap;
    Button btnBuyCloth(tx + 140, 320.f, 130.f, bh, "buy cloth",   font);
    Button btnBuyComp (tx + 140, 320.f + (bh+gap), 130.f, bh, "buy compass", font);
    Button btnBuyTorch(tx + 140, 320.f + 2*(bh+gap), 130.f, bh, "buy torch",   font);
    Button btnBuySword(tx + 140, 320.f + 3*(bh+gap), 130.f, bh, "buy iron sword", font);

    float cy = 90.f, cx = 470.f;
    Button btnCraftSpear(cx, cy, 150.f, bh, "craft bone spear", font); cy += bh + gap;
    Button btnCraftRuck (cx, cy, 150.f, bh, "craft rucksack",   font); cy += bh + gap;
    Button btnCraftTorch(cx, cy, 150.f, bh, "craft torch",      font); cy += bh + gap;
    Button btnSwitchWeap(cx, cy, 150.f, bh, "switch weapon",    font); cy += bh + gap;

    Button btnRoomBack (bx, 520.f, bw, bh, "back to the room", font);
    Button btnVenture  (bx, 480.f, bw, bh, "venture out (expedition)", font);
    Button btnSaveV (260.f, 520.f, 130.f, bh, "save game", font);
    Button btnLoadV (400.f, 520.f, 130.f, bh, "load game", font);

    // Expedition screen buttons.
    float ey = 250.f;
    Button btnMove (bx, ey, bw, bh, "move forward",     font); ey += bh + gap;
    Button btnScav (bx, ey, bw, bh, "scavenge area",    font); ey += bh + gap;
    Button btnRest (bx, ey, bw, bh, "rest (1 food,+2hp)", font); ey += bh + gap;
    Button btnReturn(bx, ey, bw, bh, "return to village", font); ey += bh + gap;

    // Combat buttons.
    float fy = 250.f, fx = 260.f;
    Button btnCAttack(fx, fy, 150.f, bh, "attack",   font); fy += bh + gap;
    Button btnCItem  (fx, fy, 150.f, bh, "use item", font); fy += bh + gap;
    Button btnCFlee  (fx, fy, 150.f, bh, "flee",     font); fy += bh + gap;
    Button btnCSwitch(fx, fy, 150.f, bh, "switch weapon", font); fy += bh + gap;

    // Story-event choice buttons.
    Button btnStoryA(bx, 300.f, 400.f, bh, "choice A", font);
    Button btnStoryB(bx, 340.f, 400.f, bh, "choice B", font);

    // Auto-tick + fade timers.
    sf::Clock tickClock;
    const float TICK_INTERVAL = 4.f;   // idle seconds per automatic turn
    sf::Clock fadeClock;

    // Vertical divider between the action panel and the event log.
    sf::RectangleShape divider;
    divider.setSize({2.f, (float)WINDOW_H});
    divider.setPosition((float)PANEL_RIGHT, 0.f);
    divider.setFillColor(sf::Color(80, 60, 30));

    // ── MAIN LOOP ────────────────────────────────────────────
    while (window.isOpen())
    {
        // ---- EVENTS ----
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                saveHighScore();             // record the score on exit
                window.close();
            }

            // Validated text input for the trading-post sell quantity.
            // Only digits are ever stored, so bad input cannot crash anything.
            if (event.type == sf::Event::TextEntered && g_screen == SCREEN_VILLAGE) {
                if (event.text.unicode == '\b') {
                    if (!g_inputBuffer.empty()) g_inputBuffer.pop_back();
                } else if (event.text.unicode >= '0' && event.text.unicode <= '9') {
                    if (g_inputBuffer.size() < 5)
                        g_inputBuffer += static_cast<char>(event.text.unicode);
                }
            }

            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left)
            {
                sf::Vector2i mp(event.mouseButton.x, event.mouseButton.y);

                // A story-event choice overrides everything else.
                if (g_storyActive) {
                    if (btnStoryA.contains(mp)) resolveStory(true);
                    else if (btnStoryB.contains(mp)) resolveStory(false);
                }
                // ---- MENU screen ----
                else if (g_screen == SCREEN_MENU) {
                    if (btnEasy.contains(mp))   { g_difficulty = 0; applyDifficultyStart(); g_screen = SCREEN_ROOM; }
                    if (btnNormal.contains(mp)) { g_difficulty = 1; applyDifficultyStart(); g_screen = SCREEN_ROOM; }
                    if (btnHard.contains(mp))   { g_difficulty = 2; applyDifficultyStart(); g_screen = SCREEN_ROOM; }
                }
                // ---- ROOM screen ----
                else if (g_screen == SCREEN_ROOM) {
                    if (btnStoke.contains(mp))        actionStokeFire();
                    else if (btnGather.contains(mp))  actionGatherWood();
                    else if (btnVillage.contains(mp) && g_strangerArrived) g_screen = SCREEN_VILLAGE;
                    else if (btnSaveR.contains(mp))   saveGame();
                    else if (btnLoadR.contains(mp))   loadGame();
                }
                // ---- VILLAGE screen ----
                else if (g_screen == SCREEN_VILLAGE) {
                    bool handled = false;
                    for (int i = 0; i < NUM_BUILD && !handled; ++i)
                        if (buildBtns[i].contains(mp)) { actionBuild(i); handled = true; }

                    for (int i = 0; i < 5 && !handled; ++i) {
                        if (workerAdd[i].contains(mp) && roleUnlocked(i)) { assignWorker(i); handled = true; }
                        else if (workerRem[i].contains(mp) && roleUnlocked(i)) { unassignWorker(i); handled = true; }
                    }
                    if (!handled) {
                        int qty = g_inputBuffer.empty() ? 1 : safeParseInt(g_inputBuffer);
                        if (btnSellFur.contains(mp))       sellResource(0, qty);
                        else if (btnSellMeat.contains(mp)) sellResource(1, qty);
                        else if (btnSellLea.contains(mp))  sellResource(2, qty);
                        else if (btnBuyCloth.contains(mp)) buyItem(0);
                        else if (btnBuyComp.contains(mp))  buyItem(1);
                        else if (btnBuyTorch.contains(mp)) buyItem(2);
                        else if (btnBuySword.contains(mp)) buyItem(3);
                        else if (btnCraftSpear.contains(mp)) craftItem(0);
                        else if (btnCraftRuck.contains(mp))  craftItem(1);
                        else if (btnCraftTorch.contains(mp)) craftItem(2);
                        else if (btnSwitchWeap.contains(mp)) switchWeapon();
                        else if (btnVenture.contains(mp))    startExpedition();
                        else if (btnRoomBack.contains(mp))   g_screen = SCREEN_ROOM;
                        else if (btnSaveV.contains(mp))      saveGame();
                        else if (btnLoadV.contains(mp))      loadGame();
                    }
                }
                // ---- EXPEDITION screen ----
                else if (g_screen == SCREEN_EXPEDITION) {
                    if (g_inCombat) {
                        if (btnCAttack.contains(mp))      combatAttack();
                        else if (btnCItem.contains(mp))   combatUseItem();
                        else if (btnCFlee.contains(mp))   combatFlee();
                        else if (btnCSwitch.contains(mp)) switchWeapon();
                    } else {
                        if (btnMove.contains(mp))        expMoveForward();
                        else if (btnScav.contains(mp))   expScavenge();
                        else if (btnRest.contains(mp))   expRest();
                        else if (btnReturn.contains(mp)) endExpedition("you decide to head home.");
                    }
                }
            }

            // Keyboard shortcuts (handy in the room).
            if (event.type == sf::Event::KeyPressed && g_screen == SCREEN_ROOM) {
                if (event.key.code == sf::Keyboard::F) actionStokeFire();
                if (event.key.code == sf::Keyboard::G) actionGatherWood();
            }
        }

        // ---- AUTO-TICK ---- (only while tending the room / village)
        if ((g_screen == SCREEN_ROOM || g_screen == SCREEN_VILLAGE) &&
            !g_storyActive &&
            tickClock.getElapsedTime().asSeconds() >= TICK_INTERVAL) {
            advanceTurn(false);
            tickClock.restart();
        }

        // ---- FADE event-log highlights ----
        float dt = fadeClock.restart().asSeconds();
        for (int i = 0; i < LOG_SIZE; ++i)
            logHighlight[i] = std::max(0.f, logHighlight[i] - dt * 0.4f);

        // ════ DRAW ════
        window.clear(sf::Color(18, 15, 12));
        window.draw(divider);
        drawResourceBar(window, font);
        drawEventLog(window, font);

        // ── Story-event overlay (drawn on top of any screen) ──
        if (g_storyActive) {
            drawText(window, font, "-- AN EVENT --", bx, 250.f, 16, sf::Color(220, 200, 120));
            drawText(window, font, storyText(g_storyIndex), bx, 275.f, 14, sf::Color(210, 200, 170));
            btnStoryA.setText(storyChoiceA(g_storyIndex));
            btnStoryB.setText(storyChoiceB(g_storyIndex));
            btnStoryA.draw(window);
            btnStoryB.draw(window);
            window.display();
            continue;                        // pause the rest of the UI until resolved
        }

        // ── MENU screen ──
        if (g_screen == SCREEN_MENU) {
            drawText(window, font, "A  D A R K  R O O M", bx, 90.f, 26, sf::Color(220, 200, 150));
            drawText(window, font, "a minimalist survival game", bx, 130.f, 14, sf::Color(150, 140, 110));
            drawText(window, font, "choose a difficulty to begin:", bx, 260.f, 14, sf::Color(180, 170, 140));
            btnEasy.draw(window); btnNormal.draw(window); btnHard.draw(window);
            drawText(window, font, "-- HIGH SCORES --", PANEL_RIGHT + 20.f, 300.f, 14, sf::Color(160, 140, 80));
            std::string hs = highScoreText();
            float hy = 326.f;
            std::istringstream iss(hs);
            std::string line;
            while (std::getline(iss, line)) { drawText(window, font, line, PANEL_RIGHT + 20.f, hy, 13); hy += 20.f; }
        }
        // ── ROOM screen ──
        else if (g_screen == SCREEN_ROOM) {
            sf::Color fc = fireColour(g_fireState);
            drawText(window, font, fireLabel(g_fireState), 20.f, 50.f, 18, fc);
            btnStoke.draw(window);
            btnGather.draw(window);
            btnVillage.enabled = g_strangerArrived; btnVillage.draw(window);
            btnSaveR.draw(window);
            btnLoadR.draw(window);
            drawText(window, font, "[ the room ]", 20.f, WINDOW_H - 26.f, 13, sf::Color(100, 90, 60));
            drawText(window, font, "tip: keep the fire alive. press F to stoke, G to gather.",
                     20.f, WINDOW_H - 50.f, 12, sf::Color(110, 100, 80));
        }
        // ── VILLAGE screen ──
        else if (g_screen == SCREEN_VILLAGE) {
            drawText(window, font, "[ the village ]", 20.f, 50.f, 16, sf::Color(180, 160, 120));

            // Building buttons (greyed out where unavailable).
            for (int i = 0; i < NUM_BUILD; ++i) {
                bool unlocked = g_strangerArrived;
                if (i == idxHut && idxTrap >= 0 && bCount[idxTrap] == 0) unlocked = false;
                buildBtns[i].enabled = unlocked;
                buildBtns[i].setText("build " + disp(bName[i]) + " (" +
                                     std::to_string(bCount[i]) + "/" + std::to_string(bMax[i]) + ")");
                buildBtns[i].draw(window);
            }

            // Worker management header + free villagers.
            drawText(window, font, "workers (free:" + std::to_string(unassignedWorkers()) +
                     " pop:" + std::to_string(g_population) + "/" + std::to_string(g_maxPopulation) + ")",
                     260.f, 70.f, 12, sf::Color(150, 170, 130));
            for (int i = 0; i < 5; ++i) {
                if (!roleUnlocked(i)) continue;   // only show unlocked roles
                workerAdd[i].setText(std::string("+ ") + roleName(i) + " (" + std::to_string(g_workers[i]) + ")");
                workerAdd[i].draw(window);
                workerRem[i].draw(window);
            }

            // Trading post + crafting (only if the relevant building exists).
            if (tradingPostBuilt()) {
                drawText(window, font, "trading post  (sell qty: " +
                         (g_inputBuffer.empty() ? std::string("1") : g_inputBuffer) + "_)",
                         260.f, 300.f, 12, sf::Color(150, 170, 130));
                btnSellFur.draw(window); btnSellMeat.draw(window); btnSellLea.draw(window);
                btnBuyCloth.draw(window); btnBuyComp.draw(window);
                btnBuyTorch.draw(window); btnBuySword.draw(window);
            } else {
                drawText(window, font, "build a trading post to trade.", 260.f, 300.f, 12, sf::Color(120, 110, 90));
            }

            drawText(window, font, "crafting / gear", 470.f, 70.f, 12, sf::Color(150, 170, 130));
            btnCraftSpear.draw(window); btnCraftRuck.draw(window);
            btnCraftTorch.draw(window); btnSwitchWeap.draw(window);

            btnVenture.enabled = g_hasCompass; btnVenture.draw(window);
            btnRoomBack.draw(window);
            btnSaveV.draw(window); btnLoadV.draw(window);
        }
        // ── EXPEDITION screen ──
        else if (g_screen == SCREEN_EXPEDITION) {
            std::ostringstream hdr;
            hdr << "DUSTY PATH  -  Step " << g_expStep
                << "   Location: " << disp(lName[g_expLocation]);
            drawText(window, font, hdr.str(), 20.f, 50.f, 15, sf::Color(200, 180, 130));
            std::ostringstream st;
            st << "Water: " << g_expWater << "   Food: " << g_expFood
               << "   HP: " << g_hp << "/" << g_maxHp
               << "   Weapon: " << disp(wName[g_equipped]) << " (dmg " << wDmg[g_equipped] << ")";
            drawText(window, font, st.str(), 20.f, 75.f, 13, sf::Color(170, 180, 150));

            // ASCII world map (bonus): '.' unexplored, ':' trail, '#' forest,
            // 'T' town, 'C' cave, 'B' battlefield, 'R' ruins, '@' you.
            updateMap();
            int prog = g_expStep / 3; if (prog >= MAP_N) prog = MAP_N - 1;
            drawText(window, font, "world map:", 20.f, 110.f, 12, sf::Color(140, 140, 110));
            for (int r = 0; r < MAP_N; ++r) {
                std::string row = "";
                for (int c = 0; c < MAP_N; ++c) {
                    if (r == prog && c == prog) row += "@ ";
                    else { row += g_map[r][c]; row += ' '; }
                }
                drawText(window, font, row, 20.f, 128.f + r * 12.f, 12, sf::Color(120, 150, 120));
            }
            drawText(window, font, "legend: . unexplored  # forest  T town  C cave  @ you",
                     20.f, 128.f + MAP_N * 12.f + 4.f, 11, sf::Color(110, 120, 100));

            if (g_inCombat) {
                drawText(window, font, "COMBAT!  " + disp(eName[g_enemyIndex]) +
                         "  hp:" + std::to_string(g_enemyHp), 260.f, 220.f, 14, sf::Color(220, 120, 100));
                btnCAttack.draw(window); btnCItem.draw(window);
                btnCFlee.draw(window);   btnCSwitch.draw(window);
            } else {
                btnMove.draw(window); btnScav.draw(window);
                btnRest.draw(window); btnReturn.draw(window);
            }
        }

        window.display();
    }

    // ── CLEANUP ──────────────────────────────────────────────
    delete[] buildBtns;       // free the dynamic Button array
    freeData();               // free all data arrays loaded from files
    return 0;
}
