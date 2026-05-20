#include <LovyanGFX.hpp>
#include <SPI.h>

// ======================================================
// POCKET_SPRITE - ARCADE SHELL v2.0 MAIN MENU + ADVENTURE TITLE
// XIAO ESP32-S3 + ST7735 TFT + Joystick + A Button
//
// v1.6 LOVYANGFX PASS:
// - Switches display driver from Adafruit_ST7735 to LovyanGFX
// - Pre-renders common top-down character sprites into ESP32-S3 SRAM sprites
// - Keeps gameplay/state flow from the working V4 sketch
// - Adds main menu shell
// - Adventure starts from menu
// - Pocket Pet placeholder
// - Dress Up placeholder
// - Existing adventure preserved:
//   Level 1: Old School platformer, find blue hat
//   Level 2: Garden maze, find friend
//   Level 3: Bridge crossing, cross together
//
// Confirmed:
// TFT driver: LovyanGFX Panel_ST7735S
// TFT rotation: setRotation(3)
// Color order: rgb_order=true
// ======================================================


// -------------------- PIN MAP --------------------
#define JOY_SW_PIN   D0
#define JOY_X_PIN    D1
#define JOY_Y_PIN    D2
#define A_BUTTON_PIN D3

#define TFT_CS       D4
#define TFT_DC       D5
#define TFT_RST      D6
#define TFT_BL       D7
#define TFT_SCLK     D8
#define TFT_MOSI     D10


// -------------------- COLORS --------------------
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_BLUE        0x001F
#define C_SKY         0x5D9F
#define C_GREEN       0x07E0
#define C_DARK_GREEN  0x03E0
#define C_YELLOW      0xFFE0
#define C_MAGENTA     0xF81F
#define C_CYAN        0x07FF
#define C_RED         0xF800
#define C_ORANGE      0xFD20
#define C_TAN         0xD5B1
#define C_BROWN       0x8200
#define C_PINK        0xF81F
#define C_GRAY        0x8410
#define C_DARK_GRAY   0x4208
#define C_DARK_BLUE   0x0010
#define C_DOOR        0x5A20
#define C_GOLD        0xFEA0
#define C_SKIN        0xFDB8
#define C_GRASS       0x05E0
#define C_PATH        0xD69A
#define C_FLOWER      0xF81F
#define C_WATER       0x04FF
#define C_LEAF        0x07E0
#define C_BRIDGE      0xA145


// -------------------- DISPLAY --------------------
// LovyanGFX is faster than Adafruit_GFX on ESP32-S3 and lets us
// keep the display config inside this sketch instead of editing a
// library User_Setup file.
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7735S _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 27000000;      // Confirmed stable on POCKET_SPRITE ST7735S test.
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = TFT_SCLK;
      cfg.pin_mosi = TFT_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = TFT_RST;
      cfg.pin_busy = -1;

      cfg.panel_width = 128;
      cfg.panel_height = 160;
      cfg.memory_width = 128;
      cfg.memory_height = 160;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = true;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};

LGFX tft;

// Transparent key used only for SRAM sprite blits. Avoid 0x0000 because
// the game uses true black in the art.
#define C_TRANSPARENT_KEY 0x0001

LGFX_Sprite sprMadelynA(&tft);
LGFX_Sprite sprMadelynB(&tft);
LGFX_Sprite sprFriendA(&tft);
LGFX_Sprite sprFriendB(&tft);

// Pocket Pet sprites are also pre-rendered into ESP32-S3 SRAM.
// This keeps pet animation smoother under LovyanGFX.
LGFX_Sprite sprPetIdle(&tft);
LGFX_Sprite sprPetWalkA(&tft);
LGFX_Sprite sprPetWalkB(&tft);
bool commonSpritesReady = false;
bool petSpritesReady = false;


// ======================================================
// APP STATE
// ======================================================

enum AppState {
  BOOT,
  TITLE,
  MAIN_MENU,
  SCHOOL_ROOM,
  GARDEN_INTRO,
  GARDEN_MAZE,
  FRIEND_FOUND,
  BRIDGE_INTRO,
  BRIDGE_CROSSING,
  BRIDGE_CLEAR,
  CITY_INTRO,
  CITY_WALK,
  CITY_CLEAR,
  POCKET_PET,
  DRESS_UP,
  DRESS_PLAY,
  TRON_GAME,
  SNAKE_GAME
};

AppState appState = BOOT;

// ======================================================
// JOYSTICK / BUTTON STATE
// ======================================================

const int JOY_X_CENTER = 1970;
const int JOY_Y_CENTER = 2020;
const int JOY_DEADZONE = 450;

int joyX = 0;
int joyY = 0;

bool joyPressed = false;
bool aPressed = false;
bool lastJoyPressed = false;
bool lastAPressed = false;

String direction = "CENTER";
String lastDirection = "CENTER";

// ======================================================
// MENU / LEVEL 1 PLAYER STATE RESTORE
// ======================================================

const int MENU_COUNT = 5;
int menuIndex = 0;

float girlX = 12;
float girlY = 96;
float oldGirlX = 12;
float oldGirlY = 96;

float girlVX = 0;
float girlVY = 0;

const int girlW = 16;
const int girlH = 19;

const float groundMoveSpeed = 3.2;
const float airMoveSpeed = 2.35;
const float gravity = 2.4;
const float jumpVelocity = -13.2;
const float maxFallSpeed = 15.0;

bool onGround = false;
bool facingRight = true;
bool jumpHeld = false;

bool hatCollected = false;
bool oldHatCollected = false;

const int hatX = 96;
const int hatY = 62;
const int hatW = 13;
const int hatH = 8;

const int doorX = 138;
const int doorY = 82;
const int doorW = 16;
const int doorH = 36;

inline int girlDrawX() { return (int)girlX - 1; }
inline int girlDrawY() { return (int)girlY; }



// ======================================================
// FUNCTION PROTOTYPES
// Arduino sometimes fails to auto-prototype larger sketches.
// ======================================================

bool rectsOverlap(int ax, int ay, int aw, int ah,
                  int bx, int by, int bw, int bh);

void redrawSchoolPatch(int x, int y, int w, int h);

void drawBridgeFrame();
void drawBridgeClearScreen();
void drawBridgePair();
void drawBridgeMadelyn(int x, int y);
void drawBridgeFriend(int x, int y);
void drawBridgeLeaves();
void updateBridgeClear();
void drawCityIntro();
void updateCityIntro();
void startCityWalk();
void updateCityWalk();
void updateCityMovement();
bool cityHitsWall(int x, int y);
void checkCityGoal();
void drawCityScene();
void drawCityStatic();
void drawCityFrame();
void redrawCityPatch(int x, int y, int w, int h);
void drawCityPair();
void drawCityMadelyn(int x, int y);
void drawCityFriend(int x, int y);
void drawWrigleyDog(int x, int y);
void drawCityClearScreen();
void updateCityClear();
void startPocketPet();
void drawPocketPetRoom();
void updatePocketPet();
void updatePocketPetAButton();
void updatePocketPetPhysics();
void drawPocketPetFrame();
void redrawPocketPetPatch(int x, int y, int w, int h);
void drawPocketPetCreature(int x, int y);
void drawPocketPetToy();
void updatePocketPetBall();
void checkPocketPetBallInteractions();
void kickPocketPetBall();
void drawPocketPetBall();
void buildPetSprites();
void drawPetShadow(int x, int y);
void startDressUp();
void drawDressUpScreen();
void updateDressUp();
void nextDressStep();
void cycleDressValue(int delta);
void drawDressFacePreview(int x, int y);
void drawDressFullPreview(int x, int y, int scaleMode);
void drawDressGirlFace(int x, int y, bool closeMode);
void drawDressBoyFace(int x, int y, bool closeMode);
void drawDressGirlBody(int x, int y, bool closeMode);
void drawDressBoyBody(int x, int y, bool closeMode);
void drawGirlHair(int x, int y, uint16_t hairColor, bool closeMode);
void drawBoyHair(int x, int y, uint16_t hairColor, bool closeMode);
void drawBoyFacialHair(int x, int y, uint16_t hairColor, bool closeMode);
uint16_t getDressHairColor(int index);
uint16_t getDressOutfitColor(int index);
uint16_t getDressShoeColor(int index);
const char* getDressStepName();
const char* getDressValueName();
const char* getGirlHairName(int index);
const char* getBoyHairName(int index);
const char* getHairColorNameNew(int index);
const char* getGirlOutfitName(int index);
const char* getBoyOutfitName(int index);
const char* getOutfitColorNameNew(int index);
const char* getShoeColorName(int index);
const char* getBoyBeardName(int index);
const char* getBoyMustacheName(int index);
void startDressPlay();
void updateDressPlay();
void updateDressPlayPhysics();
void drawDressPlayScene();
void drawDressPlayFrame();
void redrawDressPlayPatch(int x, int y, int w, int h);
void drawDressPlayCharacter(int x, int y);
void startTronGame();
void updateTronGame();
void drawTronScene();
void drawTronHUD();
void resetTronRound();
void updateTronDirection();
void drawTronCell(int gx, int gy, uint16_t color);
bool tronCellBlocked(int gx, int gy);

void startSnakeGame();
void updateSnakeGame();
void drawSnakeScene();
void drawSnakeHUD();
void resetSnakeRound();
void updateSnakeDirection();
void drawSnakeCell(int gx, int gy, uint16_t color);
bool snakeHitsBody(int gx, int gy);
void placeSnakeFood();
;

// ======================================================
// SCREEN / ROOM CONSTANTS
// ======================================================

const int HEADER_H = 18;
const int GROUND_Y = 118;

struct Platform {
  int x;
  int y;
  int w;
  int h;
};

const int PLATFORM_COUNT = 3;
Platform platforms[PLATFORM_COUNT] = {
  {16, 92, 50, 6},
  {76, 72, 52, 6},
  {28, 54, 38, 6}
};

void drawSinglePlatform(Platform p);


// ======================================================
// RECT / OBSTACLE HELPERS
// ======================================================

struct Rect {
  int x;
  int y;
  int w;
  int h;
};


// ======================================================
// LEVEL 2 TOP-DOWN GARDEN STATE
// ======================================================

const int GARDEN_OBSTACLE_COUNT = 10;

Rect gardenWalls[GARDEN_OBSTACLE_COUNT] = {
  {0,   HEADER_H, 160, 5},
  {0,   123,      160, 5},
  {0,   HEADER_H, 5,   110},
  {155, HEADER_H, 5,   110},

  {18,  38,       62,  9},
  {98,  38,       42,  9},

  {18,  70,       38,  9},
  {76,  70,       64,  9},

  {38,  98,       62,  9},
  {118, 90,       9,   28}
};

int gardenX = 12;
int gardenY = 108;
int oldGardenX = 12;
int oldGardenY = 108;

const int gardenW = 10;
const int gardenH = 12;
const int gardenSpeed = 3;

const int friendX = 136;
const int friendY = 28;
const int friendW = 11;
const int friendH = 13;


// ======================================================
// LEVEL 3 BRIDGE STATE
// ======================================================

int bridgeX = 8;
int bridgeY = 72;
int oldBridgeX = 8;
int oldBridgeY = 72;

const int bridgeW = 10;
const int bridgeH = 12;
const int bridgeSpeed = 3;

const int bridgeGoalX = 145;
const int bridgeGoalY = 62;
const int bridgeGoalW = 10;
const int bridgeGoalH = 28;

const int BRIDGE_OBSTACLE_COUNT = 6;

Rect bridgeObstacles[BRIDGE_OBSTACLE_COUNT] = {
  {34,  60,  16, 10},
  {62,  90,  18, 10},
  {94,  54,  16, 10},
  {110, 82,  20, 10},
  {52,  74,  10, 8},
  {86,  76,  10, 8}
};

int windCounter = 0;
int windPush = 0;

// ======================================================
// LEVEL 4 CITY STATE
// ======================================================

int cityX = 10;
int cityY = 92;
int oldCityX = 10;
int oldCityY = 92;

const int cityW = 10;
const int cityH = 12;
const int citySpeed = 4;

const int cityGoalX = 132;
const int cityGoalY = 72;
const int cityGoalW = 18;
const int cityGoalH = 22;

const int CITY_OBSTACLE_COUNT = 7;

Rect cityObstacles[CITY_OBSTACLE_COUNT] = {
  {0,   HEADER_H, 160, 5},
  {0,   123,      160, 5},
  {0,   HEADER_H, 5,   110},
  {155, HEADER_H, 5,   110},

  {30,  39,       34,  26},
  {78,  86,       22,  18},
  {112, 38,       12,  30}
};

// ======================================================
// POCKET PET STATE
// ======================================================

int petX = 72;
int petY = 92;
int oldPetX = 72;
int oldPetY = 92;

float petVY = 0;
bool petOnGround = false;
bool petJumpHeld = false;
bool petKickHeld = false;

const int petW = 26;
const int petH = 22;
const int PET_GROUND_Y = 112;

const int petMoveSpeed = 4;
const float petGravity = 0.55;
const float petJumpVelocity = -11.2;
const float petMaxFall = 5.5;

bool petFacingRight = true;
int petAnim = 0;

// Pocket Pet controls:
// joystick UP = jump
// A button = kick ball
// joystick press = back to menu
// v2.3.1: robust A/UP controls
unsigned long petAPressStart = 0;
bool petAHoldKickDone = false;
const unsigned long PET_KICK_HOLD_MS = 520; // unused now, kept harmless for rollback

// Pocket Pet ball physics
float petBallX = 118;
float petBallY = 104;
float oldPetBallX = 118;
float oldPetBallY = 104;
float petBallVX = 0;
float petBallVY = 0;
const int petBallR = 7;


// ======================================================
// DRESS UP STATE
// ======================================================

enum DressGender {
  DRESS_GIRL,
  DRESS_BOY
};

enum DressStep {
  STEP_GENDER,
  STEP_HAIR,
  STEP_BEARD,
  STEP_MUSTACHE,
  STEP_HAIR_COLOR,
  STEP_OUTFIT,
  STEP_OUTFIT_COLOR,
  STEP_SHOES,
  STEP_CONFIRM
};

DressGender dressGender = DRESS_GIRL;
DressStep dressStep = STEP_GENDER;

int girlHairStyle = 0;
int girlHairColor = 0;
int girlOutfitStyle = 0;
int girlOutfitColor = 0;
int girlShoes = 0;

int boyHairStyle = 0;
int boyHairColor = 0;
int boyBeard = 0;
int boyMustache = 0;
int boyOutfitStyle = 0;
int boyOutfitColor = 0;
int boyShoes = 0;

const int GIRL_HAIR_COUNT = 6;
const int BOY_HAIR_COUNT = 7;
const int HAIR_COLOR_COUNT = 5;
const int GIRL_OUTFIT_COUNT = 5;
const int BOY_OUTFIT_COUNT = 5;
const int OUTFIT_COLOR_COUNT = 6;
const int SHOE_COLOR_COUNT = 5;
const int BOY_BEARD_COUNT = 4;
const int BOY_MUSTACHE_COUNT = 5;

int dressPlayX = 72;
int dressPlayY = 92;
int oldDressPlayX = 72;
int oldDressPlayY = 92;
float dressPlayVY = 0;
bool dressPlayOnGround = false;
bool dressPlayJumpHeld = false;
bool dressPlayFacingRight = true;

const int dressPlayW = 24;
const int dressPlayH = 48;
const int DRESS_PLAY_GROUND_Y = 118;
const int dressPlaySpeed = 3;
const float dressPlayGravity = 1.0;
const float dressPlayJumpVelocity = -10.5;
const float dressPlayMaxFall = 7.0;


// ======================================================
// MINI GAME STATE: BM TRON + SNAKE
// ======================================================

const int MINI_CELL = 4;
const int MINI_GRID_W = 40;
const int MINI_GRID_H = 28;
const int MINI_TOP = 12;

bool tronTrail[MINI_GRID_W][MINI_GRID_H];
int tronX = 7;
int tronY = 14;
int tronDX = 1;
int tronDY = 0;
int tronScore = 0;
bool tronGameOver = false;
unsigned long tronLastStep = 0;
const unsigned long tronStepDelay = 72;

int snakeX[180];
int snakeY[180];
int snakeLen = 5;
int snakeDX = 1;
int snakeDY = 0;
int snakeFoodX = 20;
int snakeFoodY = 14;
int snakeScore = 0;
bool snakeGameOver = false;
unsigned long snakeLastStep = 0;
const unsigned long snakeStepDelay = 115;


// ======================================================
// TIMING
// ======================================================

unsigned long bootStartTime = 0;
unsigned long lastFrame = 0;
const unsigned long frameDelay = 28;


// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  pinMode(A_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(3);
  // Backlight is controlled directly because this board already has BL on D7.
  digitalWrite(TFT_BL, HIGH);
  buildCommonSprites();

  bootStartTime = millis();
  drawBootScreen();
}


// ======================================================
// LOOP
// ======================================================

void loop() {
  readControls();

  if (appState == BOOT) {
    updateBoot();
  }
  else if (appState == TITLE) {
    updateTitle();
  }
  else if (appState == MAIN_MENU) {
    updateMainMenu();
  }
  else if (appState == SCHOOL_ROOM) {
    updateSchoolRoom();
  }
  else if (appState == GARDEN_INTRO) {
    updateGardenIntro();
  }
  else if (appState == GARDEN_MAZE) {
    updateGardenMaze();
  }
  else if (appState == FRIEND_FOUND) {
    updateFriendFound();
  }
  else if (appState == BRIDGE_INTRO) {
    updateBridgeIntro();
  }
  else if (appState == BRIDGE_CROSSING) {
    updateBridgeCrossing();
  }
  else if (appState == BRIDGE_CLEAR) {
  updateBridgeClear();
}
else if (appState == CITY_INTRO) {
  updateCityIntro();
}
else if (appState == CITY_WALK) {
  updateCityWalk();
}
else if (appState == CITY_CLEAR) {
  updateCityClear();
}
else if (appState == POCKET_PET) {
  updatePocketPet();
}
else if (appState == DRESS_UP) {
  updateDressUp();
}
else if (appState == DRESS_PLAY) {
  updateDressPlay();
}
else if (appState == TRON_GAME) {
  updateTronGame();
}
else if (appState == SNAKE_GAME) {
  updateSnakeGame();
}

  lastAPressed = aPressed;
  lastJoyPressed = joyPressed;
  lastDirection = direction;
}


// ======================================================
// INPUT
// ======================================================

void readControls() {
  joyX = analogRead(JOY_X_PIN);
  joyY = analogRead(JOY_Y_PIN);

  joyPressed = digitalRead(JOY_SW_PIN) == LOW;
  aPressed = digitalRead(A_BUTTON_PIN) == LOW;

  direction = "CENTER";

  // Confirmed POCKET_SPRITE mapping after screen rotation.
  if (joyX < JOY_X_CENTER - JOY_DEADZONE) {
    direction = "DOWN";
  }
  else if (joyX > JOY_X_CENTER + JOY_DEADZONE) {
    direction = "UP";
  }
  else if (joyY > JOY_Y_CENTER + JOY_DEADZONE) {
    direction = "RIGHT";
  }
  else if (joyY < JOY_Y_CENTER - JOY_DEADZONE) {
    direction = "LEFT";
  }
}

bool aJustPressed() {
  return aPressed && !lastAPressed;
}

bool joyJustPressed() {
  return joyPressed && !lastJoyPressed;
}

bool directionJustPressed(String target) {
  return direction == target && lastDirection != target;
}



// ======================================================
// PIXEL SPRITE HELPERS
// ======================================================

uint16_t spriteColor(char c) {
  if (c == 'K') return C_BLACK;
  if (c == 'W') return C_WHITE;
  if (c == 'B') return C_BLUE;
  if (c == 'C') return C_CYAN;
  if (c == 'D') return C_DARK_BLUE;
  if (c == 'R') return C_RED;
  if (c == 'O') return C_ORANGE;
  if (c == 'Y') return C_YELLOW;
  if (c == 'M') return C_MAGENTA;
  if (c == 'P') return C_PINK;
  if (c == 'S') return C_SKIN;
  if (c == 'N') return C_BROWN;
  if (c == 'G') return C_GREEN;
  if (c == 'g') return C_DARK_GREEN;
  return C_BLACK;
}

int spriteRowsWidth(const char* const rows[], int h) {
  int w = 0;
  for (int row = 0; row < h; row++) {
    int len = strlen(rows[row]);
    if (len > w) w = len;
  }
  return w;
}

void drawRowsToSprite(LGFX_Sprite& spr, const char* const rows[], int h) {
  int w = spriteRowsWidth(rows, h);
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(C_TRANSPARENT_KEY);

  for (int row = 0; row < h; row++) {
    const char* line = rows[row];
    for (int col = 0; line[col] != '\0'; col++) {
      char c = line[col];
      if (c != ' ') {
        spr.drawPixel(col, row, spriteColor(c));
      }
    }
  }
}

void drawSpriteRows(int x, int y, const char* const rows[], int h) {
  // Fallback/general-purpose pixel-map draw. We wrap in one SPI transaction
  // so occasional non-cached sprites still draw faster and cleaner.
  tft.startWrite();
  for (int row = 0; row < h; row++) {
    const char* line = rows[row];
    for (int col = 0; line[col] != '\0'; col++) {
      char c = line[col];
      if (c != ' ') {
        tft.drawPixel(x + col, y + row, spriteColor(c));
      }
    }
  }
  tft.endWrite();
}

void buildCommonSprites() {
  if (commonSpritesReady) return;

  static const char* const madelynA[] = {
    "  YYYY  ", " YYYYYY ", "YYYYYYYY", "RRSSSSRR",
    "RSKSSKSR", " RSSSSR ", "  RSSR  ", "  YYYY  ",
    " YYYYYY ", "YYYYYYYY", "  K  K  ", " KK  KK "
  };
  static const char* const madelynB[] = {
    "  YYYY  ", " YYYYYY ", "YYYYYYYY", "RRSSSSRR",
    "RSKSSKSR", " RSSSSR ", "  RSSR  ", "  YYYY  ",
    " YYYYYY ", "YYYYYYYY", " K    K ", "  KK KK "
  };
  static const char* const friendA[] = {
    " P  PP P ", "PPPPPPPP ", " NNSSNN  ", "NNSSSSNN ",
    "NSKSSKSN ", " NSSSSN  ", "  NSSN   ", "  BBBB   ",
    " BBBBBB  ", "BBBBBBBB ", "  K  K   ", " KK  KK  "
  };
  static const char* const friendB[] = {
    " P  PP P ", "PPPPPPPP ", " NNSSNN  ", "NNSSSSNN ",
    "NSKSSKSN ", " NSSSSN  ", "  NSSN   ", "  BBBB   ",
    " BBBBBB  ", "BBBBBBBB ", " K    K  ", "  KK KK  "
  };

  drawRowsToSprite(sprMadelynA, madelynA, 12);
  drawRowsToSprite(sprMadelynB, madelynB, 12);
  drawRowsToSprite(sprFriendA, friendA, 12);
  drawRowsToSprite(sprFriendB, friendB, 12);

  commonSpritesReady = true;
}

void drawTopDownMadelynSprite(int x, int y) {
  buildCommonSprites();
  bool step = ((millis() / 180) % 2) == 0;
  (step ? sprMadelynB : sprMadelynA).pushSprite(x, y, C_TRANSPARENT_KEY);
}

void drawTopDownFriendSprite(int x, int y) {
  buildCommonSprites();
  bool step = ((millis() / 180) % 2) == 0;
  (step ? sprFriendB : sprFriendA).pushSprite(x, y, C_TRANSPARENT_KEY);
}



// ======================================================
// BOOT / TITLE / MAIN MENU
// ======================================================

void drawBootScreen() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(14, 28);
  tft.println("HI,");

  tft.setTextColor(C_CYAN);
  tft.setCursor(14, 54);
  tft.println("EMERSON!");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void updateBoot() {
  if (millis() - bootStartTime > 1800 || aJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
  }
}

void drawTitleScreen() {
  tft.fillScreen(C_SKY);
  tft.setTextWrap(false);

  // Sky
  tft.fillCircle(132, 18, 10, C_YELLOW);
  tft.drawCircle(132, 18, 11, C_WHITE);

  tft.fillCircle(22, 22, 3, C_WHITE);
  tft.fillCircle(27, 21, 4, C_WHITE);
  tft.fillCircle(33, 22, 3, C_WHITE);

  // Old boarding school
  tft.fillRect(8, 56, 112, 72, C_TAN);
  tft.drawRect(8, 56, 112, 72, C_DARK_BLUE);

  tft.fillTriangle(4, 56, 64, 32, 124, 56, C_DARK_BLUE);
  tft.drawLine(4, 56, 64, 32, C_WHITE);
  tft.drawLine(64, 32, 124, 56, C_WHITE);

  tft.fillRect(28, 42, 8, 14, C_BROWN);
  tft.drawRect(28, 42, 8, 14, C_DARK_BLUE);

  tft.fillRect(94, 44, 7, 12, C_BROWN);
  tft.drawRect(94, 44, 7, 12, C_DARK_BLUE);

  tft.fillRect(53, 94, 22, 34, C_DOOR);
  tft.drawRect(53, 94, 22, 34, C_WHITE);
  tft.drawCircle(64, 96, 11, C_WHITE);
  tft.drawPixel(70, 111, C_GOLD);

  drawTitleWindow(18, 66);
  drawTitleWindow(44, 66);
  drawTitleWindow(84, 66);
  drawTitleWindow(18, 96);
  drawTitleWindow(84, 96);

  // Vines
  tft.drawLine(10, 58, 20, 78, C_DARK_GREEN);
  tft.drawLine(20, 78, 16, 108, C_DARK_GREEN);
  tft.drawLine(118, 58, 106, 80, C_DARK_GREEN);
  tft.drawLine(106, 80, 112, 112, C_DARK_GREEN);

  tft.fillCircle(16, 70, 2, C_GREEN);
  tft.fillCircle(20, 82, 2, C_GREEN);
  tft.fillCircle(17, 96, 2, C_GREEN);
  tft.fillCircle(109, 70, 2, C_GREEN);
  tft.fillCircle(105, 84, 2, C_GREEN);
  tft.fillCircle(111, 100, 2, C_GREEN);

  // Tall streetlight
  tft.drawLine(140, 58, 140, 124, C_DARK_BLUE);
  tft.drawLine(130, 124, 150, 124, C_DARK_BLUE);
  tft.drawLine(140, 62, 130, 72, C_DARK_BLUE);
  tft.drawLine(140, 62, 150, 72, C_DARK_BLUE);
  tft.fillCircle(130, 74, 4, C_YELLOW);
  tft.drawCircle(130, 74, 5, C_DARK_BLUE);
  tft.fillCircle(150, 74, 4, C_YELLOW);
  tft.drawCircle(150, 74, 5, C_DARK_BLUE);

  // Title panel
  tft.fillRect(0, 0, 104, 51, C_BLACK);
  tft.drawRect(0, 0, 104, 51, C_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(6, 7);
  tft.println("MADELYN");

  tft.setTextColor(C_WHITE);
  tft.setCursor(6, 29);
  tft.println("IN PARIS");

  drawMadelynTiny(123, 96);

  tft.fillRect(6, 112, 86, 14, C_TAN);
  tft.drawRect(6, 112, 86, 14, C_DARK_BLUE);

  tft.setTextSize(1);
  tft.setTextColor(C_RED);
  tft.setCursor(12, 116);
  tft.print("A = Start");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void drawTitleWindow(int x, int y) {
  tft.fillRect(x, y + 5, 14, 16, C_SKY);
  tft.fillCircle(x + 7, y + 6, 7, C_SKY);

  tft.drawRect(x, y + 5, 14, 16, C_WHITE);
  tft.drawCircle(x + 7, y + 6, 7, C_WHITE);

  tft.drawLine(x + 7, y + 1, x + 7, y + 21, C_WHITE);
  tft.drawLine(x + 2, y + 13, x + 12, y + 13, C_WHITE);
}

void drawMadelynTiny(int x, int y) {
  static const char* const rows[] = {
    "    YYYYY    ", "  YYYYYYYYY  ", " YYYYYYYYYYY ", "RRRSSKSSRRR",
    "RRRSSSSSRRR", " RRRSSSRRR ", "   SSSSS   ", "  SYYYYYS  ",
    " SYYYYYYYS ", "SYYYYYYYYYS", " OOYYYYYOO ", "   YYYYY   ",
    "  YYYYYYY  ", " YYYYYYYYY ", "YYYYYYYYYYY", "   K   K   ",
    "  KK   KK  ", "           ", "           ", "           ",
    "           ", "           ", "           ", "           "
  };
  drawSpriteRows(x, y, rows, 24);
  // yellow hat brim/shape accent
  tft.drawPixel(x + 5, y + 1, C_GOLD);
  tft.drawPixel(x + 9, y + 1, C_GOLD);
}


void updateTitle() {
  if (aJustPressed()) {
    startSchoolRoom();
  }
}

void drawMainMenu() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  // Clean title card.
  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
  tft.drawRect(2, 2, tft.width() - 4, tft.height() - 4, C_DARK_BLUE);

  // Cooler chunky title: layered shadow + color face.
  // Centered manually for the 160x128 ST7735 landscape screen.
  tft.setTextSize(2);

  tft.setTextColor(C_DARK_BLUE);
  tft.setCursor(40, 12);
  tft.print("POCKET");
  tft.setCursor(40, 34);
  tft.print("SPRITE");

  tft.setTextColor(C_PINK);
  tft.setCursor(38, 10);
  tft.print("POCKET");
  tft.setCursor(38, 32);
  tft.print("SPRITE");

  tft.setTextColor(C_CYAN);
  tft.setCursor(37, 9);
  tft.print("POCKET");

  tft.setTextColor(C_YELLOW);
  tft.setCursor(37, 31);
  tft.print("SPRITE");

  // Pixel underline / sparkle trim.
  tft.drawLine(35, 52, 124, 52, C_CYAN);
  tft.drawLine(45, 55, 114, 55, C_PINK);
  tft.drawPixel(30, 14, C_YELLOW);
  tft.drawPixel(132, 15, C_CYAN);
  tft.drawPixel(24, 38, C_PINK);
  tft.drawPixel(139, 42, C_YELLOW);

  // Small label, bottom-right.
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(104, 116);
  tft.print("MAIN MENU");

  // Evenly distributed menu cards.
  const int cardX = 18;
  const int cardW = 124;
  const int cardH = 11;
  const int startY = 62;
  const int gapY = 10;

  for (int i = 0; i < MENU_COUNT; i++) {
    int y = startY + i * gapY;

    uint16_t borderColor = (menuIndex == i) ? C_YELLOW : C_DARK_BLUE;
    uint16_t fillColor = (menuIndex == i) ? C_DARK_BLUE : C_BLACK;
    uint16_t textColor = (menuIndex == i) ? C_YELLOW : C_WHITE;

    tft.fillRoundRect(cardX, y, cardW, cardH, 3, fillColor);
    tft.drawRoundRect(cardX, y, cardW, cardH, 3, borderColor);

    if (menuIndex == i) {
      tft.fillTriangle(cardX + 5, y + 3, cardX + 5, y + 8, cardX + 10, y + 5, C_PINK);
      tft.drawPixel(cardX + cardW - 8, y + 5, C_CYAN);
    }

    tft.setTextSize(1);
    tft.setTextColor(textColor);
    tft.setCursor(cardX + 18, y + 2);

    if (i == 0) tft.print("Adventure");
    else if (i == 1) tft.print("Pocket Pet");
    else if (i == 2) tft.print("Dress Up");
    else if (i == 3) tft.print("BM Tron");
    else if (i == 4) tft.print("Snake");
  }
}

void drawMenuOption(int index, const char* label, int y) {
  tft.setTextSize(1);

  if (menuIndex == index) {
    tft.setTextColor(C_YELLOW);
    tft.setCursor(10, y);
    tft.print(">");
  }
  else {
    tft.setTextColor(C_DARK_GRAY);
    tft.setCursor(10, y);
    tft.print(" ");
  }

  tft.setTextColor(C_WHITE);
  tft.setCursor(22, y);
  tft.print(label);
}

void updateMainMenu() {
  if (directionJustPressed("UP")) {
    menuIndex--;
    if (menuIndex < 0) {
      menuIndex = MENU_COUNT - 1;
    }
    drawMainMenu();
  }
  else if (directionJustPressed("DOWN")) {
    menuIndex++;
    if (menuIndex >= MENU_COUNT) {
      menuIndex = 0;
    }
    drawMainMenu();
  }

  if (aJustPressed()) {
    if (menuIndex == 0) {
      appState = TITLE;
      drawTitleScreen();
    }
    else if (menuIndex == 1) {
      startPocketPet();
    }
    else if (menuIndex == 2) {
      startDressUp();
    }
    else if (menuIndex == 3) {
      startTronGame();
    }
    else if (menuIndex == 4) {
      startSnakeGame();
    }
  }
}
void startPocketPet() {
  appState = POCKET_PET;

  petX = 64;
  petY = 88;
  oldPetX = petX;
  oldPetY = petY;

  petBallX = 118;
  petBallY = 104;
  oldPetBallX = petBallX;
  oldPetBallY = petBallY;
  petBallVX = 0;
  petBallVY = 0;

  petVY = 0;
  petOnGround = false;
  petJumpHeld = false;
  petFacingRight = true;
  petAnim = 0;

  tft.fillScreen(C_BLACK);
  drawPocketPetRoom();
}


void drawPocketPetRoom() {
  tft.fillRect(0, 0, tft.width(), tft.height(), C_BLACK);

  // Full-screen room wall. No top banner in Pocket Pet.
  tft.fillRect(0, 0, tft.width(), PET_GROUND_Y, C_TAN);
  for (int x = 8; x < tft.width(); x += 18) {
    tft.drawLine(x, 4, x, PET_GROUND_Y, C_BROWN);
    tft.drawPixel(x + 5, 22, C_WHITE);
    tft.drawPixel(x + 10, 46, C_WHITE);
  }

  // Window
  tft.fillRect(112, 22, 30, 22, C_SKY);
  tft.drawRect(112, 22, 30, 22, C_WHITE);
  tft.drawLine(127, 22, 127, 44, C_WHITE);
  tft.drawLine(112, 33, 142, 33, C_WHITE);
  tft.fillCircle(118, 28, 3, C_YELLOW);
  tft.drawPixel(136, 28, C_WHITE);
  tft.drawPixel(139, 30, C_WHITE);

  // Tiny wall picture
  tft.drawRect(58, 24, 24, 16, C_WHITE);
  tft.drawPixel(65, 31, C_PINK);
  tft.drawPixel(70, 30, C_YELLOW);
  tft.drawPixel(74, 32, C_CYAN);
  tft.drawLine(62, 36, 78, 36, C_DARK_GREEN);

  // Floor
  tft.fillRect(0, PET_GROUND_Y, tft.width(), tft.height() - PET_GROUND_Y, C_BROWN);
  tft.drawLine(0, PET_GROUND_Y, tft.width(), PET_GROUND_Y, C_WHITE);
  for (int x = 0; x < tft.width(); x += 20) {
    tft.drawLine(x, PET_GROUND_Y + 8, x + 10, PET_GROUND_Y + 8, C_TAN);
  }

  // Bigger bed platform the pet can jump onto
  tft.fillRect(8, 92, 42, 15, C_BLUE);
  tft.drawRect(8, 92, 42, 15, C_WHITE);
  tft.fillRect(11, 95, 13, 7, C_CYAN);
  tft.fillRect(27, 97, 19, 8, C_DARK_BLUE);
  tft.drawPixel(14, 98, C_WHITE);

  // Rug
  tft.fillRoundRect(58, 113, 42, 11, 4, C_PINK);
  tft.drawRoundRect(58, 113, 42, 11, 4, C_WHITE);

  drawPocketPetToy();
  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);

  drawPetShadow(petX, petY);
  drawPocketPetBall();
  drawPocketPetCreature(petX, petY);
}


void updatePocketPet() {
  if (joyJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
    return;
  }

  if (millis() - lastFrame < frameDelay) {
    return;
  }

  lastFrame = millis();

  oldPetX = petX;
  oldPetY = petY;
  oldPetBallX = petBallX;
  oldPetBallY = petBallY;

  updatePocketPetAButton();
  updatePocketPetPhysics();
  updatePocketPetBall();
  checkPocketPetBallInteractions();

  drawPocketPetFrame();
  petAnim++;
}


void updatePocketPetAButton() {
  // Pocket Pet controls:
  // joystick UP = jump
  // A button = kick ball
  //
  // Use held-state gates instead of aJustPressed()/directionJustPressed()
  // so this mode stays reliable even if the global last-button state
  // changes elsewhere in the game loop.

  bool jumpCommand = (direction == "UP");

  // Safety for joystick calibration/orientation weirdness:
  // If your physical UP reads as DOWN, this still lets the pet jump.
  // Remove the next line later if you want strict UP only.
  jumpCommand = jumpCommand || (direction == "DOWN");

  if (jumpCommand && !petJumpHeld && petOnGround) {
    petVY = petJumpVelocity;
    petOnGround = false;
    petJumpHeld = true;
  }

  if (!jumpCommand) {
    petJumpHeld = false;
  }

  if (aPressed && !petKickHeld) {
    kickPocketPetBall();
    petKickHeld = true;
  }

  if (!aPressed) {
    petKickHeld = false;
  }
}

void updatePocketPetPhysics() {
  int nextX = petX;

  if (direction == "LEFT") {
    nextX -= petMoveSpeed;
    petFacingRight = false;
  }
  else if (direction == "RIGHT") {
    nextX += petMoveSpeed;
    petFacingRight = true;
  }

  if (nextX < 4) nextX = 4;
  if (nextX > tft.width() - petW - 4) nextX = tft.width() - petW - 4;
  petX = nextX;

  petVY += petGravity;
  if (petVY > petMaxFall) petVY = petMaxFall;

  int oldY = petY;
  petY += (int)petVY;
  petOnGround = false;

  // Land on the bed from above.
  bool overBed = petX + petW > 8 && petX < 8 + 42;
  bool wasAboveBed = oldY + petH <= 92;
  bool nowHitsBed = petY + petH >= 92 && petY + petH <= 106;

  if (petVY >= 0 && overBed && wasAboveBed && nowHitsBed) {
    petY = 92 - petH;
    petVY = 0;
    petOnGround = true;
  }

  // Land on the ball from above.
  int ballTop = (int)petBallY - petBallR;
  bool overBall = petX + petW > (int)petBallX - petBallR && petX < (int)petBallX + petBallR;
  bool wasAboveBall = oldY + petH <= ballTop;
  bool nowHitsBall = petY + petH >= ballTop && petY + petH <= ballTop + 9;

  if (petVY >= 0 && overBall && wasAboveBall && nowHitsBall) {
    petY = ballTop - petH;
    petVY = -2.0;
    petOnGround = true;
    petBallVY += 1.5;
    petBallVX += petFacingRight ? 1.0 : -1.0;
  }

  // Land on floor.
  if (petY > PET_GROUND_Y - petH) {
    petY = PET_GROUND_Y - petH;
    petVY = 0;
    petOnGround = true;
  }
}

void drawPocketPetFrame() {
  bool moved = oldPetX != petX || oldPetY != petY ||
               (int)oldPetBallX != (int)petBallX || (int)oldPetBallY != (int)petBallY;

  if (!moved && petAnim % 12 != 0) {
    return;
  }

  int ux = min(min(oldPetX, petX), min((int)oldPetBallX - petBallR, (int)petBallX - petBallR)) - 10;
  int uy = min(min(oldPetY, petY), min((int)oldPetBallY - petBallR, (int)petBallY - petBallR)) - 8;
  int ux2 = max(max(oldPetX + petW, petX + petW), max((int)oldPetBallX + petBallR, (int)petBallX + petBallR)) + 12;
  int uy2 = max(max(oldPetY + petH + 3, petY + petH + 3), max((int)oldPetBallY + petBallR, (int)petBallY + petBallR)) + 8;

  redrawPocketPetPatch(ux, uy, ux2 - ux, uy2 - uy);

  drawPetShadow(petX, petY);
  drawPocketPetBall();
  drawPocketPetCreature(petX, petY);
}

void redrawPocketPetPatch(int x, int y, int w, int h) {

  if (x < 0) {

    w += x;

    x = 0;

  }

  if (y < 0) {

    h += y;

    y = 0;

  }

  if (x + w > tft.width()) {

    w = tft.width() - x;

  }

  if (y + h > tft.height()) {

    h = tft.height() - y;

  }

  if (w <= 0 || h <= 0) {

    return;

  }

  // wall/floor restore

  if (y >= PET_GROUND_Y) {

    tft.fillRect(x, y, w, h, C_BROWN);

  }

  else {

    tft.fillRect(x, y, w, h, C_TAN);

    // wallpaper stripes

    for (int sx = 8; sx < tft.width(); sx += 18) {

      if (sx >= x && sx <= x + w) {

        tft.drawLine(sx, max(y, 4), sx, min(y + h, PET_GROUND_Y), C_BROWN);

      }

    }

    if (y + h > PET_GROUND_Y) {

      tft.fillRect(x, PET_GROUND_Y, w, y + h - PET_GROUND_Y, C_BROWN);

      tft.drawLine(x, PET_GROUND_Y, x + w, PET_GROUND_Y, C_WHITE);

    }

  }



  // redraw window if touched
  if (rectsOverlap(x, y, w, h, 112, 22, 30, 22)) {
    tft.fillRect(112, 22, 30, 22, C_SKY);
    tft.drawRect(112, 22, 30, 22, C_WHITE);
    tft.drawLine(127, 22, 127, 44, C_WHITE);
    tft.drawLine(112, 33, 142, 33, C_WHITE);
    tft.fillCircle(118, 28, 3, C_YELLOW);
    tft.drawPixel(136, 28, C_WHITE);
    tft.drawPixel(139, 30, C_WHITE);
  }

  // redraw wall picture if touched
  if (rectsOverlap(x, y, w, h, 58, 24, 24, 16)) {
    tft.drawRect(58, 24, 24, 16, C_WHITE);
    tft.drawPixel(65, 31, C_PINK);
    tft.drawPixel(70, 30, C_YELLOW);
    tft.drawPixel(74, 32, C_CYAN);
    tft.drawLine(62, 36, 78, 36, C_DARK_GREEN);
  }

  // redraw bed if touched

  if (rectsOverlap(x, y, w, h, 8, 92, 42, 15)) {

    tft.fillRect(8, 92, 42, 15, C_BLUE);

    tft.drawRect(8, 92, 42, 15, C_WHITE);

    tft.fillRect(11, 95, 13, 7, C_CYAN);
    tft.fillRect(27, 97, 19, 8, C_DARK_BLUE);
    tft.drawPixel(14, 98, C_WHITE);

  }

  // redraw toy if touched

  if (rectsOverlap(x, y, w, h, 118, 100, 32, 18)) {

    drawPocketPetToy();

  }

  if (rectsOverlap(x, y, w, h, 58, 113, 42, 11)) {
    tft.fillRoundRect(58, 113, 42, 11, 4, C_PINK);
    tft.drawRoundRect(58, 113, 42, 11, 4, C_WHITE);
  }

  if (x <= 2 || y <= 2 || x + w >= tft.width() - 2 || y + h >= tft.height() - 2) {

    tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);

  }

}

void updatePocketPetBall() {
  // Gravity + friction.
  petBallVY += 0.32;
  if (petBallVY > 5.5) petBallVY = 5.5;

  petBallX += petBallVX;
  petBallY += petBallVY;

  // Wall ricochet.
  if (petBallX < petBallR + 3) {
    petBallX = petBallR + 3;
    petBallVX = -petBallVX * 0.82;
  }
  if (petBallX > tft.width() - petBallR - 3) {
    petBallX = tft.width() - petBallR - 3;
    petBallVX = -petBallVX * 0.82;
  }

  // Floor bounce.
  if (petBallY > PET_GROUND_Y - petBallR) {
    petBallY = PET_GROUND_Y - petBallR;
    if (abs(petBallVY) > 1.4) petBallVY = -petBallVY * 0.62;
    else petBallVY = 0;
    petBallVX *= 0.90;
  }

  // Bed bounce/top collision.
  bool overBed = petBallX + petBallR > 8 && petBallX - petBallR < 50;
  if (overBed && petBallY + petBallR > 92 && petBallY + petBallR < 105 && petBallVY > 0) {
    petBallY = 92 - petBallR;
    petBallVY = -petBallVY * 0.55;
    petBallVX *= 0.92;
  }

  // Let tiny movement settle.
  if (abs(petBallVX) < 0.06) petBallVX = 0;
  if (abs(petBallVY) < 0.06 && petBallY >= PET_GROUND_Y - petBallR - 1) petBallVY = 0;
}

void checkPocketPetBallInteractions() {
  int petLeft = petX;
  int petRight = petX + petW;
  int petTop = petY;
  int petBottom = petY + petH;
  int ballLeft = (int)petBallX - petBallR;
  int ballRight = (int)petBallX + petBallR;
  int ballTop = (int)petBallY - petBallR;
  int ballBottom = (int)petBallY + petBallR;

  if (!rectsOverlap(petLeft, petTop, petW, petH, ballLeft, ballTop, petBallR * 2, petBallR * 2)) {
    return;
  }

  // Side push from walking into the ball.
  if (petRight > ballLeft && petX < petBallX && direction == "RIGHT") {
    petBallX = petRight + petBallR;
    petBallVX += 1.2;
    petFacingRight = true;
  }
  else if (petLeft < ballRight && petX > petBallX && direction == "LEFT") {
    petBallX = petLeft - petBallR;
    petBallVX -= 1.2;
    petFacingRight = false;
  }

  // Gentle shove if overlapping from above/below.
  if (petBottom > ballTop && petTop < ballTop) {
    petBallVY += 0.8;
  }
}

void kickPocketPetBall() {
  int dx = (int)petBallX - (petX + petW / 2);
  int dy = (int)petBallY - (petY + petH / 2);
  int reachX = petW / 2 + petBallR + 10;
  int reachY = petH / 2 + petBallR + 10;

  if (abs(dx) <= reachX && abs(dy) <= reachY) {
    float dir = petFacingRight ? 1.0 : -1.0;
    if (dx < -2) dir = -1.0;
    if (dx > 2) dir = 1.0;

    petBallVX = 5.8 * dir;
    petBallVY = -4.2;
  }
}

void drawPocketPetBall() {
  int x = (int)petBallX;
  int y = (int)petBallY;
  tft.fillCircle(x, y, petBallR, C_YELLOW);
  tft.drawCircle(x, y, petBallR, C_WHITE);
  tft.drawPixel(x - 3, y - 3, C_RED);
  tft.drawPixel(x + 3, y + 3, C_CYAN);
  tft.drawPixel(x + 1, y - 4, C_WHITE);
}

void buildPetSprites() {
  if (petSpritesReady) return;

  static const char* const idleRows[] = {
    "      M      M       ",
    "     MMM    MMM      ",
    "    MMPM  MPMM      ",
    "   MMMMMMMMMMMM     ",
    "  MMMMMMMMMMMMMM    ",
    " MMMMMMMMMMMMMMMM   ",
    " MMMWWMMMMMMWWMMM   ",
    " MMMKKMMMMMMKKMMM   ",
    " MMMMMMMKKMMMMMMM   ",
    " MMMMMMMMMMMMMMMM   ",
    " MMMMMPPPPPMMMMM    ",
    "  MMMMPPPPPMMMM     ",
    "   MMMMMMMMMMMM     ",
    "    MMMWWWWMMM      ",
    "     MMWWWWMM       ",
    "      MMMMMM        ",
    "      KK  KK        ",
    "     KKK  KKK       ",
    "                    ",
    "                    ",
    "                    ",
    "                    "
  };

  static const char* const walkRowsA[] = {
    "      M      M       ",
    "     MMM    MMM      ",
    "    MMPM  MPMM      ",
    "   MMMMMMMMMMMM     ",
    "  MMMMMMMMMMMMMM    ",
    " MMMMMMMMMMMMMMMM   ",
    " MMMWWMMMMMMWWMMM   ",
    " MMMKKMMMMMMKKMMM   ",
    " MMMMMMMKKMMMMMMM   ",
    " MMMMMMMMMMMMMMMM   ",
    " MMMMMPPPPPMMMMM    ",
    "  MMMMPPPPPMMMM     ",
    "   MMMMMMMMMMMM     ",
    "    MMMWWWWMMM      ",
    "     MMWWWWMM       ",
    "      MMMMMM        ",
    "     KKK   KK       ",
    "    KKK     KK      ",
    "                    ",
    "                    ",
    "                    ",
    "                    "
  };

  static const char* const walkRowsB[] = {
    "      M      M       ",
    "     MMM    MMM      ",
    "    MMPM  MPMM      ",
    "   MMMMMMMMMMMM     ",
    "  MMMMMMMMMMMMMM    ",
    " MMMMMMMMMMMMMMMM   ",
    " MMMWWMMMMMMWWMMM   ",
    " MMMKKMMMMMMKKMMM   ",
    " MMMMMMMKKMMMMMMM   ",
    " MMMMMMMMMMMMMMMM   ",
    " MMMMMPPPPPMMMMM    ",
    "  MMMMPPPPPMMMM     ",
    "   MMMMMMMMMMMM     ",
    "    MMMWWWWMMM      ",
    "     MMWWWWMM       ",
    "      MMMMMM        ",
    "      KK   KKK      ",
    "     KK     KKK     ",
    "                    ",
    "                    ",
    "                    ",
    "                    "
  };

  drawRowsToSprite(sprPetIdle, idleRows, 22);
  drawRowsToSprite(sprPetWalkA, walkRowsA, 22);
  drawRowsToSprite(sprPetWalkB, walkRowsB, 22);
  petSpritesReady = true;
}

void drawPetShadow(int x, int y) {
  int shadowY = PET_GROUND_Y - 2;
  if (y + petH <= 92 && petX + petW > 8 && petX < 50) {
    shadowY = 92 - 1;
  }
  tft.drawFastHLine(x + 4, shadowY, 20, C_DARK_GRAY);
  tft.drawFastHLine(x + 8, shadowY + 1, 12, C_DARK_GRAY);
}

void drawPocketPetCreature(int x, int y) {
  int bounce = 0;
  if (petOnGround && direction != "CENTER") {
    bounce = (petAnim / 4) % 2;
  }
  y -= bounce;

  // Fat shadow.
  tft.drawFastHLine(x + 4, y + petH + 2, 20, C_DARK_GRAY);
  tft.drawFastHLine(x + 8, y + petH + 3, 12, C_DARK_GRAY);

  // Tail behind body.
  int tailY = y + 13 + ((petAnim / 6) % 2);
  if (petFacingRight) {
    tft.drawLine(x + 25, tailY, x + 32, tailY - 4, C_MAGENTA);
    tft.drawLine(x + 32, tailY - 4, x + 34, tailY - 1, C_PINK);
  }
  else {
    tft.drawLine(x + 2, tailY, x - 5, tailY - 4, C_MAGENTA);
    tft.drawLine(x - 5, tailY - 4, x - 7, tailY - 1, C_PINK);
  }

  // Big ears.
  tft.fillTriangle(x + 3, y + 10, x + 9, y, x + 15, y + 10, C_MAGENTA);
  tft.fillTriangle(x + 15, y + 10, x + 21, y, x + 27, y + 10, C_MAGENTA);
  tft.fillTriangle(x + 7, y + 8, x + 9, y + 3, x + 12, y + 9, C_PINK);
  tft.fillTriangle(x + 19, y + 9, x + 21, y + 3, x + 24, y + 8, C_PINK);

  // Big fat body/head blob.
  tft.fillRoundRect(x + 2, y + 8, 28, 22, 10, C_MAGENTA);
  tft.drawRoundRect(x + 2, y + 8, 28, 22, 10, C_WHITE);
  tft.fillRoundRect(x + 10, y + 20, 12, 8, 4, C_PINK);

  // Huge eyes.
  tft.fillCircle(x + 11, y + 16, 5, C_WHITE);
  tft.fillCircle(x + 21, y + 16, 5, C_WHITE);
  tft.fillCircle(x + 12, y + 17, 2, C_BLACK);
  tft.fillCircle(x + 20, y + 17, 2, C_BLACK);
  tft.drawPixel(x + 10, y + 14, C_CYAN);
  tft.drawPixel(x + 22, y + 14, C_CYAN);

  // Nose and smile.
  tft.fillCircle(x + 16, y + 21, 2, C_BLACK);
  tft.drawPixel(x + 13, y + 25, C_WHITE);
  tft.drawPixel(x + 14, y + 26, C_WHITE);
  tft.drawPixel(x + 15, y + 26, C_WHITE);
  tft.drawPixel(x + 16, y + 26, C_WHITE);
  tft.drawPixel(x + 17, y + 26, C_WHITE);
  tft.drawPixel(x + 18, y + 26, C_WHITE);
  tft.drawPixel(x + 19, y + 25, C_WHITE);

  // Big feet.
  tft.fillRect(x + 7, y + 30, 7, 3, C_BLACK);
  tft.fillRect(x + 20, y + 30, 7, 3, C_BLACK);
}

void drawPocketPetToy() {
  // Toy cube.
  tft.fillRect(137, 101, 10, 10, C_GREEN);
  tft.drawRect(137, 101, 10, 10, C_WHITE);
  tft.drawPixel(140, 104, C_YELLOW);
  tft.drawPixel(144, 108, C_PINK);

  // Little snack dish.
  tft.fillRoundRect(104, 105, 10, 5, 2, C_GRAY);
  tft.drawPixel(107, 104, C_ORANGE);
  tft.drawPixel(110, 104, C_YELLOW);
}

void startDressUp() {
  appState = DRESS_UP;
  dressStep = STEP_GENDER;
  dressGender = DRESS_GIRL;
  tft.fillScreen(C_BLACK);
  drawDressUpScreen();
}

void drawDressUpScreen() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  // No banner. Dress Up gets the whole screen.
  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);

  bool faceMode = (dressStep == STEP_HAIR || dressStep == STEP_HAIR_COLOR ||
                   dressStep == STEP_BEARD || dressStep == STEP_MUSTACHE);

  if (dressStep == STEP_GENDER) {
    tft.setTextSize(2);
    tft.setTextColor(C_PINK);
    tft.setCursor(10, 8);
    tft.print("PICK");

    tft.setTextColor(C_YELLOW);
    tft.setCursor(10, 30);
    tft.print(dressGender == DRESS_GIRL ? "GIRL" : "BOY");

    tft.setTextSize(1);
    tft.setTextColor(C_CYAN);
    tft.setCursor(12, 112);
    tft.print("< >   A");

    drawDressFullPreview(100, 36, 1);
    return;
  }

  if (dressStep == STEP_CONFIRM) {
    drawDressFacePreview(82, 12);

    tft.setTextSize(1);
    tft.setTextColor(C_WHITE);
    tft.setCursor(20, 106);
    tft.print("Push A to play");

    tft.fillCircle(12, 110, 2, C_PINK);
    tft.fillCircle(148, 110, 2, C_YELLOW);
    tft.drawPixel(80, 118, C_CYAN);
    return;
  }

  tft.setTextSize(1);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 6);
  tft.print(getDressStepName());

  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 18);
  tft.print("< ");
  tft.print(getDressValueName());
  tft.print(" >");

  if (faceMode) {
    drawDressFacePreview(78, 28);
  }
  else {
    drawDressFullPreview(98, 34, 1);
  }

  // tiny step dots, not instruction text
  int totalSteps = (dressGender == DRESS_BOY) ? 7 : 5;
  int current = 0;
  if (dressStep == STEP_HAIR) current = 0;
  else if (dressStep == STEP_BEARD) current = 1;
  else if (dressStep == STEP_MUSTACHE) current = 2;
  else if (dressStep == STEP_HAIR_COLOR) current = (dressGender == DRESS_BOY) ? 3 : 1;
  else if (dressStep == STEP_OUTFIT) current = (dressGender == DRESS_BOY) ? 4 : 2;
  else if (dressStep == STEP_OUTFIT_COLOR) current = (dressGender == DRESS_BOY) ? 5 : 3;
  else if (dressStep == STEP_SHOES) current = (dressGender == DRESS_BOY) ? 6 : 4;

  int dotStart = 8;
  for (int i = 0; i < totalSteps; i++) {
    tft.fillCircle(dotStart + i * 8, 118, 2, i == current ? C_YELLOW : C_DARK_GRAY);
  }
}

void updateDressUp() {
  if (joyJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
    return;
  }

  if (directionJustPressed("LEFT")) {
    if (dressStep == STEP_GENDER) {
      dressGender = DRESS_GIRL;
    }
    else {
      cycleDressValue(-1);
    }
    drawDressUpScreen();
  }
  else if (directionJustPressed("RIGHT")) {
    if (dressStep == STEP_GENDER) {
      dressGender = DRESS_BOY;
    }
    else {
      cycleDressValue(1);
    }
    drawDressUpScreen();
  }
  else if ((directionJustPressed("UP") || directionJustPressed("DOWN")) && dressStep == STEP_GENDER) {
    dressGender = (dressGender == DRESS_GIRL) ? DRESS_BOY : DRESS_GIRL;
    drawDressUpScreen();
  }

  if (aJustPressed()) {
    if (dressStep == STEP_CONFIRM) {
      startDressPlay();
    }
    else {
      nextDressStep();
      drawDressUpScreen();
    }
  }
}

void nextDressStep() {
  if (dressStep == STEP_GENDER) {
    dressStep = STEP_HAIR;
  }
  else if (dressStep == STEP_HAIR) {
    dressStep = (dressGender == DRESS_BOY) ? STEP_BEARD : STEP_HAIR_COLOR;
  }
  else if (dressStep == STEP_BEARD) {
    dressStep = STEP_MUSTACHE;
  }
  else if (dressStep == STEP_MUSTACHE) {
    dressStep = STEP_HAIR_COLOR;
  }
  else if (dressStep == STEP_HAIR_COLOR) {
    dressStep = STEP_OUTFIT;
  }
  else if (dressStep == STEP_OUTFIT) {
    dressStep = STEP_OUTFIT_COLOR;
  }
  else if (dressStep == STEP_OUTFIT_COLOR) {
    dressStep = STEP_SHOES;
  }
  else if (dressStep == STEP_SHOES) {
    dressStep = STEP_CONFIRM;
  }
}

void cycleDressValue(int delta) {
  if (dressGender == DRESS_GIRL) {
    if (dressStep == STEP_HAIR) {
      girlHairStyle = (girlHairStyle + delta + GIRL_HAIR_COUNT) % GIRL_HAIR_COUNT;
    }
    else if (dressStep == STEP_HAIR_COLOR) {
      girlHairColor = (girlHairColor + delta + HAIR_COLOR_COUNT) % HAIR_COLOR_COUNT;
    }
    else if (dressStep == STEP_OUTFIT) {
      girlOutfitStyle = (girlOutfitStyle + delta + GIRL_OUTFIT_COUNT) % GIRL_OUTFIT_COUNT;
    }
    else if (dressStep == STEP_OUTFIT_COLOR) {
      girlOutfitColor = (girlOutfitColor + delta + OUTFIT_COLOR_COUNT) % OUTFIT_COLOR_COUNT;
    }
    else if (dressStep == STEP_SHOES) {
      girlShoes = (girlShoes + delta + SHOE_COLOR_COUNT) % SHOE_COLOR_COUNT;
    }
  }
  else {
    if (dressStep == STEP_HAIR) {
      boyHairStyle = (boyHairStyle + delta + BOY_HAIR_COUNT) % BOY_HAIR_COUNT;
    }
    else if (dressStep == STEP_BEARD) {
      boyBeard = (boyBeard + delta + BOY_BEARD_COUNT) % BOY_BEARD_COUNT;
    }
    else if (dressStep == STEP_MUSTACHE) {
      boyMustache = (boyMustache + delta + BOY_MUSTACHE_COUNT) % BOY_MUSTACHE_COUNT;
    }
    else if (dressStep == STEP_HAIR_COLOR) {
      boyHairColor = (boyHairColor + delta + HAIR_COLOR_COUNT) % HAIR_COLOR_COUNT;
    }
    else if (dressStep == STEP_OUTFIT) {
      boyOutfitStyle = (boyOutfitStyle + delta + BOY_OUTFIT_COUNT) % BOY_OUTFIT_COUNT;
    }
    else if (dressStep == STEP_OUTFIT_COLOR) {
      boyOutfitColor = (boyOutfitColor + delta + OUTFIT_COLOR_COUNT) % OUTFIT_COLOR_COUNT;
    }
    else if (dressStep == STEP_SHOES) {
      boyShoes = (boyShoes + delta + SHOE_COLOR_COUNT) % SHOE_COLOR_COUNT;
    }
  }
}


void drawDressFacePreview(int x, int y) {
  // Close-up portrait mode: face/hair fills the screen instead of reading like a tiny wig on a ball.
  tft.fillRoundRect(50, 102, 72, 12, 6, C_DARK_GRAY);

  if (dressGender == DRESS_GIRL) {
    drawDressGirlFace(x, y, true);
  }
  else {
    drawDressBoyFace(x, y, true);
  }

  // Tiny sparkle crumbs, not instruction clutter.
  tft.drawPixel(134, 20, C_PINK);
  tft.drawPixel(142, 28, C_CYAN);
  tft.drawPixel(128, 42, C_YELLOW);
}

void drawDressFullPreview(int x, int y, int scaleMode) {
  tft.fillRoundRect(x - 10, y + 82, 62, 10, 5, C_DARK_GRAY);

  if (dressGender == DRESS_GIRL) {
    drawDressGirlFace(x, y, false);
    drawDressGirlBody(x, y, false);
  }
  else {
    drawDressBoyFace(x, y, false);
    drawDressBoyBody(x, y, false);
  }
}

uint16_t getDressHairColor(int index) {
  if (index == 0) return C_YELLOW;      // Blonde
  if (index == 1) return C_GREEN;       // Green
  if (index == 2) return C_BROWN;       // Brown
  if (index == 3) return C_RED;         // Red
  return C_PINK;                        // Pink
}

uint16_t getDressOutfitColor(int index) {
  if (index == 0) return C_BLUE;
  if (index == 1) return C_YELLOW;
  if (index == 2) return C_PINK;
  if (index == 3) return C_GREEN;
  if (index == 4) return C_RED;
  return C_CYAN;
}

uint16_t getDressShoeColor(int index) {
  if (index == 0) return C_BLACK;
  if (index == 1) return C_WHITE;
  if (index == 2) return C_RED;
  if (index == 3) return C_BLUE;
  return C_PINK;
}

void drawDressGirlFace(int x, int y, bool closeMode) {
  uint16_t hairColor = getDressHairColor(girlHairColor);
  int cx = closeMode ? x + 18 : x + 17;
  int cy = closeMode ? y + 38 : y + 18;
  int r  = closeMode ? 17 : 10;

  // Hair foundation first, then face, then feature/strand details.
  drawGirlHair(x, y, hairColor, closeMode);

  tft.fillCircle(cx, cy, r, C_SKIN);
  tft.drawCircle(cx, cy, r, C_BROWN);

  if (closeMode) {
    // Bigger eyes: white, iris, pupil, catchlight.
    tft.fillRoundRect(cx - 14, cy - 7, 11, 12, 4, C_WHITE);
    tft.fillRoundRect(cx + 3,  cy - 7, 11, 12, 4, C_WHITE);
    tft.fillCircle(cx - 9, cy - 1, 4, C_CYAN);
    tft.fillCircle(cx + 8, cy - 1, 4, C_CYAN);
    tft.fillCircle(cx - 8, cy, 2, C_BLACK);
    tft.fillCircle(cx + 9, cy, 2, C_BLACK);
    tft.drawPixel(cx - 11, cy - 4, C_WHITE);
    tft.drawPixel(cx + 6,  cy - 4, C_WHITE);

    // Brow / nose / mouth / cheeks.
    tft.drawLine(cx - 14, cy - 10, cx - 5, cy - 12, C_BROWN);
    tft.drawLine(cx + 5,  cy - 12, cx + 14, cy - 10, C_BROWN);
    tft.drawPixel(cx, cy + 4, C_BROWN);
    tft.drawPixel(cx + 1, cy + 5, C_BROWN);
    tft.drawPixel(cx, cy + 6, C_BROWN);
    tft.drawLine(cx - 5, cy + 12, cx + 5, cy + 12, C_RED);
    tft.drawPixel(cx - 6, cy + 11, C_RED);
    tft.drawPixel(cx + 6, cy + 11, C_RED);
    tft.drawPixel(cx - 15, cy + 7, C_PINK);
    tft.drawPixel(cx - 14, cy + 8, C_PINK);
    tft.drawPixel(cx + 14, cy + 7, C_PINK);
    tft.drawPixel(cx + 15, cy + 8, C_PINK);
  }
  else {
    tft.drawPixel(cx - 4, cy, C_BLACK);
    tft.drawPixel(cx + 4, cy, C_BLACK);
    tft.drawPixel(cx, cy + 4, C_BROWN);
    tft.drawLine(cx - 2, cy + 7, cx + 2, cy + 7, C_RED);
  }

  // Front lock overlays for styles that need them.
  if (closeMode && girlHairStyle == 0) {
    tft.fillTriangle(cx - 12, cy - 18, cx - 1, cy - 20, cx - 8, cy - 3, hairColor);
    tft.drawLine(cx - 16, cy - 8, cx - 18, cy + 18, C_BROWN);
    tft.drawLine(cx + 16, cy - 8, cx + 18, cy + 18, C_BROWN);
  }
  else if (closeMode && girlHairStyle == 2) {
    tft.fillTriangle(cx - 15, cy - 17, cx + 7, cy - 22, cx - 5, cy - 3, hairColor);
  }
  else if (closeMode && girlHairStyle == 3) {
    for (int i = -16; i <= 12; i += 7) {
      tft.fillTriangle(cx + i, cy - 18, cx + i + 3, cy - 2, cx + i + 8, cy - 18, hairColor);
    }
  }
  else if (closeMode && girlHairStyle == 4) {
    for (int i = -15; i <= 15; i += 6) {
      tft.drawPixel(cx + i, cy - 17, C_WHITE);
      tft.drawPixel(cx + i - 2, cy - 9, C_WHITE);
      tft.drawPixel(cx + i - 4, cy - 1, C_WHITE);
    }
  }
}

void drawDressBoyFace(int x, int y, bool closeMode) {
  uint16_t hairColor = getDressHairColor(boyHairColor);
  int cx = closeMode ? x + 18 : x + 17;
  int cy = closeMode ? y + 38 : y + 18;
  int r  = closeMode ? 17 : 10;

  drawBoyHair(x, y, hairColor, closeMode);

  tft.fillCircle(cx, cy, r, C_SKIN);
  tft.drawCircle(cx, cy, r, C_BROWN);

  if (closeMode) {
    tft.fillRoundRect(cx - 14, cy - 7, 11, 12, 4, C_WHITE);
    tft.fillRoundRect(cx + 3,  cy - 7, 11, 12, 4, C_WHITE);
    tft.fillCircle(cx - 9, cy - 1, 4, C_CYAN);
    tft.fillCircle(cx + 8, cy - 1, 4, C_CYAN);
    tft.fillCircle(cx - 8, cy, 2, C_BLACK);
    tft.fillCircle(cx + 9, cy, 2, C_BLACK);
    tft.drawPixel(cx - 11, cy - 4, C_WHITE);
    tft.drawPixel(cx + 6,  cy - 4, C_WHITE);

    tft.drawLine(cx - 14, cy - 10, cx - 5, cy - 12, C_BROWN);
    tft.drawLine(cx + 5,  cy - 12, cx + 14, cy - 10, C_BROWN);
    tft.drawPixel(cx, cy + 4, C_BROWN);
    tft.drawPixel(cx + 1, cy + 5, C_BROWN);
    tft.drawLine(cx - 4, cy + 11, cx + 4, cy + 11, C_RED);
  }
  else {
    tft.drawPixel(cx - 4, cy, C_BLACK);
    tft.drawPixel(cx + 4, cy, C_BLACK);
    tft.drawPixel(cx, cy + 4, C_BROWN);
  }

  drawBoyFacialHair(x, y, hairColor, closeMode);
}

void drawGirlHair(int x, int y, uint16_t hairColor, bool closeMode) {
  int cx = closeMode ? x + 18 : x + 17;
  int cy = closeMode ? y + 38 : y + 18;

  if (closeMode) {
    if (girlHairStyle == 0) { // Long: crown cap plus hanging side panels.
      tft.fillRoundRect(cx - 23, cy - 31, 46, 26, 10, hairColor);
      tft.fillRoundRect(cx - 25, cy - 15, 12, 52, 5, hairColor);
      tft.fillRoundRect(cx + 13, cy - 15, 12, 52, 5, hairColor);
      tft.fillTriangle(cx - 18, cy - 22, cx + 7, cy - 29, cx - 4, cy - 4, hairColor);
      tft.drawLine(cx - 20, cy - 5, cx - 21, cy + 31, C_BROWN);
      tft.drawLine(cx + 20, cy - 5, cx + 21, cy + 31, C_BROWN);
      tft.drawPixel(cx - 15, cy + 28, C_ORANGE);
      tft.drawPixel(cx + 15, cy + 28, C_ORANGE);
    }
    else if (girlHairStyle == 1) { // Pigtails: ties off the head, long thin tails.
      tft.fillRoundRect(cx - 18, cy - 30, 36, 23, 8, hairColor);
      tft.fillTriangle(cx - 17, cy - 14, cx - 34, cy - 3, cx - 25, cy + 3, hairColor);
      tft.fillTriangle(cx + 17, cy - 14, cx + 34, cy - 3, cx + 25, cy + 3, hairColor);
      tft.fillRoundRect(cx - 36, cy - 2, 8, 39, 4, hairColor);
      tft.fillRoundRect(cx + 28, cy - 2, 8, 39, 4, hairColor);
      tft.drawLine(cx - 32, cy + 7, cx - 31, cy + 35, C_BROWN);
      tft.drawLine(cx + 32, cy + 7, cx + 31, cy + 35, C_BROWN);
      tft.fillCircle(cx - 27, cy - 2, 3, C_PINK);
      tft.fillCircle(cx + 27, cy - 2, 3, C_PINK);
    }
    else if (girlHairStyle == 2) { // Pixie: cropped sides and swept front.
      tft.fillRoundRect(cx - 19, cy - 29, 38, 21, 8, hairColor);
      tft.fillTriangle(cx - 19, cy - 18, cx + 9, cy - 29, cx - 7, cy + 1, hairColor);
      tft.fillRoundRect(cx - 19, cy - 7, 8, 15, 4, hairColor);
      tft.drawPixel(cx + 15, cy - 12, hairColor);
      tft.drawPixel(cx + 16, cy - 11, hairColor);
      tft.drawPixel(cx + 17, cy - 10, hairColor);
    }
    else if (girlHairStyle == 3) { // Bangs: blunt fringe plus shoulder sides.
      tft.fillRoundRect(cx - 22, cy - 31, 44, 30, 8, hairColor);
      tft.fillRoundRect(cx - 22, cy - 3, 8, 28, 4, hairColor);
      tft.fillRoundRect(cx + 14, cy - 3, 8, 28, 4, hairColor);
      for (int i = -17; i <= 11; i += 7) {
        tft.fillTriangle(cx + i, cy - 19, cx + i + 4, cy - 2, cx + i + 8, cy - 19, hairColor);
      }
      tft.drawLine(cx - 19, cy - 1, cx - 20, cy + 22, C_BROWN);
      tft.drawLine(cx + 19, cy - 1, cx + 20, cy + 22, C_BROWN);
    }
    else if (girlHairStyle == 4) { // Cornrows: rows, parts, and hanging braids.
      tft.fillRoundRect(cx - 21, cy - 30, 42, 17, 6, hairColor);
      for (int i = -18; i <= 18; i += 6) {
        tft.drawLine(cx + i, cy - 30, cx + i - 4, cy - 1, hairColor);
        tft.drawLine(cx + i + 2, cy - 30, cx + i - 2, cy - 1, C_BROWN);
      }
      for (int j = 0; j < 4; j++) {
        tft.drawLine(cx - 21, cy - 3 + j * 6, cx - 16, cy + 3 + j * 6, hairColor);
        tft.drawLine(cx + 21, cy - 3 + j * 6, cx + 16, cy + 3 + j * 6, hairColor);
      }
      tft.fillRoundRect(cx - 21, cy - 1, 6, 29, 3, hairColor);
      tft.fillRoundRect(cx + 15, cy - 1, 6, 29, 3, hairColor);
    }
    else { // Afro: soft cloud silhouette made from several lobes.
      tft.fillCircle(cx, cy - 25, 21, hairColor);
      tft.fillCircle(cx - 19, cy - 18, 16, hairColor);
      tft.fillCircle(cx + 19, cy - 18, 16, hairColor);
      tft.fillCircle(cx - 9, cy - 36, 11, hairColor);
      tft.fillCircle(cx + 10, cy - 36, 11, hairColor);
      tft.fillCircle(cx - 26, cy - 5, 9, hairColor);
      tft.fillCircle(cx + 26, cy - 5, 9, hairColor);
    }
  }
  else {
    if (girlHairStyle == 1) {
      tft.fillRoundRect(x + 7, y + 7, 22, 14, 5, hairColor);
      tft.fillRect(x + 0, y + 17, 6, 20, hairColor);
      tft.fillRect(x + 31, y + 17, 6, 20, hairColor);
    }
    else if (girlHairStyle == 5) {
      tft.fillCircle(x + 18, y + 10, 15, hairColor);
      tft.fillCircle(x + 6, y + 15, 10, hairColor);
      tft.fillCircle(x + 30, y + 15, 10, hairColor);
    }
    else if (girlHairStyle == 4) {
      tft.fillRoundRect(x + 6, y + 7, 24, 11, 4, hairColor);
      for (int i = 7; i <= 28; i += 5) tft.drawLine(x + i, y + 7, x + i - 3, y + 22, C_BROWN);
    }
    else {
      tft.fillRoundRect(x + 5, y + 6, 26, 24, 8, hairColor);
    }
  }
}

void drawBoyHair(int x, int y, uint16_t hairColor, bool closeMode) {
  int cx = closeMode ? x + 18 : x + 17;
  int cy = closeMode ? y + 38 : y + 18;

  if (closeMode) {
    if (boyHairStyle == 0) { // Normal: mid-length with volume and sideburns.
      tft.fillRoundRect(cx - 20, cy - 30, 40, 23, 8, hairColor);
      tft.fillRoundRect(cx - 18, cy - 10, 8, 17, 4, hairColor);
      tft.fillRoundRect(cx + 10, cy - 10, 8, 17, 4, hairColor);
      tft.fillTriangle(cx - 17, cy - 22, cx - 2, cy - 30, cx - 8, cy - 8, hairColor);
      tft.drawLine(cx - 8, cy - 24, cx + 7, cy - 20, C_BROWN);
      tft.drawPixel(cx + 14, cy - 7, C_BROWN);
    }
    else if (boyHairStyle == 1) { // Butt-Cut: middle part curtains.
      tft.fillRoundRect(cx - 22, cy - 31, 44, 27, 8, hairColor);
      tft.drawLine(cx, cy - 31, cx, cy - 4, C_SKIN);
      tft.fillTriangle(cx - 2, cy - 28, cx - 21, cy - 12, cx - 6, cy + 4, hairColor);
      tft.fillTriangle(cx + 2, cy - 28, cx + 21, cy - 12, cx + 6, cy + 4, hairColor);
      tft.drawLine(cx - 5, cy - 23, cx - 17, cy - 8, C_BROWN);
      tft.drawLine(cx + 5, cy - 23, cx + 17, cy - 8, C_BROWN);
    }
    else if (boyHairStyle == 2) { // Emo: heavy sheet covering one eye.
      tft.fillRoundRect(cx - 21, cy - 30, 42, 20, 7, hairColor);
      tft.fillTriangle(cx - 24, cy - 24, cx + 24, cy - 21, cx - 10, cy + 13, hairColor);
      tft.fillTriangle(cx - 1, cy - 28, cx + 23, cy - 13, cx + 1, cy + 7, hairColor);
      tft.drawLine(cx - 6, cy - 22, cx - 16, cy + 3, C_BROWN);
    }
    else if (boyHairStyle == 3) { // Hat: crown plus brim, not a beanie.
      tft.fillRoundRect(cx - 20, cy - 31, 40, 11, 4, C_YELLOW);
      tft.fillRect(cx - 15, cy - 30, 30, 6, C_GOLD);
      tft.drawLine(cx - 30, cy - 20, cx + 30, cy - 20, C_YELLOW);
      tft.drawLine(cx - 24, cy - 19, cx + 24, cy - 19, C_GOLD);
    }
    else if (boyHairStyle == 4) { // Bowl-cut: blunt lower edge with round cap.
      tft.fillRoundRect(cx - 23, cy - 31, 46, 24, 10, hairColor);
      tft.drawLine(cx - 22, cy - 9, cx + 22, cy - 9, C_SKIN);
      tft.drawLine(cx - 18, cy - 11, cx + 18, cy - 11, C_BROWN);
      tft.drawPixel(cx - 21, cy - 8, hairColor);
      tft.drawPixel(cx + 21, cy - 8, hairColor);
    }
    else if (boyHairStyle == 5) { // Spikes: separated triangular locks.
      for (int i = -24; i <= 18; i += 8) {
        tft.fillTriangle(cx + i, cy - 5, cx + i + 5, cy - 36, cx + i + 11, cy - 5, hairColor);
        tft.drawLine(cx + i + 5, cy - 31, cx + i + 4, cy - 10, C_ORANGE);
      }
      tft.fillRoundRect(cx - 20, cy - 24, 40, 19, 5, hairColor);
    }
    else { // Goku: huge anime mane with long rear points.
      for (int i = -31; i <= 23; i += 7) {
        int spikeLift = 45 + (abs(i) / 2);
        tft.fillTriangle(cx + i, cy - 4, cx + i + 6, cy - spikeLift, cx + i + 14, cy - 4, hairColor);
      }
      tft.fillTriangle(cx - 9, cy - 22, cx - 24, cy + 19, cx + 8, cy - 4, hairColor);
      tft.fillTriangle(cx + 9, cy - 22, cx + 24, cy + 19, cx - 8, cy - 4, hairColor);
      tft.fillRoundRect(cx - 20, cy - 25, 40, 18, 6, hairColor);
      tft.drawPixel(cx - 19, cy - 34, C_WHITE);
      tft.drawPixel(cx + 12, cy - 39, C_WHITE);
    }
  }
  else {
    if (boyHairStyle == 2) {
      tft.fillTriangle(x + 3, y + 8, x + 34, y + 9, x + 10, y + 28, hairColor);
    }
    else if (boyHairStyle == 5) {
      tft.fillTriangle(x + 2, y + 9, x + 7, y - 5, x + 12, y + 9, hairColor);
      tft.fillTriangle(x + 11, y + 9, x + 17, y - 7, x + 22, y + 9, hairColor);
      tft.fillTriangle(x + 20, y + 9, x + 26, y - 4, x + 31, y + 9, hairColor);
    }
    else if (boyHairStyle == 6) {
      for (int i = -4; i <= 24; i += 7) {
        tft.fillTriangle(x + i, y + 9, x + i + 5, y - 12, x + i + 12, y + 9, hairColor);
      }
    }
    else if (boyHairStyle == 3) {
      tft.fillRect(x + 5, y + 5, 26, 6, C_YELLOW);
      tft.drawLine(x + 1, y + 11, x + 35, y + 11, C_YELLOW);
    }
    else {
      tft.fillRoundRect(x + 5, y + 6, 26, 14, 6, hairColor);
    }
  }
}

void drawBoyFacialHair(int x, int y, uint16_t hairColor, bool closeMode) {
  int cx = closeMode ? x + 18 : x + 17;
  int cy = closeMode ? y + 38 : y + 18;

  // Beard layer.
  if (boyBeard == 1) { // 5 o'clock shadow.
    tft.drawPixel(cx - 8, cy + 10, hairColor);
    tft.drawPixel(cx - 2, cy + 12, hairColor);
    tft.drawPixel(cx + 6, cy + 11, hairColor);
    tft.drawPixel(cx + 10, cy + 9, hairColor);
  }
  else if (boyBeard == 2) { // Grizzly.
    tft.fillRoundRect(cx - 14, cy + 8, 28, closeMode ? 20 : 9, 6, hairColor);
    tft.drawPixel(cx, cy + 10, C_SKIN);
    tft.drawLine(cx - 6, cy + 20, cx + 6, cy + 20, C_BROWN);
  }
  else if (boyBeard == 3) { // Chinstrap.
    tft.drawLine(cx - 14, cy + 5, cx - 12, cy + 18, hairColor);
    tft.drawLine(cx + 14, cy + 5, cx + 12, cy + 18, hairColor);
    tft.drawLine(cx - 12, cy + 18, cx + 12, cy + 18, hairColor);
  }

  // Mustache layer.
  if (boyMustache == 0) { // Normal.
    tft.drawLine(cx - 8, cy + 8, cx + 8, cy + 8, hairColor);
    tft.drawPixel(cx - 9, cy + 9, hairColor);
    tft.drawPixel(cx + 9, cy + 9, hairColor);
  }
  else if (boyMustache == 1) { // Goatee.
    tft.drawLine(cx - 7, cy + 8, cx + 7, cy + 8, hairColor);
    tft.fillRoundRect(cx - 4, cy + 13, 8, closeMode ? 11 : 5, 3, hairColor);
  }
  else if (boyMustache == 2) { // Handlebar.
    tft.drawLine(cx - 12, cy + 8, cx + 12, cy + 8, hairColor);
    tft.drawPixel(cx - 15, cy + 6, hairColor);
    tft.drawPixel(cx - 16, cy + 5, hairColor);
    tft.drawPixel(cx + 15, cy + 6, hairColor);
    tft.drawPixel(cx + 16, cy + 5, hairColor);
  }
  else if (boyMustache == 3) { // Fu Manchu.
    tft.drawLine(cx - 9, cy + 8, cx + 9, cy + 8, hairColor);
    tft.drawLine(cx - 9, cy + 8, cx - 12, cy + 22, hairColor);
    tft.drawLine(cx + 9, cy + 8, cx + 12, cy + 22, hairColor);
  }
  else { // Toothbrush.
    tft.fillRect(cx - 3, cy + 8, 6, 5, hairColor);
  }
}

void drawDressGirlBody(int x, int y, bool closeMode) {
  uint16_t outfitColor = getDressOutfitColor(girlOutfitColor);
  uint16_t shoeColor = getDressShoeColor(girlShoes);

  // Neck and shoulders.
  tft.fillRect(x + 14, y + 28, 8, 8, C_SKIN);
  tft.drawLine(x + 9, y + 36, x + 27, y + 36, C_SKIN);

  if (girlOutfitStyle == 0) { // Casual: tee + jeans.
    tft.fillRoundRect(x + 7, y + 35, 23, 19, 4, outfitColor);
    tft.drawLine(x + 8, y + 42, x + 29, y + 42, C_WHITE);
    tft.fillRect(x + 10, y + 53, 7, 20, C_DARK_BLUE);
    tft.fillRect(x + 21, y + 53, 7, 20, C_DARK_BLUE);
    tft.drawPixel(x + 18, y + 41, C_WHITE);
  }
  else if (girlOutfitStyle == 1) { // Gym: crop top/shorts.
    tft.fillRect(x + 8, y + 35, 21, 13, outfitColor);
    tft.drawLine(x + 8, y + 48, x + 29, y + 48, C_WHITE);
    tft.fillRect(x + 9, y + 49, 19, 9, C_DARK_GRAY);
    tft.fillRect(x + 11, y + 58, 6, 15, C_SKIN);
    tft.fillRect(x + 21, y + 58, 6, 15, C_SKIN);
  }
  else if (girlOutfitStyle == 2) { // Fancy: flared dress.
    tft.fillTriangle(x + 18, y + 34, x + 3, y + 74, x + 33, y + 74, outfitColor);
    tft.drawLine(x + 9, y + 48, x + 27, y + 48, C_WHITE);
    tft.drawPixel(x + 18, y + 46, C_WHITE);
    tft.drawPixel(x + 13, y + 58, C_WHITE);
    tft.drawPixel(x + 23, y + 65, C_WHITE);
  }
  else if (girlOutfitStyle == 3) { // Streetwear: oversized jacket + dark pants.
    tft.fillRoundRect(x + 5, y + 35, 27, 25, 5, outfitColor);
    tft.drawLine(x + 18, y + 35, x + 18, y + 60, C_WHITE);
    tft.drawPixel(x + 12, y + 44, C_GOLD);
    tft.drawPixel(x + 24, y + 44, C_GOLD);
    tft.fillRect(x + 9, y + 59, 8, 15, C_BLACK);
    tft.fillRect(x + 21, y + 59, 8, 15, C_BLACK);
  }
  else { // Pajamas: one-piece with dots.
    tft.fillRoundRect(x + 8, y + 35, 21, 38, 5, outfitColor);
    tft.drawLine(x + 9, y + 49, x + 28, y + 49, C_WHITE);
    tft.drawPixel(x + 14, y + 45, C_WHITE);
    tft.drawPixel(x + 22, y + 52, C_WHITE);
    tft.drawPixel(x + 15, y + 63, C_WHITE);
  }

  // Arms and hands.
  tft.drawLine(x + 8, y + 40, x + 1, y + 53, C_SKIN);
  tft.drawLine(x + 28, y + 40, x + 35, y + 53, C_SKIN);
  tft.drawPixel(x + 1, y + 54, C_SKIN);
  tft.drawPixel(x + 35, y + 54, C_SKIN);

  tft.fillRect(x + 7, y + 74, 11, 4, shoeColor);
  tft.fillRect(x + 20, y + 74, 11, 4, shoeColor);
}

void drawDressBoyBody(int x, int y, bool closeMode) {
  uint16_t outfitColor = getDressOutfitColor(boyOutfitColor);
  uint16_t shoeColor = getDressShoeColor(boyShoes);

  tft.fillRect(x + 14, y + 28, 8, 8, C_SKIN);
  tft.drawLine(x + 8, y + 36, x + 28, y + 36, C_SKIN);

  if (boyOutfitStyle == 0) { // Casual: tee.
    tft.fillRoundRect(x + 8, y + 35, 21, 22, 4, outfitColor);
    tft.drawLine(x + 9, y + 45, x + 28, y + 45, C_WHITE);
    tft.drawPixel(x + 18, y + 44, C_WHITE);
  }
  else if (boyOutfitStyle == 1) { // Gym: tank + shorts.
    tft.fillRect(x + 8, y + 35, 21, 14, outfitColor);
    tft.fillRect(x + 9, y + 49, 19, 9, C_DARK_GRAY);
    tft.drawLine(x + 8, y + 49, x + 29, y + 49, C_WHITE);
  }
  else if (boyOutfitStyle == 2) { // Fancy: suit.
    tft.fillRect(x + 7, y + 35, 24, 29, C_BLACK);
    tft.fillTriangle(x + 18, y + 36, x + 13, y + 50, x + 23, y + 50, C_WHITE);
    tft.drawPixel(x + 18, y + 51, C_RED);
    tft.drawLine(x + 12, y + 36, x + 18, y + 48, C_DARK_GRAY);
    tft.drawLine(x + 24, y + 36, x + 18, y + 48, C_DARK_GRAY);
  }
  else if (boyOutfitStyle == 3) { // Gangsta: big jacket/chain.
    tft.fillRoundRect(x + 5, y + 35, 28, 25, 5, outfitColor);
    tft.drawLine(x + 8, y + 45, x + 28, y + 45, C_GOLD);
    tft.drawPixel(x + 18, y + 48, C_GOLD);
    tft.drawPixel(x + 17, y + 49, C_GOLD);
    tft.drawPixel(x + 19, y + 49, C_GOLD);
  }
  else { // Skater: hoodie/shorts/board.
    tft.fillRoundRect(x + 5, y + 35, 28, 20, 4, outfitColor);
    tft.drawPixel(x + 14, y + 45, C_WHITE);
    tft.drawPixel(x + 22, y + 45, C_WHITE);
    tft.fillRect(x + 8, y + 55, 23, 18, C_DARK_GRAY);
    tft.drawLine(x + 3, y + 78, x + 35, y + 78, C_BROWN);
    tft.drawPixel(x + 8, y + 80, C_BLACK);
    tft.drawPixel(x + 30, y + 80, C_BLACK);
  }

  // Pants and arms.
  tft.fillRect(x + 9, y + 56, 8, 18, C_DARK_BLUE);
  tft.fillRect(x + 21, y + 56, 8, 18, C_DARK_BLUE);
  tft.drawLine(x + 8, y + 40, x + 1, y + 53, C_SKIN);
  tft.drawLine(x + 28, y + 40, x + 35, y + 53, C_SKIN);

  tft.fillRect(x + 7, y + 74, 11, 4, shoeColor);
  tft.fillRect(x + 20, y + 74, 11, 4, shoeColor);
}


const char* getDressStepName() {
  if (dressStep == STEP_HAIR) return "Hair";
  if (dressStep == STEP_BEARD) return "Beard";
  if (dressStep == STEP_MUSTACHE) return "Mustache";
  if (dressStep == STEP_HAIR_COLOR) return "Hair Color";
  if (dressStep == STEP_OUTFIT) return "Outfit";
  if (dressStep == STEP_OUTFIT_COLOR) return "Outfit Color";
  if (dressStep == STEP_SHOES) return "Shoes";
  return "";
}

const char* getDressValueName() {
  if (dressGender == DRESS_GIRL) {
    if (dressStep == STEP_HAIR) return getGirlHairName(girlHairStyle);
    if (dressStep == STEP_HAIR_COLOR) return getHairColorNameNew(girlHairColor);
    if (dressStep == STEP_OUTFIT) return getGirlOutfitName(girlOutfitStyle);
    if (dressStep == STEP_OUTFIT_COLOR) return getOutfitColorNameNew(girlOutfitColor);
    if (dressStep == STEP_SHOES) return getShoeColorName(girlShoes);
  }
  else {
    if (dressStep == STEP_HAIR) return getBoyHairName(boyHairStyle);
    if (dressStep == STEP_BEARD) return getBoyBeardName(boyBeard);
    if (dressStep == STEP_MUSTACHE) return getBoyMustacheName(boyMustache);
    if (dressStep == STEP_HAIR_COLOR) return getHairColorNameNew(boyHairColor);
    if (dressStep == STEP_OUTFIT) return getBoyOutfitName(boyOutfitStyle);
    if (dressStep == STEP_OUTFIT_COLOR) return getOutfitColorNameNew(boyOutfitColor);
    if (dressStep == STEP_SHOES) return getShoeColorName(boyShoes);
  }
  return "";
}

const char* getGirlHairName(int index) {
  if (index == 0) return "Long";
  if (index == 1) return "Pigtails";
  if (index == 2) return "Pixie";
  if (index == 3) return "Bangs";
  if (index == 4) return "Cornrows";
  return "Afro";
}

const char* getBoyHairName(int index) {
  if (index == 0) return "Normal";
  if (index == 1) return "Butt-Cut";
  if (index == 2) return "Emo";
  if (index == 3) return "Hat";
  if (index == 4) return "Bowl-Cut";
  if (index == 5) return "Spikes";
  return "Goku";
}

const char* getHairColorNameNew(int index) {
  if (index == 0) return "Blonde";
  if (index == 1) return "Green";
  if (index == 2) return "Brown";
  if (index == 3) return "Red";
  return "Pink";
}

const char* getGirlOutfitName(int index) {
  if (index == 0) return "Casual";
  if (index == 1) return "Gym";
  if (index == 2) return "Fancy";
  if (index == 3) return "Streetwear";
  return "Pajamas";
}

const char* getBoyOutfitName(int index) {
  if (index == 0) return "Casual";
  if (index == 1) return "Gym";
  if (index == 2) return "Fancy";
  if (index == 3) return "Gangsta";
  return "Skater";
}

const char* getOutfitColorNameNew(int index) {
  if (index == 0) return "Blue";
  if (index == 1) return "Yellow";
  if (index == 2) return "Pink";
  if (index == 3) return "Green";
  if (index == 4) return "Red";
  return "Cyan";
}

const char* getShoeColorName(int index) {
  if (index == 0) return "Black";
  if (index == 1) return "White";
  if (index == 2) return "Red";
  if (index == 3) return "Blue";
  return "Pink";
}

const char* getBoyBeardName(int index) {
  if (index == 0) return "Normal";
  if (index == 1) return "5 OClock";
  if (index == 2) return "Grizzly";
  return "Chinstrap";
}

const char* getBoyMustacheName(int index) {
  if (index == 0) return "Normal";
  if (index == 1) return "Goatee";
  if (index == 2) return "Handlebar";
  if (index == 3) return "Fu Manchu";
  return "Toothbrush";
}

void startDressPlay() {
  appState = DRESS_PLAY;
  dressPlayX = 68;
  dressPlayY = DRESS_PLAY_GROUND_Y - dressPlayH;
  oldDressPlayX = dressPlayX;
  oldDressPlayY = dressPlayY;
  dressPlayVY = 0;
  dressPlayOnGround = true;
  dressPlayJumpHeld = false;
  dressPlayFacingRight = true;

  tft.fillScreen(C_BLACK);
  drawDressPlayScene();
}

void updateDressPlay() {
  if (joyJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
    return;
  }

  if (millis() - lastFrame < frameDelay) {
    return;
  }
  lastFrame = millis();

  oldDressPlayX = dressPlayX;
  oldDressPlayY = dressPlayY;

  updateDressPlayPhysics();
  drawDressPlayFrame();
}

void updateDressPlayPhysics() {
  if (direction == "LEFT") {
    dressPlayX -= dressPlaySpeed;
    dressPlayFacingRight = false;
  }
  else if (direction == "RIGHT") {
    dressPlayX += dressPlaySpeed;
    dressPlayFacingRight = true;
  }

  if (direction == "UP") {
    dressPlayY -= dressPlaySpeed;
  }
  else if (direction == "DOWN") {
    dressPlayY += dressPlaySpeed;
  }

  if (dressPlayX < 2) dressPlayX = 2;
  if (dressPlayX > tft.width() - dressPlayW - 2) dressPlayX = tft.width() - dressPlayW - 2;

  if (aPressed && !dressPlayJumpHeld && dressPlayOnGround) {
    dressPlayVY = dressPlayJumpVelocity;
    dressPlayOnGround = false;
    dressPlayJumpHeld = true;
  }
  if (!aPressed) {
    dressPlayJumpHeld = false;
  }

  dressPlayVY += dressPlayGravity;
  if (dressPlayVY > dressPlayMaxFall) dressPlayVY = dressPlayMaxFall;

  dressPlayY += (int)dressPlayVY;

  if (dressPlayY < 18) dressPlayY = 18;

  if (dressPlayY > DRESS_PLAY_GROUND_Y - dressPlayH) {
    dressPlayY = DRESS_PLAY_GROUND_Y - dressPlayH;
    dressPlayVY = 0;
    dressPlayOnGround = true;
  }
}

void drawDressPlayScene() {
  tft.fillScreen(C_BLACK);
  tft.fillRect(0, 0, tft.width(), DRESS_PLAY_GROUND_Y, C_SKY);
  tft.fillRect(0, DRESS_PLAY_GROUND_Y, tft.width(), tft.height() - DRESS_PLAY_GROUND_Y, C_GREEN);
  tft.drawLine(0, DRESS_PLAY_GROUND_Y, tft.width(), DRESS_PLAY_GROUND_Y, C_WHITE);

  // blank playground with tiny stars/grass
  tft.drawPixel(20, 22, C_YELLOW);
  tft.drawPixel(138, 30, C_PINK);
  tft.drawPixel(52, 42, C_CYAN);
  tft.drawPixel(102, 26, C_WHITE);
  tft.drawPixel(35, 118, C_DARK_GREEN);
  tft.drawPixel(92, 121, C_DARK_GREEN);
  tft.drawPixel(128, 116, C_DARK_GREEN);

  drawDressPlayCharacter(dressPlayX, dressPlayY);
}

void drawDressPlayFrame() {
  bool moved = oldDressPlayX != dressPlayX || oldDressPlayY != dressPlayY;
  if (!moved) return;

  int ux = min(oldDressPlayX, dressPlayX) - 8;
  int uy = min(oldDressPlayY, dressPlayY) - 8;
  int ux2 = max(oldDressPlayX + dressPlayW, dressPlayX + dressPlayW) + 16;
  int uy2 = max(oldDressPlayY + dressPlayH, dressPlayY + dressPlayH) + 12;

  redrawDressPlayPatch(ux, uy, ux2 - ux, uy2 - uy);
  drawDressPlayCharacter(dressPlayX, dressPlayY);
}

void redrawDressPlayPatch(int x, int y, int w, int h) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > tft.width()) w = tft.width() - x;
  if (y + h > tft.height()) h = tft.height() - y;
  if (w <= 0 || h <= 0) return;

  if (y >= DRESS_PLAY_GROUND_Y) {
    tft.fillRect(x, y, w, h, C_GREEN);
  }
  else {
    tft.fillRect(x, y, w, h, C_SKY);
    if (y + h > DRESS_PLAY_GROUND_Y) {
      tft.fillRect(x, DRESS_PLAY_GROUND_Y, w, y + h - DRESS_PLAY_GROUND_Y, C_GREEN);
      tft.drawLine(x, DRESS_PLAY_GROUND_Y, x + w, DRESS_PLAY_GROUND_Y, C_WHITE);
    }
  }

  if (rectsOverlap(x, y, w, h, 18, 20, 125, 25)) {
    tft.drawPixel(20, 22, C_YELLOW);
    tft.drawPixel(138, 30, C_PINK);
    tft.drawPixel(52, 42, C_CYAN);
    tft.drawPixel(102, 26, C_WHITE);
  }
  if (rectsOverlap(x, y, w, h, 30, 114, 105, 12)) {
    tft.drawPixel(35, 118, C_DARK_GREEN);
    tft.drawPixel(92, 121, C_DARK_GREEN);
    tft.drawPixel(128, 116, C_DARK_GREEN);
  }
}

void drawDressPlayCharacter(int x, int y) {
  // Compact playable version so it never gets chopped in half, but keeps the chosen silhouette.
  uint16_t hairColor = (dressGender == DRESS_GIRL) ? getDressHairColor(girlHairColor) : getDressHairColor(boyHairColor);
  uint16_t outfitColor = (dressGender == DRESS_GIRL) ? getDressOutfitColor(girlOutfitColor) : getDressOutfitColor(boyOutfitColor);
  uint16_t shoeColor = (dressGender == DRESS_GIRL) ? getDressShoeColor(girlShoes) : getDressShoeColor(boyShoes);

  int cx = x + 12;

  if (dressGender == DRESS_GIRL) {
    if (girlHairStyle == 1) { // pigtails
      tft.fillRoundRect(x + 6, y + 2, 12, 10, 4, hairColor);
      tft.drawLine(x + 6, y + 11, x + 1, y + 15, hairColor);
      tft.drawLine(x + 18, y + 11, x + 23, y + 15, hairColor);
      tft.fillRect(x + 1, y + 15, 5, 15, hairColor);
      tft.fillRect(x + 18, y + 15, 5, 15, hairColor);
      tft.drawPixel(x + 3, y + 14, C_PINK);
      tft.drawPixel(x + 21, y + 14, C_PINK);
    }
    else if (girlHairStyle == 5) { // afro
      tft.fillCircle(cx, y + 7, 10, hairColor);
      tft.fillCircle(cx - 8, y + 10, 7, hairColor);
      tft.fillCircle(cx + 8, y + 10, 7, hairColor);
    }
    else if (girlHairStyle == 4) { // cornrows
      tft.fillRoundRect(x + 4, y + 2, 16, 10, 4, hairColor);
      for (int i = 4; i <= 20; i += 5) tft.drawLine(x + i, y + 2, x + i - 2, y + 17, C_BROWN);
    }
    else {
      tft.fillRoundRect(x + 4, y + 2, 16, 14, 5, hairColor);
    }
  }
  else {
    if (boyHairStyle == 2) { // emo
      tft.fillTriangle(x + 2, y + 5, x + 22, y + 5, x + 7, y + 20, hairColor);
    }
    else if (boyHairStyle == 3) { // hat
      tft.fillRect(x + 4, y + 2, 16, 5, C_YELLOW);
      tft.drawLine(x + 1, y + 7, x + 23, y + 7, C_YELLOW);
    }
    else if (boyHairStyle == 5) { // spikes
      tft.fillTriangle(x + 2, y + 7, x + 6, y - 3, x + 10, y + 7, hairColor);
      tft.fillTriangle(x + 8, y + 7, x + 12, y - 5, x + 16, y + 7, hairColor);
      tft.fillTriangle(x + 14, y + 7, x + 18, y - 3, x + 22, y + 7, hairColor);
    }
    else if (boyHairStyle == 6) { // goku
      tft.fillTriangle(x + 0, y + 7, x + 6, y - 8, x + 12, y + 7, hairColor);
      tft.fillTriangle(x + 7, y + 7, x + 13, y - 12, x + 19, y + 7, hairColor);
      tft.fillTriangle(x + 14, y + 7, x + 22, y - 7, x + 24, y + 7, hairColor);
    }
    else {
      tft.fillRoundRect(x + 4, y + 2, 16, 10, 4, hairColor);
    }
  }

  tft.fillCircle(cx, y + 12, 7, C_SKIN);
  tft.drawPixel(cx - 3, y + 11, C_BLACK);
  tft.drawPixel(cx + 3, y + 11, C_BLACK);
  tft.drawPixel(cx, y + 15, C_BROWN);
  tft.drawLine(cx - 2, y + 17, cx + 2, y + 17, C_RED);

  if (dressGender == DRESS_BOY && boyBeard > 0) {
    tft.drawLine(cx - 5, y + 17, cx + 5, y + 17, hairColor);
  }
  if (dressGender == DRESS_BOY && boyMustache > 0) {
    tft.drawLine(cx - 4, y + 15, cx + 4, y + 15, hairColor);
  }

  tft.fillRoundRect(x + 5, y + 20, 14, 17, 3, outfitColor);
  tft.drawLine(x + 5, y + 24, x + 1, y + 32, C_SKIN);
  tft.drawLine(x + 19, y + 24, x + 23, y + 32, C_SKIN);

  tft.fillRect(x + 6, y + 36, 5, 10, C_DARK_BLUE);
  tft.fillRect(x + 14, y + 36, 5, 10, C_DARK_BLUE);
  tft.fillRect(x + 5, y + 46, 7, 3, shoeColor);
  tft.fillRect(x + 14, y + 46, 7, 3, shoeColor);
}

// ======================================================
// LEVEL 1: SCHOOL ROOM PLATFORMER
// ======================================================

void startSchoolRoom() {
  appState = SCHOOL_ROOM;

  girlX = 12;
  girlY = 96;
  oldGirlX = girlX;
  oldGirlY = girlY;

  girlVX = 0;
  girlVY = 0;

  onGround = false;
  jumpHeld = false;
  facingRight = true;

  hatCollected = false;
  oldHatCollected = false;

  tft.fillScreen(C_BLACK);
  drawSchoolScene();
}

void updateSchoolRoom() {
  if (millis() - lastFrame < frameDelay) {
    return;
  }

  lastFrame = millis();

  oldGirlX = girlX;
  oldGirlY = girlY;
  oldHatCollected = hatCollected;

  updateGirlPhysics();
  checkHatPickup();
  checkDoorEnter();

  if (appState != SCHOOL_ROOM) {
    return;
  }

  drawSchoolFrame();
}

void updateGirlPhysics() {
  float speed = onGround ? groundMoveSpeed : airMoveSpeed;

  girlVX = 0;

  if (direction == "LEFT") {
    girlVX = -speed;
    facingRight = false;
  }
  else if (direction == "RIGHT") {
    girlVX = speed;
    facingRight = true;
  }

  bool standingOnSomething = onGround || girlY >= GROUND_Y - girlH - 1;

  if (aPressed && !jumpHeld && standingOnSomething) {
    girlVY = jumpVelocity;
    onGround = false;
    jumpHeld = true;
  }

  if (!aPressed) {
    jumpHeld = false;
  }

  girlVY += gravity;

  if (girlVY > maxFallSpeed) {
    girlVY = maxFallSpeed;
  }

  girlX += girlVX;
  girlY += girlVY;

  handlePlatformCollisions();
  clampGirlToScreen();
}

void handlePlatformCollisions() {
  onGround = false;

  for (int i = 0; i < PLATFORM_COUNT; i++) {
    Platform p = platforms[i];

    bool wasAbove = oldGirlY + girlH <= p.y;
    bool nowOverlapsX = girlX + girlW > p.x && girlX < p.x + p.w;
    bool nowHitsY = girlY + girlH >= p.y && girlY + girlH <= p.y + p.h + 4;

    if (girlVY >= 0 && wasAbove && nowOverlapsX && nowHitsY) {
      girlY = p.y - girlH;
      girlVY = 0;
      onGround = true;
    }
  }
}

void clampGirlToScreen() {
  if (girlX < 2) {
    girlX = 2;
  }

  if (girlX > tft.width() - girlW - 2) {
    girlX = tft.width() - girlW - 2;
  }

  if (girlY < HEADER_H + 2) {
    girlY = HEADER_H + 2;
    girlVY = 0;
  }

  if (girlY > GROUND_Y - girlH) {
    girlY = GROUND_Y - girlH;
    girlVY = 0;
    onGround = true;
  }
}

void checkHatPickup() {
  if (hatCollected) {
    return;
  }

  if (rectsOverlap((int)girlX, (int)girlY, girlW, girlH, hatX, hatY, hatW, hatH)) {
    hatCollected = true;
  }
}

void checkDoorEnter() {
  if (!hatCollected) {
    return;
  }

  if (rectsOverlap((int)girlX, (int)girlY, girlW, girlH, doorX, doorY, doorW, doorH)) {
    appState = GARDEN_INTRO;
    drawGardenIntro();
  }
}

void drawSchoolScene() {
  drawSchoolStatic();
  drawHeader();

  if (!hatCollected) {
    drawHat();
  }

  drawGirl();
}

void drawSchoolFrame() {
  int dx = girlDrawX();
  int dy = girlDrawY();
  int odx = (int)oldGirlX - 1;
  int ody = (int)oldGirlY;

  bool girlMoved = (odx != dx || ody != dy);
  bool hatChanged = oldHatCollected != hatCollected;

  if (girlMoved) {
    int ux = min(odx, dx);
    int uy = min(ody, dy);
    int ux2 = max(odx + girlW, dx + girlW);
    int uy2 = max(ody + girlH, dy + girlH);

    redrawSchoolPatch(ux, uy, ux2 - ux, uy2 - uy);

    for (int i = 0; i < PLATFORM_COUNT; i++) {
      Platform p = platforms[i];

      if (rectsOverlap(ux, uy, ux2 - ux, uy2 - uy, p.x, p.y, p.w, p.h)) {
        drawSinglePlatform(p);
      }
    }

    if (rectsOverlap(ux, uy, ux2 - ux, uy2 - uy, doorX, doorY, doorW, doorH)) {
      drawDoor();
    }

    if (!hatCollected && rectsOverlap(ux, uy, ux2 - ux, uy2 - uy, hatX, hatY, hatW, hatH)) {
      drawHat();
    }

    drawGirl();
  }

  if (hatChanged) {
    redrawSchoolPatch(hatX - 4, hatY - 4, hatW + 8, hatH + 8);
    drawDoor();
    drawHeader();
    drawGirl();
  }
}

void drawSchoolStatic() {
  tft.fillRect(0, HEADER_H, tft.width(), tft.height() - HEADER_H, C_TAN);

  for (int y = HEADER_H + 8; y < GROUND_Y; y += 14) {
    tft.drawLine(0, y, tft.width(), y, C_BROWN);
  }

  for (int x = 0; x < tft.width(); x += 22) {
    tft.drawLine(x, HEADER_H + 2, x, GROUND_Y, C_BROWN);
  }

  tft.fillRect(0, GROUND_Y, tft.width(), tft.height() - GROUND_Y, C_BROWN);

  drawArchedWindow(10, 30);
  drawArchedWindow(48, 30);
  drawDoor();
  drawVines();

  drawPlatforms();

  tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
}

void redrawSchoolPatch(int x, int y, int w, int h) {
  if (x < 0) {
    w += x;
    x = 0;
  }

  if (y < HEADER_H) {
    h += (y - HEADER_H);
    y = HEADER_H;
  }

  if (x + w > tft.width()) {
    w = tft.width() - x;
  }

  if (y + h > tft.height()) {
    h = tft.height() - y;
  }

  if (w <= 0 || h <= 0) {
    return;
  }

  if (y >= GROUND_Y) {
    tft.fillRect(x, y, w, h, C_BROWN);
  }
  else {
    tft.fillRect(x, y, w, h, C_TAN);

    for (int sy = HEADER_H + 8; sy < GROUND_Y; sy += 14) {
      if (sy >= y && sy <= y + h) {
        tft.drawLine(x, sy, x + w, sy, C_BROWN);
      }
    }

    for (int sx = 0; sx < tft.width(); sx += 22) {
      if (sx >= x && sx <= x + w) {
        tft.drawLine(sx, max(y, HEADER_H + 2), sx, min(y + h, GROUND_Y), C_BROWN);
      }
    }

    if (y + h > GROUND_Y) {
      tft.fillRect(x, GROUND_Y, w, (y + h) - GROUND_Y, C_BROWN);
    }
  }

  if (rectsOverlap(x, y, w, h, 10, 24, 22, 34)) {
    drawArchedWindow(10, 30);
  }

  if (rectsOverlap(x, y, w, h, 48, 24, 22, 34)) {
    drawArchedWindow(48, 30);
  }

  if (x <= 2 || y <= HEADER_H + 3 || x + w >= tft.width() - 2 || y + h >= tft.height() - 2) {
    tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
  }
}

void drawHeader() {
  tft.fillRect(0, 0, tft.width(), HEADER_H, C_BLUE);

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 5);
  tft.print("OLD SCHOOL");

  tft.setCursor(86, 5);

  if (hatCollected) {
    tft.setTextColor(C_CYAN);
    tft.print("HAT!");
  }
  else {
    tft.setTextColor(C_WHITE);
    tft.print("Find hat");
  }
}

void drawArchedWindow(int x, int y) {
  tft.fillRect(x, y + 6, 22, 22, C_SKY);
  tft.fillCircle(x + 11, y + 7, 11, C_SKY);

  tft.drawRect(x, y + 6, 22, 22, C_WHITE);
  tft.drawCircle(x + 11, y + 7, 11, C_WHITE);

  tft.drawLine(x + 11, y + 2, x + 11, y + 28, C_WHITE);
  tft.drawLine(x + 2, y + 17, x + 20, y + 17, C_WHITE);
}

void drawVines() {
  tft.drawLine(2, HEADER_H + 4, 20, 48, C_DARK_GREEN);
  tft.drawLine(20, 48, 34, 72, C_DARK_GREEN);

  tft.drawLine(76, HEADER_H + 3, 92, 44, C_DARK_GREEN);
  tft.drawLine(92, 44, 118, 66, C_DARK_GREEN);

  tft.drawLine(150, HEADER_H + 6, 132, 48, C_DARK_GREEN);
  tft.drawLine(132, 48, 120, 82, C_DARK_GREEN);

  for (int i = 0; i < 8; i++) {
    int lx = 8 + i * 15;
    int ly = HEADER_H + 12 + (i % 3) * 12;
    tft.fillCircle(lx, ly, 2, C_GREEN);
  }

  tft.fillCircle(91, 39, 2, C_GREEN);
  tft.fillCircle(101, 50, 2, C_GREEN);
  tft.fillCircle(115, 63, 2, C_GREEN);
  tft.fillCircle(137, 42, 2, C_GREEN);
  tft.fillCircle(127, 62, 2, C_GREEN);
}

void drawSinglePlatform(Platform p) {
  tft.fillRect(p.x, p.y, p.w, p.h, C_DARK_GRAY);
  tft.drawLine(p.x, p.y, p.x + p.w - 1, p.y, C_WHITE);
  tft.drawLine(p.x, p.y + p.h, p.x + p.w - 1, p.y + p.h, C_BROWN);
}

void drawPlatforms() {
  for (int i = 0; i < PLATFORM_COUNT; i++) {
    drawSinglePlatform(platforms[i]);
  }
}

void drawDoor() {
  uint16_t doorColor = hatCollected ? C_GOLD : C_DOOR;

  tft.fillRect(doorX, doorY, doorW, doorH, doorColor);
  tft.drawRect(doorX, doorY, doorW, doorH, C_WHITE);

  tft.drawCircle(doorX + 8, doorY + 4, 7, C_WHITE);
  tft.drawPixel(doorX + 12, doorY + 18, C_BLACK);

  if (!hatCollected) {
    tft.drawLine(doorX + 3, doorY + 18, doorX + 13, doorY + 18, C_RED);
  }
}

void drawHat() {
  // Yellow hat pickup with a clear brim, so it reads as a hat, not a beanie.
  tft.drawRect(hatX - 2, hatY - 2, hatW + 4, hatH + 4, C_WHITE);

  tft.fillRect(hatX + 3, hatY + 2, 8, 4, C_YELLOW);
  tft.drawRect(hatX + 3, hatY + 2, 8, 4, C_BROWN);

  // wide brim
  tft.drawLine(hatX,     hatY + 7, hatX + 13, hatY + 7, C_YELLOW);
  tft.drawLine(hatX + 1, hatY + 8, hatX + 12, hatY + 8, C_BROWN);

  // small ribbon/shadow
  tft.drawPixel(hatX + 7, hatY + 5, C_ORANGE);
  tft.drawPixel(hatX + 8, hatY + 5, C_ORANGE);
}

void drawGirl() {
  int x = girlDrawX();
  int y = girlDrawY();

  // Level 1 story rule:
  // Madelyn starts bare-headed. Once she picks up the yellow hat,
  // this same sprite switches to the hat version immediately.
  static const char* const rowsRightNoHat[] = {
    "   RRRRRR      ", "  RRRRRRRR     ", " RRRSSSS       ", "RRRSSKSS       ",
    "RRRSSSSS  R    ", " RRRSSS  RR    ", "   SSSSS       ", "  SYYYYYS      ",
    " SYYYYYYYYS    ", "SYYYYYYYYYYS   ", " OOYYYYYOO     ", "   YYYYY       ",
    "  YYYYYYY      ", " YYYYYYYYY     ", "YYYYYYYYYYY    ", "   K   K       ",
    "  KK   KK      ", "               ", "               "
  };
  static const char* const rowsLeftNoHat[] = {
    "      RRRRRR   ", "     RRRRRRRR  ", "       SSSSRRR ", "       SSKSSRRR",
    "    R  SSSSSRRR", "    RR  SSSRRR ", "       SSSSS   ", "      SYYYYYS  ",
    "    SYYYYYYYYS ", "   SYYYYYYYYYYS", "     OOYYYYYOO ", "       YYYYY   ",
    "      YYYYYYY  ", "     YYYYYYYYY ", "    YYYYYYYYYYY", "       K   K   ",
    "      KK   KK  ", "               ", "               "
  };
  static const char* const rowsRightHat[] = {
    "   YYYYYYY     ", " YYYYYYYYYYYY  ", " RRRSSSS       ", "RRRSSKSS       ",
    "RRRSSSSS  R    ", " RRRSSS  RR    ", "   SSSSS       ", "  SYYYYYS      ",
    " SYYYYYYYYS    ", "SYYYYYYYYYYS   ", " OOYYYYYOO     ", "   YYYYY       ",
    "  YYYYYYY      ", " YYYYYYYYY     ", "YYYYYYYYYYY    ", "   K   K       ",
    "  KK   KK      ", "               ", "               "
  };
  static const char* const rowsLeftHat[] = {
    "     YYYYYYY   ", "  YYYYYYYYYYYY ", "       SSSSRRR ", "       SSKSSRRR",
    "    R  SSSSSRRR", "    RR  SSSRRR ", "       SSSSS   ", "      SYYYYYS  ",
    "    SYYYYYYYYS ", "   SYYYYYYYYYYS", "     OOYYYYYOO ", "       YYYYY   ",
    "      YYYYYYY  ", "     YYYYYYYYY ", "    YYYYYYYYYYY", "       K   K   ",
    "      KK   KK  ", "               ", "               "
  };

  if (hatCollected) {
    drawSpriteRows(x, y, facingRight ? rowsRightHat : rowsLeftHat, 19);
  }
  else {
    drawSpriteRows(x, y, facingRight ? rowsRightNoHat : rowsLeftNoHat, 19);
  }
}




// ======================================================
// LEVEL 2: GARDEN INTRO + MAZE
// ======================================================

void drawGardenIntro() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 18);
  tft.println("HAT");

  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 42);
  tft.println("FOUND!");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 76);
  tft.println("Now find your friend");
  tft.setCursor(8, 90);
  tft.println("in the garden.");

  tft.setTextColor(C_RED);
  tft.setCursor(8, 110);
  tft.println("Press A");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void updateGardenIntro() {
  if (aJustPressed() || joyJustPressed()) {
    startGardenMaze();
  }
}

void startGardenMaze() {
  appState = GARDEN_MAZE;

  gardenX = 12;
  gardenY = 108;
  oldGardenX = gardenX;
  oldGardenY = gardenY;

  tft.fillScreen(C_BLACK);
  drawGardenScene();
}

void updateGardenMaze() {
  if (millis() - lastFrame < frameDelay) {
    return;
  }

  lastFrame = millis();

  oldGardenX = gardenX;
  oldGardenY = gardenY;

  updateGardenMovement();
  checkFriendFound();
  drawGardenFrame();
}

void updateGardenMovement() {
  int nextX = gardenX;
  int nextY = gardenY;

  if (direction == "LEFT") {
    nextX -= gardenSpeed;
  }
  else if (direction == "RIGHT") {
    nextX += gardenSpeed;
  }
  else if (direction == "UP") {
    nextY -= gardenSpeed;
  }
  else if (direction == "DOWN") {
    nextY += gardenSpeed;
  }

  if (!gardenHitsWall(nextX, nextY)) {
    gardenX = nextX;
    gardenY = nextY;
  }
  else {
    if (!gardenHitsWall(nextX, gardenY)) {
      gardenX = nextX;
    }

    if (!gardenHitsWall(gardenX, nextY)) {
      gardenY = nextY;
    }
  }
}

bool gardenHitsWall(int x, int y) {
  for (int i = 0; i < GARDEN_OBSTACLE_COUNT; i++) {
    Rect r = gardenWalls[i];

    if (rectsOverlap(x, y, gardenW, gardenH, r.x, r.y, r.w, r.h)) {
      return true;
    }
  }

  return false;
}

void checkFriendFound() {
  if (rectsOverlap(gardenX, gardenY, gardenW, gardenH, friendX, friendY, friendW, friendH)) {
    appState = FRIEND_FOUND;
    drawFriendFoundScreen();
  }
}

void drawGardenScene() {
  drawGardenStatic();
  drawFriend();
  drawGardenMadelyn();
}

void drawGardenFrame() {
  bool moved = oldGardenX != gardenX || oldGardenY != gardenY;

  if (!moved) {
    return;
  }

  int ux = min(oldGardenX, gardenX) - 2;
  int uy = min(oldGardenY, gardenY) - 2;
  int ux2 = max(oldGardenX + gardenW, gardenX + gardenW) + 2;
  int uy2 = max(oldGardenY + gardenH, gardenY + gardenH) + 2;

  redrawGardenPatch(ux, uy, ux2 - ux, uy2 - uy);

  if (rectsOverlap(ux, uy, ux2 - ux, uy2 - uy, friendX, friendY, friendW, friendH)) {
    drawFriend();
  }

  drawGardenMadelyn();
}

void drawGardenStatic() {
  tft.fillRect(0, HEADER_H, tft.width(), tft.height() - HEADER_H, C_GRASS);

  tft.fillRect(0, 0, tft.width(), HEADER_H, C_BLUE);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 5);
  tft.print("GARDEN MAZE");
  tft.setCursor(96, 5);
  tft.print("Find friend");

  tft.fillRect(8, 108, 36, 10, C_PATH);
  tft.fillRect(36, 82, 10, 36, C_PATH);
  tft.fillRect(36, 82, 52, 10, C_PATH);
  tft.fillRect(86, 50, 10, 42, C_PATH);
  tft.fillRect(96, 50, 46, 10, C_PATH);
  tft.fillRect(132, 28, 10, 32, C_PATH);

  tft.fillCircle(74, 103, 8, C_CYAN);
  tft.drawCircle(74, 103, 9, C_WHITE);
  tft.drawPixel(74, 103, C_WHITE);

  drawGardenFlowers();

  for (int i = 0; i < GARDEN_OBSTACLE_COUNT; i++) {
    drawGardenWall(i);
  }

  tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
}

void redrawGardenPatch(int x, int y, int w, int h) {
  if (x < 0) {
    w += x;
    x = 0;
  }

  if (y < HEADER_H) {
    h += (y - HEADER_H);
    y = HEADER_H;
  }

  if (x + w > tft.width()) {
    w = tft.width() - x;
  }

  if (y + h > tft.height()) {
    h = tft.height() - y;
  }

  if (w <= 0 || h <= 0) {
    return;
  }

  tft.fillRect(x, y, w, h, C_GRASS);

  redrawPathIfHit(x, y, w, h, 8, 108, 36, 10);
  redrawPathIfHit(x, y, w, h, 36, 82, 10, 36);
  redrawPathIfHit(x, y, w, h, 36, 82, 52, 10);
  redrawPathIfHit(x, y, w, h, 86, 50, 10, 42);
  redrawPathIfHit(x, y, w, h, 96, 50, 46, 10);
  redrawPathIfHit(x, y, w, h, 132, 28, 10, 32);

  if (rectsOverlap(x, y, w, h, 65, 94, 18, 18)) {
    tft.fillCircle(74, 103, 8, C_CYAN);
    tft.drawCircle(74, 103, 9, C_WHITE);
    tft.drawPixel(74, 103, C_WHITE);
  }

  drawGardenFlowers();

  for (int i = 0; i < GARDEN_OBSTACLE_COUNT; i++) {
    Rect r = gardenWalls[i];

    if (rectsOverlap(x, y, w, h, r.x, r.y, r.w, r.h)) {
      drawGardenWall(i);
    }
  }

  if (x <= 2 || y <= HEADER_H + 3 || x + w >= tft.width() - 2 || y + h >= tft.height() - 2) {
    tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
  }
}

void redrawPathIfHit(int patchX, int patchY, int patchW, int patchH, int x, int y, int w, int h) {
  if (rectsOverlap(patchX, patchY, patchW, patchH, x, y, w, h)) {
    tft.fillRect(x, y, w, h, C_PATH);
  }
}

void drawGardenWall(int i) {
  Rect r = gardenWalls[i];

  tft.fillRect(r.x, r.y, r.w, r.h, C_DARK_GREEN);
  tft.drawRect(r.x, r.y, r.w, r.h, C_GREEN);

  if (r.w > 12 && r.h > 6) {
    for (int x = r.x + 4; x < r.x + r.w - 2; x += 12) {
      tft.drawPixel(x, r.y + 3, C_GREEN);
    }
  }
}

void drawGardenFlowers() {
  tft.drawPixel(18, 58, C_FLOWER);
  tft.drawPixel(22, 58, C_YELLOW);
  tft.drawPixel(146, 112, C_FLOWER);
  tft.drawPixel(150, 112, C_YELLOW);
  tft.drawPixel(62, 30, C_FLOWER);
  tft.drawPixel(66, 30, C_YELLOW);
  tft.drawPixel(112, 104, C_FLOWER);
  tft.drawPixel(116, 104, C_YELLOW);
}

void drawGardenMadelyn() {
  drawTopDownMadelynSprite(gardenX, gardenY);
}

void drawFriend() {
  drawTopDownFriendSprite(friendX, friendY);
}


// ======================================================
// FRIEND FOUND SCREEN
// ======================================================

void drawFriendFoundScreen() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 22);
  tft.println("FRIEND");

  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 46);
  tft.println("FOUND!");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 82);
  tft.println("Next stop:");

  tft.setTextColor(C_PINK);
  tft.setCursor(8, 96);
  tft.println("the bridge!");

  tft.setTextColor(C_RED);
  tft.setCursor(8, 114);
  tft.println("Press A");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void updateFriendFound() {
  if (aJustPressed() || joyJustPressed()) {
    appState = BRIDGE_INTRO;
    drawBridgeIntro();
  }
}


// ======================================================
// LEVEL 3: BRIDGE INTRO + CROSSING
// ======================================================

void drawBridgeIntro() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 18);
  tft.println("THE");

  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 42);
  tft.println("BRIDGE");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 76);
  tft.println("The wind is strong.");
  tft.setCursor(8, 90);
  tft.println("Cross together!");

  tft.setTextColor(C_RED);
  tft.setCursor(8, 110);
  tft.println("Press A");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void updateBridgeIntro() {
  if (aJustPressed() || joyJustPressed()) {
    startBridgeCrossing();
  }
}

void startBridgeCrossing() {
  appState = BRIDGE_CROSSING;

  bridgeX = 8;
  bridgeY = 72;
  oldBridgeX = bridgeX;
  oldBridgeY = bridgeY;

  windCounter = 0;
  windPush = 0;

  tft.fillScreen(C_BLACK);
  drawBridgeScene();
}

void updateBridgeCrossing() {
  if (millis() - lastFrame < frameDelay) {
    return;
  }

  lastFrame = millis();

  oldBridgeX = bridgeX;
  oldBridgeY = bridgeY;

  updateBridgeWind();
  updateBridgeMovement();
  checkBridgeGoal();
  drawBridgeFrame();
}

void updateBridgeWind() {
  windCounter++;

  if (windCounter < 35) {
    windPush = 0;
  }
  else if (windCounter < 70) {
    windPush = 1;
  }
  else if (windCounter < 105) {
    windPush = 0;
  }
  else if (windCounter < 140) {
    windPush = -1;
  }
  else {
    windCounter = 0;
    windPush = 0;
  }
}

void updateBridgeMovement() {
  int nextX = bridgeX;
  int nextY = bridgeY;

  if (direction == "LEFT") {
    nextX -= bridgeSpeed;
  }
  else if (direction == "RIGHT") {
    nextX += bridgeSpeed;
  }
  else if (direction == "UP") {
    nextY -= bridgeSpeed;
  }
  else if (direction == "DOWN") {
    nextY += bridgeSpeed;
  }

  // gentle wind drift
  nextY += windPush;

  if (!bridgeHitsBlock(nextX, nextY)) {
    bridgeX = nextX;
    bridgeY = nextY;
  }
  else {
    if (!bridgeHitsBlock(nextX, bridgeY)) {
      bridgeX = nextX;
    }

    if (!bridgeHitsBlock(bridgeX, nextY)) {
      bridgeY = nextY;
    }
  }
}

bool bridgeHitsBlock(int x, int y) {
  // keep player on bridge deck
  if (x < 4 || x > 150 || y < 42 || y > 106) {
    return true;
  }

  for (int i = 0; i < BRIDGE_OBSTACLE_COUNT; i++) {
    Rect r = bridgeObstacles[i];

    if (rectsOverlap(x, y, bridgeW, bridgeH, r.x, r.y, r.w, r.h)) {
      return true;
    }
  }

  return false;
}

void checkBridgeGoal() {
  if (rectsOverlap(bridgeX, bridgeY, bridgeW, bridgeH, bridgeGoalX, bridgeGoalY, bridgeGoalW, bridgeGoalH)) {
    appState = BRIDGE_CLEAR;
    drawBridgeClearScreen();
  }
}

void drawBridgeScene() {
  drawBridgeStatic();
  drawBridgePair();
}

void drawBridgeFrame() {
  bool moved = oldBridgeX != bridgeX || oldBridgeY != bridgeY;

  if (!moved) {
    return;
  }

  int ux = min(oldBridgeX, bridgeX) - 12;
  int uy = min(oldBridgeY, bridgeY) - 6;
  int ux2 = max(oldBridgeX + bridgeW, bridgeX + bridgeW) + 18;
  int uy2 = max(oldBridgeY + bridgeH, bridgeY + bridgeH) + 6;

  redrawBridgePatch(ux, uy, ux2 - ux, uy2 - uy);
  drawBridgePair();
}

void drawBridgeStatic() {
  // river
  tft.fillRect(0, HEADER_H, tft.width(), tft.height() - HEADER_H, C_WATER);

  // header
  tft.fillRect(0, 0, tft.width(), HEADER_H, C_BLUE);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 5);
  tft.print("THE BRIDGE");

  tft.setCursor(96, 5);
  if (windPush > 0) {
    tft.print("Wind v");
  }
  else if (windPush < 0) {
    tft.print("Wind ^");
  }
  else {
    tft.print("Steady");
  }

  // bridge deck
  tft.fillRect(0, 42, 160, 68, C_BRIDGE);

  // rails
  tft.fillRect(0, 38, 160, 4, C_DARK_GRAY);
  tft.fillRect(0, 110, 160, 4, C_DARK_GRAY);

  for (int x = 6; x < 160; x += 16) {
    tft.drawLine(x, 38, x + 6, 42, C_WHITE);
    tft.drawLine(x, 110, x + 6, 114, C_WHITE);
  }

  // plank lines
  for (int x = 8; x < 160; x += 16) {
    tft.drawLine(x, 44, x, 108, C_BROWN);
  }

  // goal gate
  tft.fillRect(bridgeGoalX, bridgeGoalY, bridgeGoalW, bridgeGoalH, C_GOLD);
  tft.drawRect(bridgeGoalX, bridgeGoalY, bridgeGoalW, bridgeGoalH, C_WHITE);

  // obstacles
  drawBridgeObstacles();

  // small wind leaves
  drawBridgeLeaves();

  tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
}

void redrawBridgePatch(int x, int y, int w, int h) {
  if (x < 0) {
    w += x;
    x = 0;
  }

  if (y < HEADER_H) {
    h += (y - HEADER_H);
    y = HEADER_H;
  }

  if (x + w > tft.width()) {
    w = tft.width() - x;
  }

  if (y + h > tft.height()) {
    h = tft.height() - y;
  }

  if (w <= 0 || h <= 0) {
    return;
  }

  // base river
  tft.fillRect(x, y, w, h, C_WATER);

  // bridge deck if overlap
  if (rectsOverlap(x, y, w, h, 0, 42, 160, 68)) {
    tft.fillRect(x, max(y, 42), w, min(y + h, 110) - max(y, 42), C_BRIDGE);

    for (int px = 8; px < 160; px += 16) {
      if (px >= x && px <= x + w) {
        tft.drawLine(px, max(y, 44), px, min(y + h, 108), C_BROWN);
      }
    }
  }

  // rails if overlap
  if (rectsOverlap(x, y, w, h, 0, 38, 160, 4)) {
    tft.fillRect(0, 38, 160, 4, C_DARK_GRAY);
  }

  if (rectsOverlap(x, y, w, h, 0, 110, 160, 4)) {
    tft.fillRect(0, 110, 160, 4, C_DARK_GRAY);
  }

  // goal
  if (rectsOverlap(x, y, w, h, bridgeGoalX, bridgeGoalY, bridgeGoalW, bridgeGoalH)) {
    tft.fillRect(bridgeGoalX, bridgeGoalY, bridgeGoalW, bridgeGoalH, C_GOLD);
    tft.drawRect(bridgeGoalX, bridgeGoalY, bridgeGoalW, bridgeGoalH, C_WHITE);
  }

  // obstacles
  for (int i = 0; i < BRIDGE_OBSTACLE_COUNT; i++) {
    Rect r = bridgeObstacles[i];

    if (rectsOverlap(x, y, w, h, r.x, r.y, r.w, r.h)) {
      drawBridgeObstacle(i);
    }
  }

  drawBridgeLeaves();

  if (x <= 2 || y <= HEADER_H + 3 || x + w >= tft.width() - 2 || y + h >= tft.height() - 2) {
    tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
  }
}

void drawBridgeObstacles() {
  for (int i = 0; i < BRIDGE_OBSTACLE_COUNT; i++) {
    drawBridgeObstacle(i);
  }
}

void drawBridgeObstacle(int i) {
  Rect r = bridgeObstacles[i];

  if (i < 4) {
    // puddle
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, C_CYAN);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, C_WHITE);
  }
  else {
    // leaf pile
    tft.fillTriangle(r.x, r.y + r.h, r.x + r.w / 2, r.y, r.x + r.w, r.y + r.h, C_LEAF);
        tft.drawPixel(r.x + r.w / 2, r.y + 3, C_YELLOW);
  }
}


void drawBridgeLeaves() {
  tft.drawPixel(20, 34, C_YELLOW);
  tft.drawPixel(24, 35, C_LEAF);
  tft.drawPixel(54, 119, C_YELLOW);
  tft.drawPixel(58, 118, C_LEAF);
  tft.drawPixel(126, 33, C_YELLOW);
  tft.drawPixel(130, 34, C_LEAF);
}

void drawBridgePair() {
  drawBridgeMadelyn(bridgeX, bridgeY);
  drawBridgeFriend(bridgeX - 10, bridgeY + 2);
}

void drawBridgeMadelyn(int x, int y) {
  drawTopDownMadelynSprite(x, y);
}

void drawBridgeFriend(int x, int y) {
  drawTopDownFriendSprite(x, y);
}

void drawBridgeClearScreen() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 22);
  tft.println("BRIDGE");

  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 46);
  tft.println("CROSSED!");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 82);
  tft.println("Next stop:");

  tft.setTextColor(C_PINK);
  tft.setCursor(8, 96);
  tft.println("the city!");

  tft.setTextColor(C_RED);
  tft.setCursor(8, 114);
  tft.println("Press A");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void updateBridgeClear() {
  if (aJustPressed() || joyJustPressed()) {
    appState = CITY_INTRO;
    drawCityIntro();
  }
}
// ======================================================
// LEVEL 4: CITY FINALE
// ======================================================

void drawCityIntro() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 18);
  tft.println("THE");

  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 42);
  tft.println("CITY");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 76);
  tft.println("Almost there!");
  tft.setCursor(8, 90);
  tft.println("Find Wrigley!");

  tft.setTextColor(C_RED);
  tft.setCursor(8, 110);
  tft.println("Press A");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}

void updateCityIntro() {
  if (aJustPressed() || joyJustPressed()) {
    startCityWalk();
  }
}

void startCityWalk() {
  appState = CITY_WALK;

  cityX = 10;
  cityY = 92;
  oldCityX = cityX;
  oldCityY = cityY;

  tft.fillScreen(C_BLACK);
  drawCityScene();
}

void updateCityWalk() {
  if (millis() - lastFrame < frameDelay) {
    return;
  }

  lastFrame = millis();

  oldCityX = cityX;
  oldCityY = cityY;

  updateCityMovement();
  checkCityGoal();
  drawCityFrame();
}

void updateCityMovement() {
  int nextX = cityX;
  int nextY = cityY;

  if (direction == "LEFT") {
    nextX -= citySpeed;
  }
  else if (direction == "RIGHT") {
    nextX += citySpeed;
  }
  else if (direction == "UP") {
    nextY -= citySpeed;
  }
  else if (direction == "DOWN") {
    nextY += citySpeed;
  }

  if (!cityHitsWall(nextX, nextY)) {
    cityX = nextX;
    cityY = nextY;
  }
  else {
    if (!cityHitsWall(nextX, cityY)) {
      cityX = nextX;
    }

    if (!cityHitsWall(cityX, nextY)) {
      cityY = nextY;
    }
  }
}

bool cityHitsWall(int x, int y) {
  for (int i = 0; i < CITY_OBSTACLE_COUNT; i++) {
    Rect r = cityObstacles[i];

    if (rectsOverlap(x, y, cityW, cityH, r.x, r.y, r.w, r.h)) {
      return true;
    }
  }

  return false;
}

void checkCityGoal() {
  if (rectsOverlap(cityX, cityY, cityW, cityH, cityGoalX, cityGoalY, cityGoalW, cityGoalH)) {
    appState = CITY_CLEAR;
    drawCityClearScreen();
  }
}

void drawCityScene() {
  drawCityStatic();
  drawCityPair();
}

void drawCityStatic() {
  tft.fillRect(0, HEADER_H, tft.width(), tft.height() - HEADER_H, C_PATH);

  tft.fillRect(0, 0, tft.width(), HEADER_H, C_BLUE);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(4, 5);
  tft.print("CITY SQUARE");

  tft.setCursor(96, 5);
  tft.print("Find dog");

  // cobblestone hints
  for (int y = HEADER_H + 12; y < 124; y += 16) {
    tft.drawLine(4, y, 154, y, C_TAN);
  }

  for (int x = 12; x < 160; x += 24) {
    tft.drawLine(x, HEADER_H + 5, x, 123, C_TAN);
  }

  // cafe / shop
  tft.fillRect(30, 39, 34, 26, C_BROWN);
  tft.drawRect(30, 39, 34, 26, C_WHITE);
  tft.fillRect(34, 43, 26, 6, C_RED);
  tft.drawLine(34, 49, 60, 49, C_WHITE);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(36, 55);
  tft.print("CAFE");
  tft.drawPixel(33, 62, C_YELLOW);
  tft.drawPixel(60, 62, C_YELLOW);

  // flower cart
  tft.fillRect(78, 86, 22, 18, C_BROWN);
  tft.drawRect(78, 86, 22, 18, C_WHITE);
  tft.fillCircle(83, 88, 2, C_PINK);
  tft.fillCircle(89, 88, 2, C_YELLOW);
  tft.fillCircle(95, 88, 2, C_CYAN);
  tft.drawPixel(82, 105, C_BLACK);
  tft.drawPixel(96, 105, C_BLACK);

  // street lamp
  tft.drawLine(118, 38, 118, 68, C_DARK_BLUE);
  tft.fillCircle(118, 38, 4, C_YELLOW);
  tft.drawCircle(118, 38, 5, C_DARK_BLUE);
  tft.drawLine(112, 68, 124, 68, C_DARK_BLUE);

  // Wrigley the tiny golden doodle waits at the finish.
  drawWrigleyDog(cityGoalX, cityGoalY);

  // tiny sparkle near Wrigley
  tft.drawPixel(140, 60, C_PINK);
  tft.drawPixel(144, 58, C_YELLOW);
  tft.drawPixel(148, 62, C_CYAN);

  tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);
}


void drawCityFrame() {

  bool moved = oldCityX != cityX || oldCityY != cityY;

  if (!moved) {

    return;

  }

  int ux = min(oldCityX, cityX) - 14;

  int uy = min(oldCityY, cityY) - 6;

  int ux2 = max(oldCityX + cityW, cityX + cityW) + 20;

  int uy2 = max(oldCityY + cityH, cityY + cityH) + 6;

  redrawCityPatch(ux, uy, ux2 - ux, uy2 - uy);

  drawCityPair();

}



void redrawCityPatch(int x, int y, int w, int h) {

  if (x < 0) {

    w += x;

    x = 0;

  }

  if (y < HEADER_H) {

    h += (y - HEADER_H);

    y = HEADER_H;

  }

  if (x + w > tft.width()) {

    w = tft.width() - x;

  }

  if (y + h > tft.height()) {

    h = tft.height() - y;

  }

  if (w <= 0 || h <= 0) {

    return;

  }

  // Base plaza

  tft.fillRect(x, y, w, h, C_PATH);

  // Cobblestone hints inside patch

  for (int sy = HEADER_H + 12; sy < 124; sy += 16) {

    if (sy >= y && sy <= y + h) {

      tft.drawLine(x, sy, x + w, sy, C_TAN);

    }

  }

  for (int sx = 12; sx < 160; sx += 24) {

    if (sx >= x && sx <= x + w) {

      tft.drawLine(sx, y, sx, y + h, C_TAN);

    }

  }

  // Cafe / shop

  if (rectsOverlap(x, y, w, h, 34, 42, 28, 22)) {

    tft.fillRect(34, 42, 28, 22, C_BROWN);

    tft.drawRect(34, 42, 28, 22, C_WHITE);

    tft.fillRect(38, 46, 20, 6, C_RED);

    tft.setTextSize(1);

    tft.setTextColor(C_WHITE);

    tft.setCursor(38, 55);

    tft.print("CAFE");

  }

  // Flower cart

  if (rectsOverlap(x, y, w, h, 78, 86, 22, 20)) {

    tft.fillRect(78, 86, 22, 18, C_BROWN);

    tft.drawRect(78, 86, 22, 18, C_WHITE);

    tft.fillCircle(83, 88, 2, C_PINK);

    tft.fillCircle(89, 88, 2, C_YELLOW);

    tft.fillCircle(95, 88, 2, C_CYAN);

    tft.drawPixel(82, 105, C_BLACK);

    tft.drawPixel(96, 105, C_BLACK);

  }

  // Street lamp

  if (rectsOverlap(x, y, w, h, 112, 33, 14, 37)) {

    tft.drawLine(118, 38, 118, 68, C_DARK_BLUE);

    tft.fillCircle(118, 38, 4, C_YELLOW);

    tft.drawCircle(118, 38, 5, C_DARK_BLUE);

    tft.drawLine(112, 68, 124, 68, C_DARK_BLUE);

  }

  // Wrigley / finish target

  if (rectsOverlap(x, y, w, h, cityGoalX - 4, cityGoalY, cityGoalW + 8, cityGoalH)) {

    drawWrigleyDog(cityGoalX, cityGoalY);

  }

  // Sparkles near square

  if (rectsOverlap(x, y, w, h, 138, 56, 14, 10)) {

    tft.drawPixel(140, 60, C_PINK);

    tft.drawPixel(144, 58, C_YELLOW);

    tft.drawPixel(148, 62, C_CYAN);

  }

  // Border restore

  if (x <= 2 || y <= HEADER_H + 3 || x + w >= tft.width() - 2 || y + h >= tft.height() - 2) {

    tft.drawRect(0, HEADER_H + 1, tft.width(), tft.height() - HEADER_H - 1, C_WHITE);

  }

}

void drawCityPair() {
  drawCityMadelyn(cityX, cityY);
  drawCityFriend(cityX - 10, cityY + 2);
}

void drawCityMadelyn(int x, int y) {
  drawTopDownMadelynSprite(x, y);
}

void drawCityFriend(int x, int y) {
  drawTopDownFriendSprite(x, y);
}

void drawWrigleyDog(int x, int y) {
  // Tiny golden doodle: curly golden body, floppy ears, wagging tail.
  // x/y is the top-left of the finish target collision box.
  int bx = x + 1;
  int by = y + 6;

  // curly body
  tft.fillCircle(bx + 6,  by + 8, 5, C_GOLD);
  tft.fillCircle(bx + 11, by + 7, 5, C_GOLD);
  tft.fillCircle(bx + 8,  by + 11, 4, C_TAN);
  tft.drawPixel(bx + 5,  by + 5, C_WHITE);
  tft.drawPixel(bx + 12, by + 5, C_WHITE);
  tft.drawPixel(bx + 9,  by + 11, C_WHITE);

  // head, muzzle, floppy ear
  tft.fillCircle(bx + 13, by + 2, 5, C_GOLD);
  tft.fillCircle(bx + 15, by + 4, 3, C_TAN);
  tft.fillCircle(bx + 9,  by + 3, 3, C_BROWN);
  tft.drawPixel(bx + 8, by + 6, C_BROWN);

  // face
  tft.drawPixel(bx + 15, by + 1, C_BLACK);
  tft.drawPixel(bx + 18, by + 4, C_BLACK);

  // little red collar
  tft.drawPixel(bx + 12, by + 6, C_RED);
  tft.drawPixel(bx + 13, by + 6, C_RED);

  // wagging tail
  tft.drawLine(bx + 2, by + 8, bx - 2, by + 4, C_GOLD);
  tft.drawPixel(bx - 3, by + 3, C_GOLD);

  // paws
  tft.drawPixel(bx + 5,  by + 14, C_BROWN);
  tft.drawPixel(bx + 11, by + 14, C_BROWN);
  tft.drawPixel(bx + 14, by + 14, C_BROWN);
}

void drawCityClearScreen() {
  tft.fillScreen(C_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(8, 14);
  tft.println("WRIGLEY");

  tft.setTextColor(C_CYAN);
  tft.setCursor(8, 38);
  tft.println("FOUND!");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 76);
  tft.println("A perfect");
  tft.setCursor(8, 90);
  tft.println("Paris day.");

  tft.setTextColor(C_PINK);
  tft.setCursor(8, 108);
  tft.println("THE END");

  tft.setTextColor(C_RED);
  tft.setCursor(96, 116);
  tft.println("A=Menu");

  tft.drawRect(0, 0, tft.width(), tft.height(), C_WHITE);
}


void updateCityClear() {
  if (aJustPressed() || joyJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
  }
}

bool rectsOverlap(int ax, int ay, int aw, int ah,
                  int bx, int by, int bw, int bh) {
  return ax < bx + bw && ax + aw > bx &&
         ay < by + bh && ay + ah > by;
}  

// ======================================================
// MINI GAME: BM TRON
// ======================================================

void startTronGame() {
  appState = TRON_GAME;
  resetTronRound();
  drawTronScene();
}

void resetTronRound() {
  for (int x = 0; x < MINI_GRID_W; x++) {
    for (int y = 0; y < MINI_GRID_H; y++) {
      tronTrail[x][y] = false;
    }
  }

  tronX = 7;
  tronY = 14;
  tronDX = 1;
  tronDY = 0;
  tronScore = 0;
  tronGameOver = false;
  tronLastStep = millis();

  for (int x = 0; x < MINI_GRID_W; x++) {
    tronTrail[x][0] = true;
    tronTrail[x][MINI_GRID_H - 1] = true;
  }

  for (int y = 0; y < MINI_GRID_H; y++) {
    tronTrail[0][y] = true;
    tronTrail[MINI_GRID_W - 1][y] = true;
  }
}

void updateTronGame() {
  if (joyJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
    return;
  }

  if (tronGameOver) {
    if (aJustPressed()) {
      resetTronRound();
      drawTronScene();
    }
    return;
  }

  updateTronDirection();

  if (millis() - tronLastStep < tronStepDelay) {
    return;
  }
  tronLastStep = millis();

  tronTrail[tronX][tronY] = true;
  drawTronCell(tronX, tronY, C_CYAN);

  tronX += tronDX;
  tronY += tronDY;

  if (tronCellBlocked(tronX, tronY)) {
    tronGameOver = true;

    tft.fillRect(20, 48, 120, 34, C_BLACK);
    tft.drawRect(20, 48, 120, 34, C_RED);

    tft.setTextSize(1);
    tft.setTextColor(C_RED);
    tft.setCursor(40, 56);
    tft.print("TRON CRASH");

    tft.setTextColor(C_WHITE);
    tft.setCursor(34, 70);
    tft.print("A=again  Press=menu");
    return;
  }

  tronScore++;
  drawTronCell(tronX, tronY, C_YELLOW);

  if (tronScore % 6 == 0) {
    drawTronHUD();
  }
}

void updateTronDirection() {
  int ndx = tronDX;
  int ndy = tronDY;

  if (directionJustPressed("LEFT")) {
    ndx = -1;
    ndy = 0;
  }
  else if (directionJustPressed("RIGHT")) {
    ndx = 1;
    ndy = 0;
  }
  else if (directionJustPressed("UP")) {
    ndx = 0;
    ndy = -1;
  }
  else if (directionJustPressed("DOWN")) {
    ndx = 0;
    ndy = 1;
  }

  if (!(ndx == -tronDX && ndy == -tronDY)) {
    tronDX = ndx;
    tronDY = ndy;
  }
}

bool tronCellBlocked(int gx, int gy) {
  if (gx < 0 || gx >= MINI_GRID_W || gy < 0 || gy >= MINI_GRID_H) {
    return true;
  }

  return tronTrail[gx][gy];
}

void drawTronScene() {
  tft.fillScreen(C_BLACK);
  tft.drawRect(0, MINI_TOP, tft.width(), MINI_GRID_H * MINI_CELL, C_WHITE);
  drawTronHUD();

  for (int x = 0; x < MINI_GRID_W; x++) {
    drawTronCell(x, 0, C_DARK_BLUE);
    drawTronCell(x, MINI_GRID_H - 1, C_DARK_BLUE);
  }

  for (int y = 0; y < MINI_GRID_H; y++) {
    drawTronCell(0, y, C_DARK_BLUE);
    drawTronCell(MINI_GRID_W - 1, y, C_DARK_BLUE);
  }

  drawTronCell(tronX, tronY, C_YELLOW);
}

void drawTronHUD() {
  tft.fillRect(0, 0, tft.width(), MINI_TOP, C_BLACK);
  tft.setTextSize(1);

  tft.setTextColor(C_CYAN);
  tft.setCursor(2, 2);
  tft.print("BM TRON");

  tft.setTextColor(C_WHITE);
  tft.setCursor(88, 2);
  tft.print("S:");
  tft.print(tronScore);
}

void drawTronCell(int gx, int gy, uint16_t color) {
  tft.fillRect(gx * MINI_CELL, MINI_TOP + gy * MINI_CELL, MINI_CELL, MINI_CELL, color);
}


// ======================================================
// MINI GAME: SNAKE
// ======================================================

void startSnakeGame() {
  appState = SNAKE_GAME;
  resetSnakeRound();
  drawSnakeScene();
}

void resetSnakeRound() {
  snakeLen = 5;
  snakeDX = 1;
  snakeDY = 0;
  snakeScore = 0;
  snakeGameOver = false;
  snakeLastStep = millis();

  for (int i = 0; i < snakeLen; i++) {
    snakeX[i] = 10 - i;
    snakeY[i] = 14;
  }

  placeSnakeFood();
}

void updateSnakeGame() {
  if (joyJustPressed()) {
    appState = MAIN_MENU;
    drawMainMenu();
    return;
  }

  if (snakeGameOver) {
    if (aJustPressed()) {
      resetSnakeRound();
      drawSnakeScene();
    }
    return;
  }

  updateSnakeDirection();

  if (millis() - snakeLastStep < snakeStepDelay) {
    return;
  }
  snakeLastStep = millis();

  int oldTailX = snakeX[snakeLen - 1];
  int oldTailY = snakeY[snakeLen - 1];

  for (int i = snakeLen - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  snakeX[0] += snakeDX;
  snakeY[0] += snakeDY;

  bool hitWall = snakeX[0] <= 0 || snakeX[0] >= MINI_GRID_W - 1 ||
                 snakeY[0] <= 0 || snakeY[0] >= MINI_GRID_H - 1;

  if (hitWall || snakeHitsBody(snakeX[0], snakeY[0])) {
    snakeGameOver = true;

    tft.fillRect(22, 48, 116, 34, C_BLACK);
    tft.drawRect(22, 48, 116, 34, C_RED);

    tft.setTextSize(1);
    tft.setTextColor(C_GREEN);
    tft.setCursor(48, 56);
    tft.print("SNAKE!");

    tft.setTextColor(C_WHITE);
    tft.setCursor(34, 70);
    tft.print("A=again  Press=menu");
    return;
  }

  bool ate = snakeX[0] == snakeFoodX && snakeY[0] == snakeFoodY;

  if (ate) {
    if (snakeLen < 175) {
      snakeX[snakeLen] = oldTailX;
      snakeY[snakeLen] = oldTailY;
      snakeLen++;
    }

    snakeScore++;
    placeSnakeFood();
    drawSnakeCell(snakeFoodX, snakeFoodY, C_RED);
    drawSnakeHUD();
  }
  else {
    drawSnakeCell(oldTailX, oldTailY, C_BLACK);
  }

  drawSnakeCell(snakeX[0], snakeY[0], C_YELLOW);

  if (snakeLen > 1) {
    drawSnakeCell(snakeX[1], snakeY[1], C_GREEN);
  }
}

void updateSnakeDirection() {
  int ndx = snakeDX;
  int ndy = snakeDY;

  if (directionJustPressed("LEFT")) {
    ndx = -1;
    ndy = 0;
  }
  else if (directionJustPressed("RIGHT")) {
    ndx = 1;
    ndy = 0;
  }
  else if (directionJustPressed("UP")) {
    ndx = 0;
    ndy = -1;
  }
  else if (directionJustPressed("DOWN")) {
    ndx = 0;
    ndy = 1;
  }

  if (!(ndx == -snakeDX && ndy == -snakeDY)) {
    snakeDX = ndx;
    snakeDY = ndy;
  }
}

bool snakeHitsBody(int gx, int gy) {
  for (int i = 1; i < snakeLen; i++) {
    if (snakeX[i] == gx && snakeY[i] == gy) {
      return true;
    }
  }

  return false;
}

void placeSnakeFood() {
  int seed = (int)(millis() % 997);

  for (int tries = 0; tries < 60; tries++) {
    int fx = 2 + ((seed + tries * 7) % (MINI_GRID_W - 4));
    int fy = 2 + ((seed / 3 + tries * 11) % (MINI_GRID_H - 4));

    bool onSnake = false;

    for (int i = 0; i < snakeLen; i++) {
      if (snakeX[i] == fx && snakeY[i] == fy) {
        onSnake = true;
      }
    }

    if (!onSnake) {
      snakeFoodX = fx;
      snakeFoodY = fy;
      return;
    }
  }

  snakeFoodX = 30;
  snakeFoodY = 10;
}

void drawSnakeScene() {
  tft.fillScreen(C_BLACK);
  tft.drawRect(0, MINI_TOP, tft.width(), MINI_GRID_H * MINI_CELL, C_WHITE);
  drawSnakeHUD();

  for (int x = 0; x < MINI_GRID_W; x++) {
    drawSnakeCell(x, 0, C_DARK_GREEN);
    drawSnakeCell(x, MINI_GRID_H - 1, C_DARK_GREEN);
  }

  for (int y = 0; y < MINI_GRID_H; y++) {
    drawSnakeCell(0, y, C_DARK_GREEN);
    drawSnakeCell(MINI_GRID_W - 1, y, C_DARK_GREEN);
  }

  drawSnakeCell(snakeFoodX, snakeFoodY, C_RED);

  for (int i = snakeLen - 1; i >= 0; i--) {
    drawSnakeCell(snakeX[i], snakeY[i], i == 0 ? C_YELLOW : C_GREEN);
  }
}

void drawSnakeHUD() {
  tft.fillRect(0, 0, tft.width(), MINI_TOP, C_BLACK);
  tft.setTextSize(1);

  tft.setTextColor(C_GREEN);
  tft.setCursor(2, 2);
  tft.print("SNAKE");

  tft.setTextColor(C_WHITE);
  tft.setCursor(88, 2);
  tft.print("S:");
  tft.print(snakeScore);
}

void drawSnakeCell(int gx, int gy, uint16_t color) {
  tft.fillRect(gx * MINI_CELL, MINI_TOP + gy * MINI_CELL, MINI_CELL, MINI_CELL, color);
}


