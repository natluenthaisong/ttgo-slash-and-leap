// Slash & Leap — vertical ninja runner for the LilyGO TTGO T-Display (ESP32, 135x240)
//
// Hold the board portrait (USB at the bottom). The ninja runs UP the mountain path;
// obstacles scroll down toward you. Strict pairing:
//   LEFT button  (next to USB) = JUMP  -> clears spike strips and rolling rocks
//   RIGHT button                = SLASH -> cuts bamboo and demons
// 3 hearts, combo multiplier, coins (every 10th heals), day-night cycle.
// High score persists in flash. Hold BOTH buttons on the title screen to reset it.
//
// Serial (115200): 'S' dumps the framebuffer as raw RGB565, 'J' = jump, 'K' = slash.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <BleKeyboard.h>
#include <esp_system.h>

TFT_eSPI tft;
TFT_eSprite spr(&tft);
Preferences prefs;

// ---------- screen layout (portrait) ----------
constexpr int SCR_W    = 135;
constexpr int SCR_H    = 240;
constexpr int SKY_H    = 30;          // horizon band at the top (HUD sits over it)
constexpr int PATH_L   = 30;
constexpr int PATH_R   = 105;
constexpr int PATH_W   = PATH_R - PATH_L;
constexpr int NINJA_X  = 67;
constexpr int NINJA_Y  = 185;         // feet line
constexpr int FRAME_MS = 20;          // ~50 fps

// ---------- buttons (both active-low) ----------
// Swap these two pin numbers if jump/slash feel reversed in your hand.
constexpr int PIN_JUMP  = 0;   // BOOT button — left of the USB port in portrait
constexpr int PIN_SLASH = 35;  // input-only pin, board provides the pull-up

struct Button {
  explicit Button(int p) : pin(p) {}
  int pin;
  bool lastHigh = true;
  bool clicked  = false;
  bool down     = false;
  void begin(bool internalPullup) { pinMode(pin, internalPullup ? INPUT_PULLUP : INPUT); }
  void poll() {
    bool high = digitalRead(pin);
    clicked  = (lastHigh && !high);
    down     = !high;
    lastHigh = high;
  }
};
Button btnJump{PIN_JUMP}, btnSlash{PIN_SLASH};
bool serialJump = false, serialSlash = false;

// ---------- colors ----------
constexpr uint16_t C565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t COL_OUTLINE  = C565(40, 36, 30);
constexpr uint16_t COL_NAVY     = C565(46, 52, 88);
constexpr uint16_t COL_NAVY_DK  = C565(30, 34, 60);
constexpr uint16_t COL_SKIN     = C565(244, 200, 160);
constexpr uint16_t COL_RED      = C565(216, 56, 48);
constexpr uint16_t COL_BAMBOO   = C565(110, 186, 70);
constexpr uint16_t COL_BAMBOO_D = C565(70, 130, 40);
constexpr uint16_t COL_DEMON    = C565(200, 60, 50);
constexpr uint16_t COL_DEMON_D  = C565(140, 34, 30);
constexpr uint16_t COL_SPIKE    = C565(176, 180, 190);
constexpr uint16_t COL_SPIKE_D  = C565(110, 114, 126);
constexpr uint16_t COL_ROCK     = C565(150, 118, 84);
constexpr uint16_t COL_ROCK_D   = C565(104, 78, 52);
constexpr uint16_t COL_COIN     = C565(255, 210, 60);
constexpr uint16_t COL_COIN_D   = C565(190, 140, 20);
constexpr uint16_t COL_HEART    = C565(235, 60, 70);
constexpr uint16_t COL_HEART_E  = C565(120, 110, 110);
constexpr uint16_t COL_PANEL    = C565(232, 226, 160);
constexpr uint16_t COL_TEXT_DK  = C565(84, 66, 28);

// day-night keyframes: day, dusk, night, dawn
struct RGB { uint8_t r, g, b; };
const RGB KF_SKY_T[4] = {{120,190,235},{250,150, 90},{ 18, 24, 60},{170,160,220}};
const RGB KF_SKY_B[4] = {{200,235,245},{255,205,130},{ 40, 50, 95},{255,205,170}};
const RGB KF_GRASS[4] = {{110,185, 90},{130,150, 70},{ 38, 72, 62},{ 95,160, 95}};
const RGB KF_PATH [4] = {{225,205,150},{215,180,130},{112,108,112},{205,190,150}};

// current environment colors, recomputed each frame from distance
uint16_t envSkyT, envSkyB, envGrass, envGrassDk, envPath, envPathEdge, envShadow;
float nightness = 0;   // 0 day .. 1 deep night

// ---------- obstacles / coins / particles ----------
enum ObType { OB_BAMBOO = 0, OB_DEMON, OB_SPIKES, OB_ROCK };
inline bool slashable(int type) { return type <= OB_DEMON; }

struct Obstacle {
  bool  active = false;
  int   type = 0;
  float x = 0, y = 0;
  bool  cleared = false;   // jumped over / already resolved
  uint8_t seed = 0;
};
constexpr int N_OBS = 6;
Obstacle obs[N_OBS];

struct Coin {
  bool  active = false;
  bool  air = false;       // only collectible while airborne
  float x = 0, y = 0;
};
constexpr int N_COINS = 4;
Coin coins[N_COINS];

struct Particle {
  bool active = false;
  float x, y, vx, vy;
  int life;
  uint16_t col;
};
constexpr int N_PARTS = 14;
Particle parts[N_PARTS];

void spawnParticles(float x, float y, uint16_t col, int n) {
  for (int i = 0; i < N_PARTS && n > 0; i++) {
    if (parts[i].active) continue;
    parts[i].active = true;
    parts[i].x = x; parts[i].y = y;
    parts[i].vx = (random(-20, 21)) / 10.0f;
    parts[i].vy = (random(-24, 5)) / 10.0f;
    parts[i].life = 14 + random(0, 10);
    parts[i].col = col;
    n--;
  }
}

// ---------- game state ----------
enum State { ST_TITLE, ST_PLAYING, ST_DYING, ST_OVER, ST_PRESENTER };
State state = ST_TITLE;

// ---------- presenter (BLE slide clicker) mode ----------
BleKeyboard bleKeyboard("Slash & Leap Remote", "LilyGO", 100);
bool     bleStarted = false;
int      slashHoldFrames = 0;   // hold RIGHT on title to enter presenter
int      presExitHold = 0;      // hold BOTH in presenter to exit
int      slideNum = 1;
uint32_t timerStartMs = 0;      // talk timer, starts on first "next"

float    dist = 0;          // world distance scrolled (px)
float    speedNow = 2.0f;
int      hearts = 3;
uint32_t score = 0, hiScore = 0;
bool     newHi = false;
int      combo = 0;
int      coinCount = 0;     // coins this run (every 10th heals)
int      jumpT = 0;         // 0 = grounded, 1..29 airborne
int      slashT = 0;        // >0 while slash anim runs (10..0)
int      iFrames = 0;       // invincibility after a hit
int      spawnTimer = 60;
uint32_t t = 0;
uint32_t stateT = 0;
int      flashFrames = 0;
uint32_t bothHoldFrames = 0;
uint32_t toastUntil = 0;

void setState(State s) { state = s; stateT = 0; }

inline int multNow() { int m = 1 + (combo < 35 ? combo : 35) / 5; return m; }

float jumpZ() { return jumpT > 0 ? 16.0f * sinf(PI * jumpT / 30.0f) : 0.0f; }

void resetRun() {
  dist = 0; speedNow = 2.0f;
  hearts = 3; score = 0; combo = 0; coinCount = 0;
  jumpT = 0; slashT = 0; iFrames = 0;
  spawnTimer = 70;
  newHi = false;
  for (int i = 0; i < N_OBS; i++) obs[i].active = false;
  for (int i = 0; i < N_COINS; i++) coins[i].active = false;
  for (int i = 0; i < N_PARTS; i++) parts[i].active = false;
}

// ---------- environment colors ----------
uint8_t lerp8(uint8_t a, uint8_t b, float f) { return (uint8_t)(a + (b - a) * f); }
uint16_t lerpRGB(const RGB *kf, float ph) {
  int i = (int)ph;
  float f = ph - i;
  const RGB &a = kf[i & 3], &b = kf[(i + 1) & 3];
  return C565(lerp8(a.r, b.r, f), lerp8(a.g, b.g, f), lerp8(a.b, b.b, f));
}
uint16_t scale565(uint16_t c, float k) {
  uint8_t r = ((c >> 11) << 3), g = (((c >> 5) & 0x3F) << 2), b = ((c & 0x1F) << 3);
  return C565((uint8_t)(r * k), (uint8_t)(g * k), (uint8_t)(b * k));
}

void updateEnv() {
  float ph = fmodf(dist / 2400.0f, 4.0f);       // full day cycle ~ every 9600 px
  envSkyT     = lerpRGB(KF_SKY_T, ph);
  envSkyB     = lerpRGB(KF_SKY_B, ph);
  envGrass    = lerpRGB(KF_GRASS, ph);
  envPath     = lerpRGB(KF_PATH,  ph);
  envGrassDk  = scale565(envGrass, 0.72f);
  envPathEdge = scale565(envPath, 0.70f);
  envShadow   = scale565(envPath, 0.62f);
  float d = fabsf(ph - 2.0f);                    // 0 at deep night
  nightness = d > 1.0f ? 0.0f : 1.0f - d;
}

// ---------- gameplay ----------
void enterPresenter() {
  if (!bleStarted) {           // BLE stays off in game mode; started once on demand
    bleKeyboard.begin();
    bleStarted = true;
  }
  slideNum = 1;
  timerStartMs = 0;
  presExitHold = 0;
  setState(ST_PRESENTER);
}

void hitNinja(float ox, float oy) {
  spawnParticles(ox, oy, COL_RED, 6);
  hearts--;
  combo = 0;
  iFrames = 70;
  flashFrames = 3;
  if (hearts <= 0) setState(ST_DYING);
}

void spawnObstacle() {
  for (int i = 0; i < N_OBS; i++) {
    if (obs[i].active) continue;
    Obstacle &o = obs[i];
    o.active = true;
    o.cleared = false;
    o.seed = (uint8_t)random(0, 255);
    int r = random(0, 100);
    o.type = r < 30 ? OB_BAMBOO : r < 55 ? OB_DEMON : r < 80 ? OB_SPIKES : OB_ROCK;
    o.y = -30;
    o.x = (o.type == OB_DEMON || o.type == OB_ROCK) ? NINJA_X + random(-10, 11) : NINJA_X;

    // sometimes drop a coin midway to the next obstacle
    if (random(0, 100) < 55) {
      for (int c = 0; c < N_COINS; c++) {
        if (coins[c].active) continue;
        coins[c].active = true;
        coins[c].air = random(0, 100) < 40;
        coins[c].x = NINJA_X + random(-8, 9);
        coins[c].y = -30 - (spawnTimer * speedNow) * 0.5f;
        break;
      }
    }
    return;
  }
}

void update() {
  bool anyClick   = btnJump.clicked || btnSlash.clicked || serialJump || serialSlash;
  bool jumpPress  = btnJump.clicked || serialJump;
  bool slashPress = btnSlash.clicked || serialSlash;
  serialJump = serialSlash = false;

  switch (state) {
    case ST_TITLE: {
      dist += 1.2f;
      if (btnJump.down && btnSlash.down) {
        if (++bothHoldFrames == 75) {
          hiScore = 0;
          prefs.putUInt("hi", 0);
          toastUntil = t + 90;
        }
        slashHoldFrames = -40;   // lockout: releasing after a both-hold shouldn't start a run
      } else {
        bothHoldFrames = 0;
        bool startRun = jumpPress || (slashPress && !btnSlash.down);   // serial 'K' starts instantly
        if (btnSlash.down) {
          // RIGHT is start-on-release so a long hold can open presenter mode instead
          if (++slashHoldFrames == 75) {
            slashHoldFrames = 0;
            enterPresenter();
            break;
          }
        } else {
          if (slashHoldFrames > 0) startRun = true;   // short tap released
          slashHoldFrames = 0;
        }
        if (startRun) {
          resetRun();
          setState(ST_PLAYING);
        }
      }
      break;
    }

    case ST_PRESENTER: {
      if (btnJump.down && btnSlash.down) {
        if (++presExitHold >= 75) ESP.restart();     // clean way back to game mode
      } else {
        presExitHold = 0;
        if (bleKeyboard.isConnected()) {
          if (jumpPress) {
            bleKeyboard.write(KEY_LEFT_ARROW);
            if (slideNum > 1) slideNum--;
          }
          if (slashPress) {
            bleKeyboard.write(KEY_RIGHT_ARROW);
            slideNum++;
            if (timerStartMs == 0) timerStartMs = millis();
          }
        }
      }
      break;
    }

    case ST_PLAYING: {
      speedNow = 2.0f + (dist * 0.00018f > 1.2f ? 1.2f : dist * 0.00018f);
      dist += speedNow;

      if (jumpPress && jumpT == 0) jumpT = 1;
      if (jumpT > 0) { jumpT++; if (jumpT >= 30) { jumpT = 0; spawnParticles(NINJA_X, NINJA_Y + 2, envPathEdge, 3); } }
      if (slashPress && slashT == 0) slashT = 10;
      if (slashT > 0) slashT--;
      if (iFrames > 0) iFrames--;

      if (--spawnTimer <= 0) {
        spawnObstacle();
        spawnTimer = (int)((60 + random(0, 40)) * (2.0f / speedNow));
      }

      float z = jumpZ();
      bool slashActive = slashT >= 5;   // first frames of the swing

      for (int i = 0; i < N_OBS; i++) {
        if (!obs[i].active) continue;
        Obstacle &o = obs[i];
        o.y += speedNow + (o.type == OB_DEMON ? 0.6f : 0.0f);

        // slash resolves slashables ahead of the ninja
        if (slashActive && slashable(o.type) && !o.cleared &&
            o.y > NINJA_Y - 46 && o.y < NINJA_Y - 4) {
          o.active = false;
          combo++;
          score += 10 * multNow();
          spawnParticles(o.x, o.y, o.type == OB_BAMBOO ? COL_BAMBOO : COL_DEMON, 8);
          continue;
        }

        // collision zone at the ninja's feet
        if (!o.cleared && o.y > NINJA_Y - 8 && o.y < NINJA_Y + 4) {
          bool safe = !slashable(o.type) && z > 5.0f;   // airborne over a jumpable
          if (safe) {
            o.cleared = true;
          } else if (iFrames == 0) {
            o.active = false;
            hitNinja(o.x, o.y - 6);
            continue;
          }
        }
        // score a clean jump once the obstacle has passed underneath
        if (o.cleared && o.y >= NINJA_Y + 4) {
          o.active = false;
          combo++;
          score += 10 * multNow();
        }
        if (o.y > SCR_H + 20) o.active = false;
      }

      for (int i = 0; i < N_COINS; i++) {
        if (!coins[i].active) continue;
        Coin &c = coins[i];
        c.y += speedNow;
        bool inReach = fabsf(c.y - NINJA_Y) < 10 && fabsf(c.x - NINJA_X) < 13;
        bool heightOk = c.air ? (z > 7.0f) : (z < 4.0f);
        if (inReach && heightOk) {
          c.active = false;
          coinCount++;
          score += 5 * multNow();
          spawnParticles(c.x, c.y, COL_COIN, 5);
          if (coinCount % 10 == 0) {
            if (hearts < 3) { hearts++; spawnParticles(NINJA_X, NINJA_Y - 20, COL_HEART, 8); }
            else score += 50;
          }
        }
        if (c.y > SCR_H + 10) c.active = false;
      }
      break;
    }

    case ST_DYING: {
      if (stateT > 40) {
        if (score > hiScore) {
          hiScore = score;
          newHi = true;
          prefs.putUInt("hi", hiScore);
        }
        setState(ST_OVER);
      }
      break;
    }

    case ST_OVER: {
      if (stateT > 30 && anyClick) {
        resetRun();
        setState(ST_PLAYING);
      }
      break;
    }
  }

  for (int i = 0; i < N_PARTS; i++) {
    if (!parts[i].active) continue;
    parts[i].x += parts[i].vx;
    parts[i].y += parts[i].vy;
    parts[i].vy += 0.12f;
    if (--parts[i].life <= 0) parts[i].active = false;
  }

  t++;
  stateT++;
}

// ---------- drawing ----------
void drawHeart(int x, int y, bool filled) {
  uint16_t c = filled ? COL_HEART : COL_HEART_E;
  spr.fillCircle(x + 2, y + 2, 2, c);
  spr.fillCircle(x + 6, y + 2, 2, c);
  spr.fillTriangle(x, y + 3, x + 8, y + 3, x + 4, y + 9, c);
  if (!filled) {
    spr.fillCircle(x + 2, y + 2, 1, envSkyT);
    spr.fillCircle(x + 6, y + 2, 1, envSkyT);
    spr.fillTriangle(x + 2, y + 4, x + 6, y + 4, x + 4, y + 7, envSkyT);
  }
}

void drawCoinIcon(int x, int y, int r, bool airRing) {
  if (airRing) spr.drawCircle(x, y, r + 3, TFT_WHITE);
  spr.fillCircle(x, y, r, COL_COIN);
  spr.drawCircle(x, y, r, COL_COIN_D);
  spr.drawPixel(x - 1, y - 1, TFT_WHITE);
}

void drawNinja(int x, int yFeet, float z, bool dead) {
  int y = yFeet - (int)z;

  // drop shadow stays on the ground
  int sw = 10 - (int)(z / 3);
  if (sw < 4) sw = 4;
  spr.fillEllipse(x, yFeet + 2, sw, 3, envShadow);

  if (iFrames > 0 && (t / 3) % 2 == 0 && !dead) return;   // blink while invincible

  int leg = ((int)(dist / 6) % 2 == 0) ? 1 : -1;
  if (z > 2) leg = 0;

  // legs
  spr.fillRect(x - 5, y - 5 + (leg == 1 ? 1 : 0), 4, 6, COL_NAVY_DK);
  spr.fillRect(x + 1, y - 5 + (leg == -1 ? 1 : 0), 4, 6, COL_NAVY_DK);
  // body
  spr.fillRoundRect(x - 6, y - 16, 12, 12, 3, COL_NAVY);
  spr.drawRoundRect(x - 6, y - 16, 12, 12, 3, COL_OUTLINE);
  // red belt/scarf trailing downward as we run up
  int flick = (t / 4) % 2 == 0 ? 1 : -1;
  spr.fillRect(x - 1 + flick, y - 6, 3, 6, COL_RED);
  // sword on the back (hidden mid-swing)
  if (slashT == 0) spr.drawLine(x - 7, y - 20, x + 2, y - 11, TFT_WHITE);
  // head + headband (seen from behind)
  spr.fillCircle(x, y - 20, 5, COL_NAVY);
  spr.drawCircle(x, y - 20, 5, COL_OUTLINE);
  spr.fillRect(x - 5, y - 22, 10, 3, COL_RED);
  spr.fillCircle(x, y - 24, 1, COL_SKIN);      // top knot

  if (dead) {
    spr.fillEllipse(x, y - 20, 3, 2, TFT_WHITE);  // dizzy swirl
  }

  // slash crescent sweeping the zone ahead (above)
  if (slashT >= 4) {
    spr.fillEllipse(x, y - 30, 16, 9, TFT_WHITE);
    spr.fillEllipse(x, y - 26, 16, 9, envPath);
    spr.drawLine(x - 10, y - 34, x + 12, y - 30, COL_SPIKE);
  }
}

void drawObstacle(const Obstacle &o) {
  int x = (int)o.x, y = (int)o.y;
  switch (o.type) {
    case OB_BAMBOO: {
      spr.fillEllipse(NINJA_X, y + 5, PATH_W / 2 - 4, 3, envShadow);
      spr.fillRoundRect(PATH_L + 4, y - 5, PATH_W - 8, 10, 4, COL_BAMBOO);
      spr.drawRoundRect(PATH_L + 4, y - 5, PATH_W - 8, 10, 4, COL_BAMBOO_D);
      for (int nx = PATH_L + 18; nx < PATH_R - 8; nx += 16)
        spr.drawFastVLine(nx, y - 5, 10, COL_BAMBOO_D);
      spr.fillTriangle(PATH_R - 6, y - 5, PATH_R + 4, y - 11, PATH_R - 1, y - 3, COL_BAMBOO);
      break;
    }
    case OB_DEMON: {
      int wob = (int)(sinf((t + o.seed) * 0.2f) * 3);
      x += wob;
      spr.fillEllipse(x, y + 6, 9, 3, envShadow);
      spr.fillRoundRect(x - 8, y - 7, 16, 14, 4, COL_DEMON);
      spr.drawRoundRect(x - 8, y - 7, 16, 14, 4, COL_DEMON_D);
      spr.fillTriangle(x - 6, y - 6, x - 4, y - 12, x - 2, y - 6, TFT_WHITE);
      spr.fillTriangle(x + 2, y - 6, x + 4, y - 12, x + 6, y - 6, TFT_WHITE);
      spr.fillCircle(x - 3, y - 1, 1, TFT_WHITE);
      spr.fillCircle(x + 3, y - 1, 1, TFT_WHITE);
      spr.drawFastHLine(x - 3, y + 4, 6, COL_DEMON_D);   // grumpy mouth
      break;
    }
    case OB_SPIKES: {
      spr.fillRect(PATH_L + 3, y + 2, PATH_W - 6, 4, COL_SPIKE_D);
      for (int sx = PATH_L + 4; sx < PATH_R - 8; sx += 10) {
        spr.fillTriangle(sx, y + 3, sx + 5, y - 7, sx + 10, y + 3, COL_SPIKE);
        spr.drawLine(sx + 5, y - 7, sx + 10, y + 3, COL_SPIKE_D);
      }
      break;
    }
    case OB_ROCK: {
      spr.fillEllipse(x, y + 8, 11, 3, envShadow);
      spr.fillCircle(x, y, 11, COL_ROCK);
      spr.drawCircle(x, y, 11, COL_ROCK_D);
      // rolling texture
      float a = dist * 0.15f + o.seed;
      spr.fillCircle(x + (int)(cosf(a) * 5), y + (int)(sinf(a) * 5), 2, COL_ROCK_D);
      spr.fillCircle(x - (int)(cosf(a) * 4), y - (int)(sinf(a) * 4), 1, COL_ROCK_D);
      break;
    }
  }
}

void renderPresenter() {
  const uint16_t BG     = C565(26, 30, 44);
  const uint16_t GREY   = C565(140, 148, 168);
  const uint16_t GREEN  = C565(80, 220, 120);
  const uint16_t AMBER  = C565(255, 190, 60);
  bool blink = (t / 22) % 2 == 0;

  spr.fillSprite(BG);
  // faint mountain silhouettes at the bottom for identity
  uint16_t mt = C565(38, 44, 64);
  spr.fillTriangle(0, 240, 30, 214, 68, 240, mt);
  spr.fillTriangle(45, 240, 95, 206, 140, 240, mt);

  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(GREY);
  spr.drawString("PRESENTER", 68, 10, 2);

  bool conn = bleKeyboard.isConnected();
  if (conn) {
    spr.setTextColor(GREEN);
    spr.drawString("CONNECTED", 68, 30, 2);
  } else if (blink) {
    spr.setTextColor(AMBER);
    spr.drawString("PAIRING...", 68, 30, 2);
  }

  spr.setTextColor(GREY);
  spr.drawString("SLIDE", 68, 56, 2);
  spr.setTextColor(TFT_WHITE);
  spr.drawNumber(slideNum, 68, 72, 6);

  spr.setTextColor(GREY);
  spr.drawString("TIME", 68, 130, 2);
  uint32_t secs = timerStartMs ? (millis() - timerStartMs) / 1000 : 0;
  String tstr = String(secs / 60) + ":" + (secs % 60 < 10 ? "0" : "") + String(secs % 60);
  spr.setTextColor(TFT_WHITE);
  spr.drawString(tstr, 68, 146, 4);

  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(TFT_WHITE);
  spr.drawString("< PREV", 8, 190, 2);
  spr.setTextDatum(TR_DATUM);
  spr.drawString("NEXT >", 127, 190, 2);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(GREY);
  spr.drawString("HOLD BOTH = EXIT", 68, 214, 2);
}

void render() {
  if (state == ST_PRESENTER) {
    renderPresenter();
    spr.pushSprite(0, 0);
    return;
  }

  // sky band with gradient + celestial body
  for (int y = 0; y < SKY_H; y++) {
    float f = (float)y / (SKY_H - 1);
    uint16_t c = C565(
      lerp8((envSkyT >> 11) << 3, (envSkyB >> 11) << 3, f),
      lerp8(((envSkyT >> 5) & 0x3F) << 2, ((envSkyB >> 5) & 0x3F) << 2, f),
      lerp8((envSkyT & 0x1F) << 3, (envSkyB & 0x1F) << 3, f));
    spr.drawFastHLine(0, y, SCR_W, c);
  }
  if (nightness > 0.5f) {
    spr.fillCircle(108, 12, 6, C565(235, 235, 210));           // moon
    spr.fillCircle(111, 10, 5, envSkyT);
    const uint8_t sx[7] = {12, 30, 55, 78, 95, 40, 65};
    const uint8_t sy[7] = {6, 14, 4, 10, 18, 20, 15};
    for (int i = 0; i < 7; i++) spr.drawPixel(sx[i], sy[i], TFT_WHITE);
  } else {
    spr.fillCircle(108, 12, 7, C565(255, 240, 150));           // sun
  }
  // mountain silhouettes on the horizon
  uint16_t mt = scale565(envSkyB, 0.55f);
  spr.fillTriangle(0, SKY_H, 28, 10, 62, SKY_H, mt);
  spr.fillTriangle(40, SKY_H, 85, 4, 132, SKY_H, mt);
  spr.fillTriangle(100, SKY_H, 130, 14, 160, SKY_H, mt);

  // grass + path
  spr.fillRect(0, SKY_H, PATH_L, SCR_H - SKY_H, envGrass);
  spr.fillRect(PATH_R, SKY_H, SCR_W - PATH_R, SCR_H - SKY_H, envGrass);
  spr.fillRect(PATH_L, SKY_H, PATH_W, SCR_H - SKY_H, envPath);
  spr.fillRect(PATH_L, SKY_H, 2, SCR_H - SKY_H, envPathEdge);
  spr.fillRect(PATH_R - 2, SKY_H, 2, SCR_H - SKY_H, envPathEdge);

  // scrolling decorations
  int off28 = (int)dist % 28;
  for (int y = SKY_H - 10 + off28; y < SCR_H; y += 28) {
    if (y > SKY_H + 2) {
      spr.fillRect(8, y, 5, 2, envGrassDk);                    // grass tufts
      spr.fillRect(118, y + 9, 5, 2, envGrassDk);
      spr.drawFastHLine(NINJA_X - 2 + ((y / 28) % 2) * 8, y + 4, 5, envPathEdge);  // path pebbles
    }
  }
  int off90 = (int)dist % 90;
  for (int y = SKY_H - 20 + off90; y < SCR_H + 20; y += 90) {
    if (y > SKY_H + 6) {                                        // trees on both sides
      spr.fillRect(13, y, 3, 6, COL_ROCK_D);
      spr.fillCircle(14, y - 4, 7, envGrassDk);
      spr.fillRect(119, y + 40, 3, 6, COL_ROCK_D);
      spr.fillCircle(120, y + 36, 7, envGrassDk);
    }
  }

  // coins, obstacles, ninja
  for (int i = 0; i < N_COINS; i++) {
    if (!coins[i].active) continue;
    int bob = coins[i].air ? (int)(sinf(t * 0.2f + i) * 2) : 0;
    drawCoinIcon((int)coins[i].x, (int)coins[i].y + bob, 4, coins[i].air);
  }
  for (int i = 0; i < N_OBS; i++)
    if (obs[i].active) drawObstacle(obs[i]);

  if (state == ST_DYING) {
    drawNinja(NINJA_X, NINJA_Y + (int)(stateT * 1.2f), 0, true);
  } else if (state != ST_TITLE) {
    drawNinja(NINJA_X, NINJA_Y, jumpZ(), false);
  }

  for (int i = 0; i < N_PARTS; i++)
    if (parts[i].active) spr.fillRect((int)parts[i].x, (int)parts[i].y, 2, 2, parts[i].col);

  // ---------- UI ----------
  bool blink = (t / 22) % 2 == 0;
  spr.setTextDatum(TC_DATUM);

  switch (state) {
    case ST_TITLE: {
      spr.setTextColor(COL_OUTLINE);
      spr.drawString("SLASH &", 69, 41, 4);
      spr.drawString("LEAP", 69, 67, 4);
      spr.setTextColor(TFT_WHITE);
      spr.drawString("SLASH &", 68, 40, 4);
      spr.drawString("LEAP", 68, 66, 4);

      drawNinja(NINJA_X, 150 + (int)(sinf(t * 0.09f) * 2), 0, false);

      spr.setTextColor(TFT_WHITE);
      spr.setTextDatum(TL_DATUM);
      spr.drawString("L: LEAP", 8, 196, 2);
      spr.setTextDatum(TR_DATUM);
      spr.drawString("R: SLASH", 127, 196, 2);
      spr.setTextDatum(TC_DATUM);
      spr.setTextColor(COL_OUTLINE);
      if (t < toastUntil)      spr.drawString("BEST RESET!", 68, 220, 2);
      else if ((t / 110) % 2 == 1) spr.drawString("HOLD R: PRESENTER", 68, 220, 2);
      else if (blink)          spr.drawString("PRESS ANY BUTTON", 68, 220, 2);
      if (hiScore > 0) {
        spr.setTextColor(TFT_WHITE);
        spr.drawString("BEST " + String(hiScore), 68, 4, 2);
      }
      break;
    }

    case ST_PLAYING:
    case ST_DYING: {
      for (int h = 0; h < 3; h++) drawHeart(4 + h * 11, 3, h < hearts);
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(COL_OUTLINE);
      spr.drawNumber(score, 132, 3, 2);
      spr.setTextColor(TFT_WHITE);
      spr.drawNumber(score, 131, 2, 2);
      spr.setTextDatum(TL_DATUM);
      drawCoinIcon(9, 22, 4, false);
      spr.setTextColor(TFT_WHITE);
      spr.drawNumber(coinCount, 17, 16, 2);
      if (multNow() > 1) {
        spr.setTextDatum(TR_DATUM);
        spr.setTextColor(multNow() >= 6 ? COL_RED : multNow() >= 3 ? COL_COIN : TFT_WHITE);
        spr.drawString("x" + String(multNow()), 131, 16, 2);
      }
      break;
    }

    case ST_OVER: {
      spr.fillRoundRect(10, 58, 115, 116, 8, COL_PANEL);
      spr.drawRoundRect(10, 58, 115, 116, 8, COL_OUTLINE);
      spr.setTextColor(COL_RED);
      spr.drawString("GAME", 68, 64, 4);
      spr.drawString("OVER", 68, 88, 4);
      spr.setTextColor(COL_TEXT_DK);
      spr.drawString("SCORE " + String(score), 68, 116, 2);
      spr.drawString("BEST " + String(hiScore), 68, 131, 2);
      spr.drawString(String(coinCount) + " COINS  " + String((int)(dist / 25)) + "m", 68, 146, 2);
      if (newHi) {
        spr.setTextColor(COL_RED);
        spr.drawString("NEW BEST!", 68, 100, 2);
      }
      if (stateT > 30 && blink) {
        spr.setTextColor(COL_TEXT_DK);
        spr.drawString("PRESS TO RETRY", 68, 160, 2);
      }
      break;
    }
  }

  if (flashFrames > 0) {
    flashFrames--;
    spr.fillSprite(TFT_WHITE);
  }

  spr.pushSprite(0, 0);
}

// dump the last rendered frame as raw RGB565 over serial ('S' command)
void dumpScreenshot() {
  const uint8_t *p = (const uint8_t *)spr.getPointer();
  if (p == nullptr) return;
  Serial.print("SNAP\n");
  Serial.write(p, SCR_W * SCR_H * 2);
  Serial.print("\nENDS\n");
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Slash & Leap booting...");

  prefs.begin("slashleap", false);
  hiScore = prefs.getUInt("hi", 0);
  Serial.printf("High score: %u\n", hiScore);

  btnJump.begin(true);    // GPIO0 has an internal pull-up
  btnSlash.begin(false);  // GPIO35: board provides the pull-up

  tft.init();
  tft.setRotation(0);     // portrait, USB at the bottom (use 2 if upside down)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  spr.setColorDepth(16);
  if (spr.createSprite(SCR_W, SCR_H) == nullptr) {
    spr.setColorDepth(8);
    spr.createSprite(SCR_W, SCR_H);
  }

  randomSeed(esp_random());
  updateEnv();
  Serial.println("Ready. Press a button!");
}

void loop() {
  static uint32_t nextFrame = millis();

  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'J') serialJump = true;
    if (c == 'K') serialSlash = true;
    if (c == 'S') dumpScreenshot();
    if (c == 'P') { if (state != ST_PRESENTER) enterPresenter(); else ESP.restart(); }
  }

  btnJump.poll();
  btnSlash.poll();
  updateEnv();
  update();
  render();

  uint32_t now = millis();
  if (nextFrame + FRAME_MS > now) delay(nextFrame + FRAME_MS - now);
  nextFrame += FRAME_MS;
  if (now > nextFrame + 100) nextFrame = now;
}
