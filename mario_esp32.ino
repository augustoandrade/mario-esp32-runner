/* =====================================================================
   MARIO RUNNER  —  ESP32-2432S012 (tela redonda 240x240, GC9A01)
   ---------------------------------------------------------------------
   Jogo estilo "dino do Chrome" com mecanicas de SUPER MARIO WORLD:

     - TOQUE na tela = pular  (segure = pulo alto, toque curto = baixinho)
     - Canos verdes  = desvie ou POUSE em cima (plataforma!)
     - PLANTA CARNIVORA = sobe e desce de alguns canos — cuidado ao pousar!
     - GOOMBA / KOOPA   = pise pra derrotar; casco do koopa e chutavel
     - BALA BILL        = voa baixo (pule!) ou alto (fique no chao!)
     - BLOCO "?"        = bata POR BAIXO: moeda, cogumelo, estrela ou YOSHI
     - COGUMELO  = Super Mario com capa (aguenta 1 batida)
     - ESTRELA   = invencivel ~5s, destroi TUDO que tocar (ate canos!)
     - OVO YOSHI = monta no Yoshi: pulo mais forte + aguenta 1 batida
                   (o Yoshi foge quando voce apanha, como no SNES)
     - VIDAS     = 3 coracoes; a cada 30 moedas ganha 1UP (max 5)
     - MUNDOS    = a cada 150 pts muda o cenario (dia/por do sol/noite)
     - RECORDE   = salvo na flash (sobrevive ao desligar)

   OPCIONAL — tema pelo horario real (relogio via internet):
     preencha WIFI_SSID/WIFI_PASS abaixo (rede 2.4 GHz!). Ao ligar, a
     placa acerta o relogio e comeca o jogo com cenario de dia, por do
     sol ou noite conforme a hora. Deixe "" pra desativar.

   Placa: ESP32-C3 (1 nucleo, ~400KB RAM, SEM PSRAM)
   Tela : GC9A01 240x240 SPI       Touch: CST816 I2C (0x15)

   >>> Ferramentas (Tools) no Arduino IDE:
       Board .............. ESP32C3 Dev Module
       USB CDC On Boot .... Enabled
       Partition Scheme ... Huge APP (3MB No OTA/1MB SPIFFS)

   >>> Bibliotecas (Library Manager):
       - GFX Library for Arduino  (moononournation)
       (Preferences, Wire e WiFi ja vem no core ESP32)
   ===================================================================== */

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>

/* ---------------------------------------------------------------------
   0) WI-FI OPCIONAL (so pro relogio; deixe "" pra desativar)
   --------------------------------------------------------------------- */
#define WIFI_SSID ""          // nome da sua rede 2.4 GHz
#define WIFI_PASS ""          // senha
const long TZ_OFFSET = -3 * 3600;   // fuso de Brasilia

/* ---------------------------------------------------------------------
   1) PINOS (fixos, internos da placa — NAO mexer)
   --------------------------------------------------------------------- */
#define TFT_SCK   6
#define TFT_MOSI  7
#define TFT_DC    2
#define TFT_CS    10
#define TFT_RST   -1
#define TFT_BL    3

#define TOUCH_SDA 4
#define TOUCH_SCL 5
#define TOUCH_ADDR 0x15

/* ---------------------------------------------------------------------
   2) CORES (RGB565 — Arduino_GFX nao define RED/BLUE, definimos na mao)
   --------------------------------------------------------------------- */
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_MRED    0xE185
#define C_SKIN    0xFDB8
#define C_MBLUE   0x22BC
#define C_BROWN   0x8A22
#define C_DARK    0x2124
#define C_GREEN   0x07E0
#define C_LGREEN  0x8FE0
#define C_DGREEN  0x0480
#define C_YELLOW  0xFFE0
#define C_COINDK  0xC300
#define C_GBODY   0xAA85
#define C_GTAN    0xF652
#define C_KSKIN   0xFE60
#define C_UBLOCK  0x9284
#define C_YGRN    0x5647   // verde do Yoshi

/* ---------------------------------------------------------------------
   3) TIPOS (struct/enum SEMPRE no topo, antes das funcoes!)
   --------------------------------------------------------------------- */
enum GameState { STATE_READY, STATE_PLAY, STATE_OVER };

struct Pipe   { float x; int h; bool active; bool scored; bool hasPlant; int plantPhase; };
struct Goomba { float x; bool active; bool scored; int squash; };
struct Koopa  { float x; bool active; bool scored; int mode; };   // 0 anda, 1 casco, 2 casco chutado
struct Coin   { float x; int y; bool active; };
struct Item   { float x; float baseY; bool active; int kind; };   // 1 cogumelo, 2 estrela, 3 ovo yoshi
struct QBlock { float x; bool active; bool used; int bump; int coinPop; };
struct Bullet { float x; int y; bool active; bool scored; };

/* ---------------------------------------------------------------------
   4) SPRITE DO MARIO (16x16, ampliado 2x -> 32x32)
        0=transp 1=vermelho 2=pele 3=marrom 4=azul 5=escuro 6=branco
   --------------------------------------------------------------------- */
const uint16_t marioPal[7] = { 0, C_MRED, C_SKIN, C_BROWN, C_MBLUE, C_DARK, C_WHITE };

const uint8_t marioBody[14][16] = {
  {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
  {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
  {0,0,0,0,3,3,3,2,2,3,2,0,0,0,0,0},
  {0,0,0,3,2,5,2,2,2,3,2,2,2,0,0,0},
  {0,0,0,3,2,5,5,2,2,2,3,2,2,2,0,0},
  {0,0,0,3,3,2,2,2,2,3,3,3,3,0,0,0},
  {0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0},
  {0,0,0,1,1,1,4,1,1,4,1,1,1,0,0,0},
  {0,0,1,1,1,1,4,1,1,4,1,1,1,1,0,0},
  {0,0,1,1,1,1,4,4,4,4,1,1,1,1,0,0},
  {0,0,2,2,4,4,6,4,4,6,4,4,2,2,0,0},
  {0,0,2,2,4,4,4,4,4,4,4,4,2,2,0,0},
  {0,0,2,2,4,4,4,4,4,4,4,4,2,2,0,0},
  {0,0,0,0,4,4,4,0,0,4,4,4,0,0,0,0},
};

const uint8_t marioLegs[3][2][16] = {
  { {0,0,0,3,3,3,0,0,0,0,3,3,3,0,0,0},
    {0,0,3,3,3,3,0,0,0,0,3,3,3,3,0,0} },
  { {0,0,0,0,3,3,3,0,3,3,3,0,0,0,0,0},
    {0,0,0,3,3,3,3,3,3,3,3,3,0,0,0,0} },
  { {0,0,3,3,3,0,0,0,0,0,0,3,3,3,0,0},
    {0,3,3,3,0,0,0,0,0,0,0,0,3,3,3,0} },
};

/* ---------------------------------------------------------------------
   5) SPRITE DO GOOMBA (16x12 -> 32x24)
   --------------------------------------------------------------------- */
const uint16_t goombaPal[5] = { 0, C_GBODY, C_GTAN, C_DARK, C_WHITE };

const uint8_t goombaSpr[12][16] = {
  {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
  {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
  {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
  {0,1,1,1,4,4,1,1,1,1,4,4,1,1,1,0},
  {0,1,1,4,4,3,1,1,1,1,3,4,4,1,1,0},
  {1,1,1,4,4,3,1,1,1,1,3,4,4,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
  {0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
  {0,3,3,3,3,2,2,2,2,2,2,3,3,3,3,0},
  {3,3,3,3,3,3,0,0,0,0,3,3,3,3,3,3},
};

/* ---------------------------------------------------------------------
   6) SPRITE DO KOOPA (16x16 -> 32x32)
   --------------------------------------------------------------------- */
const uint16_t koopaPal[6] = { 0, C_GREEN, C_KSKIN, C_DARK, C_WHITE, C_DGREEN };

const uint8_t koopaSpr[16][16] = {
  {0,0,2,2,2,0,0,0,0,0,0,0,0,0,0,0},
  {0,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0},
  {2,2,4,3,2,2,0,0,0,0,0,0,0,0,0,0},
  {2,2,4,3,2,2,0,0,0,0,0,0,0,0,0,0},
  {2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0},
  {0,2,2,2,2,0,1,1,1,1,1,0,0,0,0,0},
  {0,0,2,2,1,1,1,1,1,1,1,1,1,0,0,0},
  {0,0,2,2,1,1,5,1,1,1,5,1,1,1,0,0},
  {0,0,2,2,1,5,1,1,5,1,1,5,1,1,1,0},
  {0,0,0,2,1,1,1,5,1,1,5,1,1,1,1,0},
  {0,0,0,2,1,5,1,1,1,5,1,1,5,1,1,0},
  {0,0,0,2,2,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,0,0,2,4,4,4,4,4,4,4,4,0,0,0},
  {0,0,0,0,2,2,2,0,0,0,2,2,2,0,0,0},
  {0,0,0,2,2,2,0,0,0,0,0,2,2,2,0,0},
  {0,0,2,2,2,2,0,0,0,0,2,2,2,2,0,0},
};

/* ---------------------------------------------------------------------
   7) SPRITE DO COGUMELO (12x11 -> 24x22)
   --------------------------------------------------------------------- */
const uint16_t shroomPal[5] = { 0, C_MRED, C_WHITE, C_SKIN, C_DARK };

const uint8_t shroomSpr[11][12] = {
  {0,0,0,0,1,1,1,1,0,0,0,0},
  {0,0,1,1,2,2,2,2,1,1,0,0},
  {0,1,1,2,2,2,2,2,2,1,1,0},
  {0,1,1,2,2,2,2,2,2,1,1,0},
  {1,2,1,1,1,1,1,1,1,1,2,1},
  {1,2,2,1,1,1,1,1,1,2,2,1},
  {1,1,1,1,1,1,1,1,1,1,1,1},
  {0,0,3,3,3,3,3,3,3,3,0,0},
  {0,0,3,4,3,3,3,3,4,3,0,0},
  {0,0,3,4,3,3,3,3,4,3,0,0},
  {0,0,3,3,3,3,3,3,3,3,0,0},
};

/* ---------------------------------------------------------------------
   8) SPRITE DO YOSHI (16x12 -> 32x24, virado pra direita)
        0=transp 1=verde 2=branco 3=escuro 4=vermelho (sela)
   --------------------------------------------------------------------- */
const uint16_t yoshiPal[5] = { 0, C_YGRN, C_WHITE, C_DARK, C_MRED };

const uint8_t yoshiSpr[12][16] = {
  {0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,2,3,1,0,0},
  {0,0,0,0,0,0,0,0,0,1,1,2,3,1,1,1},
  {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1},
  {0,0,4,4,4,4,4,0,1,1,1,1,1,1,1,1},
  {0,4,4,4,4,4,4,4,1,1,1,0,0,0,0,0},
  {1,1,4,4,4,4,4,4,1,1,1,0,0,0,0,0},
  {1,1,1,2,2,2,2,1,1,1,0,0,0,0,0,0},
  {0,1,1,2,2,2,2,1,1,1,0,0,0,0,0,0},
  {0,0,1,1,2,2,1,1,1,0,0,0,0,0,0,0},
  {0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0},
  {0,1,1,1,0,0,1,1,1,0,0,0,0,0,0,0},
};

/* ---------------------------------------------------------------------
   9) OBJETOS DE TELA
   --------------------------------------------------------------------- */
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
Arduino_GFX     *out = new Arduino_GC9A01(bus, TFT_RST, 0, true);
Arduino_Canvas  *gfx = new Arduino_Canvas(240, 240, out);

Preferences prefs;

/* ---------------------------------------------------------------------
   10) CONSTANTES DO MUNDO
   --------------------------------------------------------------------- */
const int SCREEN_W  = 240;
const int SCREEN_H  = 240;
const int GROUND_Y  = 178;
const int MARIO_X   = 44;
const int MARIO_W   = 32;
const int MARIO_H   = 32;
const int PIPE_W    = 26;
const int GO_W      = 32, GO_H = 24;
const int KO_W      = 32, KO_H = 32;
const int SH_W      = 24, SH_H = 16;
const int QB_SIZE   = 24;
const int QB_Y      = GROUND_Y - 94;
const int SHR_W     = 24, SHR_H = 22;   // caixa dos itens (cogumelo/estrela/ovo)
const int BUL_W     = 28, BUL_H = 14;   // bala bill

const float GRAVITY   = 0.65f;
const float JUMP_VEL  = -9.2f;

const int MAX_PIPES   = 3;
const int MAX_GOOMBAS = 3;
const int MAX_KOOPAS  = 2;
const int MAX_COINS   = 6;
const int MAX_QBLOCKS = 2;
const int MAX_ITEMS   = 2;
const int MAX_BULLETS = 2;

const int WORLD_STEP  = 150;   // pontos pra trocar de mundo
const int COINS_1UP   = 30;    // moedas pra ganhar vida
const int MAX_LIVES   = 5;

/* ---------------------------------------------------------------------
   11) TEMAS DOS MUNDOS (0=dia 1=por do sol 2=noite)
   --------------------------------------------------------------------- */
const uint16_t skyCols[3]   = { 0x5D9F, 0xFCAC, 0x10C8 };
const uint16_t hillCols[3]  = { 0x0480, 0x52C3, 0x09E3 };
const uint16_t cloudCols[3] = { 0xFFFF, 0xFF39, 0xA534 };
const uint16_t grassCols[3] = { 0x07E0, 0x8DA7, 0x03E0 };

const uint8_t starX[6] = { 30, 90, 160, 210, 120, 60 };
const uint8_t starY[6] = { 60, 30,  50,  90,  14, 110 };

/* ---------------------------------------------------------------------
   12) VARIAVEIS DO JOGO
   --------------------------------------------------------------------- */
GameState state = STATE_READY;

float marioY;
float marioVel;
bool  onGround;
int   runFrame;

bool  superMario;     // capa (cogumelo)
bool  riding;         // montado no Yoshi
int   invTimer;       // invencibilidade piscando (apos dano)
int   starTimer;      // invencibilidade da ESTRELA (destroi tudo)
float yoshiFleeX = -999;   // yoshi fugindo (apos dano); -999 = nenhum

int   lives;
int   nextLifeAt;     // proxima vida por moedas

Pipe   pipes[MAX_PIPES];
Goomba goombas[MAX_GOOMBAS];
Koopa  koopas[MAX_KOOPAS];
Coin   coinsArr[MAX_COINS];
QBlock qblocks[MAX_QBLOCKS];
Item   items[MAX_ITEMS];
Bullet bullets[MAX_BULLETS];

float worldSpeed;
int   passedCount;
int   distSinceLast;
int   nextGap;
float coinDist;
int   coinGap;

int   score;
int   coinCount;
int   bestScore = 0;

int   worldNum     = 1;
int   lastWorldNum = 1;
int   themeOffset  = 0;    // definido pelo relogio (se Wi-Fi configurado)
int   bannerTimer  = 0;
char  bannerText[16] = "";

float hillOff = 0, cloudOff = 0, groundOff = 0;

bool  wasTouching = false;
unsigned long lastFrame = 0;
const unsigned long FRAME_MS = 24;
uint32_t frameTick = 0;

/* =====================================================================
   TOUCH — CST816 por polling
   ===================================================================== */
bool isTouched() {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(TOUCH_ADDR, 6);
  if (Wire.available() < 6) return false;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();
  return (d[1] > 0);
}

bool justTapped() {
  bool now = isTouched();
  bool tapped = (now && !wasTouching);
  wasTouching = now;
  return tapped;
}

/* =====================================================================
   BANNER (avisos: MUNDO N, 1UP!)
   ===================================================================== */
void setBanner(const char *t) {
  strncpy(bannerText, t, 15);
  bannerText[15] = 0;
  bannerTimer = 45;
}

/* =====================================================================
   RESET DO JOGO
   ===================================================================== */
void resetGame() {
  marioY   = GROUND_Y - MARIO_H;
  marioVel = 0;
  onGround = true;
  runFrame = 0;

  superMario = false;
  riding     = false;
  invTimer   = 0;
  starTimer  = 0;
  yoshiFleeX = -999;

  lives      = 3;
  nextLifeAt = COINS_1UP;

  for (int i = 0; i < MAX_PIPES;   i++) pipes[i].active    = false;
  for (int i = 0; i < MAX_GOOMBAS; i++) goombas[i].active  = false;
  for (int i = 0; i < MAX_KOOPAS;  i++) koopas[i].active   = false;
  for (int i = 0; i < MAX_COINS;   i++) coinsArr[i].active = false;
  for (int i = 0; i < MAX_QBLOCKS; i++) qblocks[i].active  = false;
  for (int i = 0; i < MAX_ITEMS;   i++) items[i].active    = false;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active  = false;

  worldSpeed    = 3.0f;
  passedCount   = 0;
  distSinceLast = 0;
  nextGap       = 290;   // 1o obstaculo demora mais
  coinDist      = 0;
  coinGap       = 120;

  score        = 0;
  coinCount    = 0;
  worldNum     = 1;
  lastWorldNum = 1;
  bannerTimer  = 0;
}

/* =====================================================================
   VIDA EXTRA por moedas
   ===================================================================== */
void checkOneUp() {
  if (coinCount >= nextLifeAt) {
    if (lives < MAX_LIVES) lives++;
    nextLifeAt += COINS_1UP;
    setBanner("1UP!");
  }
}

/* =====================================================================
   DANO — cascata: estrela > yoshi > capa > vida > game over
   ===================================================================== */
void takeHit() {
  if (invTimer > 0 || starTimer > 0) return;
  if (riding) {                       // Yoshi assusta e foge (como no SNES!)
    riding = false;
    yoshiFleeX = MARIO_X;
    invTimer = 45;
    return;
  }
  if (superMario) {                   // perde a capa
    superMario = false;
    invTimer = 45;
    return;
  }
  lives--;                            // perde um coracao
  if (lives > 0) {
    invTimer = 60;                    // respawn piscando, jogo continua
  } else {
    if (score > bestScore) {
      bestScore = score;
      prefs.putInt("best", bestScore);
    }
    state = STATE_OVER;
  }
}

/* =====================================================================
   SPAWNS
   ===================================================================== */
void spawnPipe() {
  for (int i = 0; i < MAX_PIPES; i++)
    if (!pipes[i].active) {
      int h = (passedCount < 2) ? random(26, 34) : random(26, 50);
      bool plant = (passedCount >= 8 && random(0, 100) < 35);
      pipes[i] = { (float)(SCREEN_W + 10), h, true, false, plant, (int)random(0, 100) };
      return;
    }
}

void spawnGoomba() {
  for (int i = 0; i < MAX_GOOMBAS; i++)
    if (!goombas[i].active) {
      goombas[i] = { (float)(SCREEN_W + 10), true, false, 0 };
      return;
    }
}

void spawnKoopa() {
  for (int i = 0; i < MAX_KOOPAS; i++)
    if (!koopas[i].active) {
      koopas[i] = { (float)(SCREEN_W + 10), true, false, 0 };
      return;
    }
}

void spawnBullet() {
  for (int i = 0; i < MAX_BULLETS; i++)
    if (!bullets[i].active) {
      // baixa (tem que pular) ou alta (fique no chao que ela passa!)
      int y = (random(0, 2) == 0) ? GROUND_Y - 20 : GROUND_Y - 58;
      bullets[i] = { (float)(SCREEN_W + 10), y, true, false };
      return;
    }
}

void spawnCoinArc() {
  bool pipeNear = false;
  for (int i = 0; i < MAX_PIPES; i++)
    if (pipes[i].active && pipes[i].x > SCREEN_W - 60) pipeNear = true;

  bool high = pipeNear ? true : (random(0, 2) == 0);
  float x0 = SCREEN_W + 20;

  int placed = 0;
  for (int i = 0; i < MAX_COINS && placed < 3; i++) {
    if (coinsArr[i].active) continue;
    coinsArr[i].active = true;
    coinsArr[i].x = x0 + placed * 20;
    if (high) coinsArr[i].y = (placed == 1) ? GROUND_Y - 86 : GROUND_Y - 72;
    else      coinsArr[i].y = GROUND_Y - 26;
    placed++;
  }
}

void spawnQBlock() {
  for (int i = 0; i < MAX_QBLOCKS; i++)
    if (!qblocks[i].active) {
      qblocks[i] = { (float)(SCREEN_W + 30), true, false, 0, 0 };
      return;
    }
}

bool spawnItem(int kind, float x, float baseY) {
  for (int i = 0; i < MAX_ITEMS; i++)
    if (!items[i].active) {
      items[i] = { x, baseY, true, kind };
      return true;
    }
  return false;
}

/* =====================================================================
   PLANTA CARNIVORA — quanto ela esta pra fora (0..22 px)
   ciclo: sobe (20 quadros) - fora (40) - desce (20) - escondida (20)
   ===================================================================== */
int plantExt(int phase) {
  int ph = phase % 100;
  if (ph < 20) return ph * 22 / 20;
  if (ph < 60) return 22;
  if (ph < 80) return (80 - ph) * 22 / 20;
  return 0;
}

/* =====================================================================
   ATUALIZACAO DO MUNDO
   ===================================================================== */
void updateWorld() {
  // ---- linha do tempo de obstaculos ----
  distSinceLast += (int)worldSpeed;
  if (distSinceLast >= nextGap) {
    int r = random(0, 100);
    if      (passedCount >= 10 && r < 15) spawnBullet();
    else if (passedCount >= 6  && r < 35) spawnKoopa();
    else if (passedCount >= 3  && r < 60) spawnGoomba();
    else                                   spawnPipe();

    if (!superMario && starTimer == 0 && random(0, 100) < 8)
      spawnItem(1, SCREEN_W + 70, GROUND_Y - 82);   // cogumelo avulso raro

    distSinceLast = 0;
    nextGap = random(115, 185);
  }

  // ---- moedas / blocos "?" ----
  coinDist += worldSpeed;
  if (coinDist >= coinGap) {
    if (random(0, 100) < 30) spawnQBlock();
    else                     spawnCoinArc();
    coinDist = 0;
    coinGap = random(150, 280);
  }

  // caixa do Mario (montado no Yoshi fica mais alto)
  int mL = MARIO_X + 5;
  int mR = MARIO_X + MARIO_W - 5;
  int mT = (int)marioY + (riding ? -8 : 4);
  int mB = (int)marioY + MARIO_H;

  bool star = (starTimer > 0);

  // ---- canos (+ planta carnivora) ----
  for (int i = 0; i < MAX_PIPES; i++) {
    if (!pipes[i].active) continue;
    pipes[i].x -= worldSpeed;
    pipes[i].plantPhase++;

    if (!pipes[i].scored && pipes[i].x + PIPE_W < MARIO_X) {
      pipes[i].scored = true;
      score += 10;  passedCount++;
    }
    if (pipes[i].x < -30) { pipes[i].active = false; continue; }

    int pL = (int)pipes[i].x + 3;
    int pR = (int)pipes[i].x + PIPE_W - 3;
    int pT = GROUND_Y - pipes[i].h;

    // planta carnivora morde?
    if (pipes[i].hasPlant) {
      int ext = plantExt(pipes[i].plantPhase);
      if (ext > 8) {
        int cx = (int)pipes[i].x + PIPE_W / 2;
        if (mR > cx - 8 && mL < cx + 8 && mB > pT - ext && mT < pT) {
          if (star) { pipes[i].hasPlant = false; score += 20; }
          else takeHit();
        }
      }
    }

    if (mR > pL && mL < pR && mB >= pT) {
      if (marioVel >= 0 && (mB - pT) <= 14) {   // pousa no cano (plataforma)
        marioY   = pT - MARIO_H;
        marioVel = 0;
        onGround = true;
      } else if (star) {                        // estrela ATRAVESSA o cano!
        pipes[i].active = false;
        score += 10;
      } else {
        takeHit();
      }
    }
  }

  // ---- goombas ----
  for (int i = 0; i < MAX_GOOMBAS; i++) {
    if (!goombas[i].active) continue;
    goombas[i].x -= worldSpeed + 0.4f;

    if (goombas[i].squash > 0) {
      if (--goombas[i].squash == 0) goombas[i].active = false;
      continue;
    }
    if (!goombas[i].scored && goombas[i].x + GO_W < MARIO_X) {
      goombas[i].scored = true;
      score += 10;  passedCount++;
    }
    if (goombas[i].x < -40) { goombas[i].active = false; continue; }

    int gL = (int)goombas[i].x + 4;
    int gR = (int)goombas[i].x + GO_W - 4;
    int gT = GROUND_Y - GO_H;
    if (mR > gL && mL < gR && mB > gT) {
      if (star) {                                  // estrela destroi
        goombas[i].squash = 8;
        goombas[i].scored = true;
        score += 20;
      } else if (marioVel > 1.0f && mB < gT + 16) {  // pisao
        goombas[i].squash = 10;
        goombas[i].scored = true;
        marioVel = JUMP_VEL * 0.75f;
        onGround = false;
        score += 20;  passedCount++;
      } else takeHit();
    }
  }

  // ---- koopas ----
  for (int i = 0; i < MAX_KOOPAS; i++) {
    if (!koopas[i].active) continue;

    if (koopas[i].mode == 0)      koopas[i].x -= worldSpeed + 0.3f;
    else if (koopas[i].mode == 1) koopas[i].x -= worldSpeed;
    else                          koopas[i].x += 7.5f - worldSpeed;

    if (koopas[i].x < -45 || koopas[i].x > SCREEN_W + 20) {
      koopas[i].active = false;
      continue;
    }

    if (koopas[i].mode == 0 && !koopas[i].scored && koopas[i].x + KO_W < MARIO_X) {
      koopas[i].scored = true;
      score += 10;  passedCount++;
    }

    // casco deslizando destroi inimigos
    if (koopas[i].mode == 2) {
      float sx = koopas[i].x;
      for (int g = 0; g < MAX_GOOMBAS; g++) {
        if (!goombas[g].active || goombas[g].squash > 0) continue;
        if (sx + SH_W > goombas[g].x + 4 && sx < goombas[g].x + GO_W - 4) {
          goombas[g].squash = 8;
          goombas[g].scored = true;
          score += 20;
        }
      }
      for (int k = 0; k < MAX_KOOPAS; k++) {
        if (k == i || !koopas[k].active || koopas[k].mode != 0) continue;
        if (sx + SH_W > koopas[k].x + 4 && sx < koopas[k].x + KO_W - 4) {
          koopas[k].active = false;
          score += 20;
        }
      }
      for (int p = 0; p < MAX_PIPES; p++) {
        if (!pipes[p].active) continue;
        if (sx + SH_W > pipes[p].x + 3 && sx < pipes[p].x + PIPE_W - 3) {
          koopas[i].active = false;
          break;
        }
      }
      continue;
    }

    int kL = (int)koopas[i].x + 4;
    int kR = (int)koopas[i].x + ((koopas[i].mode == 0) ? KO_W : SH_W) - 4;
    int kT = GROUND_Y - ((koopas[i].mode == 0) ? KO_H : SH_H);
    if (mR > kL && mL < kR && mB > kT) {
      if (star) {                                  // estrela destroi
        koopas[i].active = false;
        score += 20;
      } else if (koopas[i].mode == 0) {
        if (marioVel > 1.0f && mB < kT + 16) {
          koopas[i].mode = 1;
          koopas[i].scored = true;
          marioVel = JUMP_VEL * 0.75f;
          onGround = false;
          score += 20;  passedCount++;
        } else takeHit();
      } else {                                     // chuta o casco
        koopas[i].mode = 2;
        score += 10;
        if (marioVel > 1.0f) { marioVel = JUMP_VEL * 0.6f; onGround = false; }
      }
    }
  }

  // ---- balas bill ----
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    bullets[i].x -= worldSpeed + 2.5f;   // mais rapida que o mundo

    if (!bullets[i].scored && bullets[i].x + BUL_W < MARIO_X) {
      bullets[i].scored = true;
      score += 10;  passedCount++;
    }
    if (bullets[i].x < -40) { bullets[i].active = false; continue; }

    int bL = (int)bullets[i].x + 2;
    int bR = (int)bullets[i].x + BUL_W - 2;
    int bT = bullets[i].y;
    int bB = bullets[i].y + BUL_H;
    if (mR > bL && mL < bR && mB > bT && mT < bB) {
      if (star) {
        bullets[i].active = false;
        score += 20;
      } else if (marioVel > 1.0f && mB < bT + 10) {   // da pra pisar nela!
        bullets[i].active = false;
        marioVel = JUMP_VEL * 0.75f;
        onGround = false;
        score += 20;
      } else takeHit();
    }
  }

  // ---- blocos "?" ----
  for (int i = 0; i < MAX_QBLOCKS; i++) {
    if (!qblocks[i].active) continue;
    qblocks[i].x -= worldSpeed;
    if (qblocks[i].bump    > 0) qblocks[i].bump--;
    if (qblocks[i].coinPop > 0) qblocks[i].coinPop--;
    if (qblocks[i].x < -30) { qblocks[i].active = false; continue; }

    if (!qblocks[i].used && marioVel < -2.0f) {
      int bL = (int)qblocks[i].x;
      int bR = (int)qblocks[i].x + QB_SIZE;
      int bBot = QB_Y + QB_SIZE;
      if (mR > bL + 3 && mL < bR - 3 && mT <= bBot + 4 && mT >= bBot - 8) {
        qblocks[i].used = true;
        qblocks[i].bump = 6;
        marioVel = 0.5f;

        // premio: ovo do Yoshi > estrela > cogumelo > moeda
        int roll = random(0, 100);
        bool given = false;
        if      (roll < 15 && !riding)          given = spawnItem(3, qblocks[i].x, QB_Y - 26);
        else if (roll < 28 && starTimer == 0)   given = spawnItem(2, qblocks[i].x, QB_Y - 26);
        else if (roll < 55 && !superMario)      given = spawnItem(1, qblocks[i].x, QB_Y - 24);
        if (!given) {
          qblocks[i].coinPop = 12;
          coinCount++;
          score += 5;
          checkOneUp();
        }
      }
    }
  }

  // ---- moedas ----
  for (int i = 0; i < MAX_COINS; i++) {
    if (!coinsArr[i].active) continue;
    coinsArr[i].x -= worldSpeed;
    if (coinsArr[i].x < -15) { coinsArr[i].active = false; continue; }

    int cx = (int)coinsArr[i].x, cy = coinsArr[i].y;
    if (mR > cx - 8 && mL < cx + 8 && mB > cy - 9 && mT < cy + 9) {
      coinsArr[i].active = false;
      coinCount++;
      score += 5;
      checkOneUp();
    }
  }

  // ---- itens (cogumelo / estrela / ovo yoshi) ----
  for (int i = 0; i < MAX_ITEMS; i++) {
    if (!items[i].active) continue;
    items[i].x -= worldSpeed;
    if (items[i].x < -30) { items[i].active = false; continue; }

    int sy = (int)(items[i].baseY + sinf(frameTick * 0.15f) * 3.0f);
    if (mR > items[i].x && mL < items[i].x + SHR_W && mB > sy && mT < sy + SHR_H) {
      int kind = items[i].kind;
      items[i].active = false;
      score += 10;
      if      (kind == 1) superMario = true;
      else if (kind == 2) { starTimer = 200; setBanner("ESTRELA!"); }
      else if (kind == 3 && !riding) { riding = true; setBanner("YOSHI!"); }
    }
  }

  // ---- yoshi fugindo ----
  if (yoshiFleeX > -999) {
    yoshiFleeX += 5.0f;
    if (yoshiFleeX > SCREEN_W + 20) yoshiFleeX = -999;
  }

  // ---- velocidade / mundo / parallax ----
  worldSpeed = 3.0f + passedCount * 0.1f;
  if (worldSpeed > 6.5f) worldSpeed = 6.5f;

  worldNum = 1 + score / WORLD_STEP;
  if (worldNum != lastWorldNum) {
    lastWorldNum = worldNum;
    char buf[16];
    snprintf(buf, sizeof(buf), "MUNDO %d", worldNum);
    setBanner(buf);
  }
  if (bannerTimer > 0) bannerTimer--;

  groundOff = fmodf(groundOff + worldSpeed, 24.0f);
  hillOff   = fmodf(hillOff  + worldSpeed * 0.30f, 360.0f);
  cloudOff  = fmodf(cloudOff + worldSpeed * 0.12f, 360.0f);
}

/* =====================================================================
   DESENHO — MARIO (com estrela, as cores piscam num arco-iris)
   ===================================================================== */
uint16_t marioAccent() {
  if (starTimer == 0) return C_MRED;
  static const uint16_t cyc[4] = { C_MRED, C_YELLOW, C_GREEN, C_WHITE };
  return cyc[(frameTick / 3) % 4];
}

void drawMario(int x, int y, int frame) {
  uint16_t accent = marioAccent();
  if (superMario) {
    int flap = (frame == 2) ? 4 : (runFrame ? 2 : 0);
    gfx->fillTriangle(x + 8, y + 16, x - 4 - flap, y + 30, x + 10, y + 30, C_YELLOW);
  }
  for (int r = 0; r < 14; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = marioBody[r][c];
      if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2,
                             (idx == 1) ? accent : marioPal[idx]);
    }
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = marioLegs[frame][r][c];
      if (idx) gfx->fillRect(x + c * 2, y + (14 + r) * 2, 2, 2, marioPal[idx]);
    }
}

/* =====================================================================
   DESENHO — YOSHI (montaria e fugindo)
   ===================================================================== */
void drawYoshi(int x, int y) {
  for (int r = 0; r < 12; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = yoshiSpr[r][c];
      if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2, yoshiPal[idx]);
    }
}

/* =====================================================================
   DESENHO — GOOMBA / KOOPA / CASCO
   ===================================================================== */
void drawGoomba(int x, bool squashed) {
  if (!squashed) {
    int y = GROUND_Y - GO_H;
    for (int r = 0; r < 12; r++)
      for (int c = 0; c < 16; c++) {
        uint8_t idx = goombaSpr[r][c];
        if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2, goombaPal[idx]);
      }
  } else {
    int y = GROUND_Y - 12;
    for (int r = 0; r < 12; r++)
      for (int c = 0; c < 16; c++) {
        uint8_t idx = goombaSpr[r][c];
        if (idx) gfx->fillRect(x + c * 2, y + r, 2, 1, goombaPal[idx]);
      }
  }
}

void drawKoopaWalk(int x) {
  int y = GROUND_Y - KO_H;
  int fo = ((frameTick / 4) % 2) * 2;
  for (int r = 0; r < 16; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = koopaSpr[r][c];
      if (idx) {
        int xo = (r >= 13) ? fo : 0;
        gfx->fillRect(x + c * 2 + xo, y + r * 2, 2, 2, koopaPal[idx]);
      }
    }
}

void drawShell(int x, bool sliding) {
  int y = GROUND_Y - SH_H;
  gfx->fillRoundRect(x, y, SH_W, SH_H, 6, C_GREEN);
  gfx->drawRoundRect(x, y, SH_W, SH_H, 6, C_DGREEN);
  gfx->fillRoundRect(x + 2, y + SH_H - 6, SH_W - 4, 5, 3, C_WHITE);
  if (sliding) {
    int o = (frameTick / 2) % 3;
    gfx->fillRect(x + 4 + o * 6, y + 3, 2, 7, C_DGREEN);
    gfx->fillRect(x + 10 + o * 4, y + 3, 2, 7, C_DGREEN);
  } else {
    gfx->fillRect(x + 8, y + 3, 2, 7, C_DGREEN);
    gfx->fillRect(x + 14, y + 3, 2, 7, C_DGREEN);
  }
}

/* =====================================================================
   DESENHO — BALA BILL (aponta pra esquerda)
   ===================================================================== */
void drawBullet(int x, int y) {
  gfx->fillCircle(x + 7, y + 7, 7, C_DARK);
  gfx->fillRoundRect(x + 7, y, BUL_W - 8, BUL_H, 3, C_DARK);
  gfx->fillCircle(x + 5, y + 5, 2, C_WHITE);
  gfx->fillRect(x + 18, y - 3, 5, 4, C_DARK);
  gfx->fillRect(x + 18, y + BUL_H - 1, 5, 4, C_DARK);
}

/* =====================================================================
   DESENHO — ITENS
   ===================================================================== */
void drawCoin(int cx, int cy) {
  static const int rxTab[4] = { 7, 5, 2, 5 };
  int rx = rxTab[(frameTick / 3) % 4];
  gfx->fillEllipse(cx, cy, rx, 8, C_YELLOW);
  gfx->drawEllipse(cx, cy, rx, 8, C_COINDK);
  if (rx > 4) gfx->fillRect(cx - 1, cy - 4, 2, 8, C_COINDK);
}

void drawShroom(int x, int y) {
  for (int r = 0; r < 11; r++)
    for (int c = 0; c < 12; c++) {
      uint8_t idx = shroomSpr[r][c];
      if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2, shroomPal[idx]);
    }
}

void drawStarItem(int x, int y) {
  int cx = x + 11;
  gfx->fillTriangle(cx, y, x + 2, y + 20, x + 20, y + 20, C_YELLOW);
  gfx->fillTriangle(cx, y + 22, x + 2, y + 6, x + 20, y + 6, C_YELLOW);
  gfx->fillRect(cx - 5, y + 8, 2, 5, C_DARK);   // olhos
  gfx->fillRect(cx + 3, y + 8, 2, 5, C_DARK);
}

void drawEgg(int x, int y) {
  gfx->fillEllipse(x + 11, y + 11, 9, 11, C_WHITE);
  gfx->drawEllipse(x + 11, y + 11, 9, 11, C_DARK);
  gfx->fillCircle(x + 8,  y + 6,  3, C_YGRN);
  gfx->fillCircle(x + 15, y + 12, 3, C_YGRN);
  gfx->fillCircle(x + 8,  y + 17, 3, C_YGRN);
}

void drawItem(int i) {
  int x = (int)items[i].x;
  int y = (int)(items[i].baseY + sinf(frameTick * 0.15f) * 3.0f);
  if      (items[i].kind == 1) drawShroom(x, y);
  else if (items[i].kind == 2) drawStarItem(x, y);
  else                         drawEgg(x, y);
}

/* =====================================================================
   DESENHO — BLOCO "?"
   ===================================================================== */
void drawQBlock(int x, bool used, int bump, int coinPop) {
  int y = QB_Y - ((bump > 0) ? 4 : 0);

  if (coinPop > 0) {
    int cy = QB_Y - 12 - (12 - coinPop) * 3;
    drawCoin(x + QB_SIZE / 2, cy);
  }
  gfx->fillRoundRect(x, y, QB_SIZE, QB_SIZE, 3, used ? C_UBLOCK : C_YELLOW);
  gfx->drawRoundRect(x, y, QB_SIZE, QB_SIZE, 3, C_DARK);
  if (!used) {
    gfx->fillRect(x + 2, y + 2, 2, 2, C_DARK);
    gfx->fillRect(x + QB_SIZE - 4, y + 2, 2, 2, C_DARK);
    gfx->fillRect(x + 2, y + QB_SIZE - 4, 2, 2, C_DARK);
    gfx->fillRect(x + QB_SIZE - 4, y + QB_SIZE - 4, 2, 2, C_DARK);
    gfx->setTextColor(C_DARK);
    gfx->setTextSize(2);
    gfx->setCursor(x + 7, y + 5);
    gfx->print("?");
  }
}

/* =====================================================================
   DESENHO — PLANTA CARNIVORA e CANO
   ===================================================================== */
void drawPlant(int px, int pipeTop, int ext) {
  if (ext <= 2) return;
  int cx = px + PIPE_W / 2;
  int headY = pipeTop - ext + 7;
  gfx->fillRect(cx - 2, headY + 5, 5, ext, C_DGREEN);       // talo
  gfx->fillCircle(cx, headY, 8, C_MRED);                    // cabeca
  gfx->fillCircle(cx - 3, headY - 3, 2, C_WHITE);           // bolinhas
  gfx->fillCircle(cx + 4, headY - 1, 2, C_WHITE);
  gfx->fillRect(cx - 7, headY + 4, 14, 3, C_WHITE);         // boca
}

void drawPipe(int x, int h, bool hasPlant, int plantPhase) {
  int top = GROUND_Y - h;
  if (hasPlant) drawPlant(x, top, plantExt(plantPhase));    // planta ATRAS do cano
  gfx->fillRect(x + 3, top + 10, PIPE_W - 6, h - 10, C_GREEN);
  gfx->fillRect(x + 5, top + 10, 4, h - 10, C_LGREEN);
  gfx->drawRect(x + 3, top + 10, PIPE_W - 6, h - 10, C_DGREEN);
  gfx->fillRect(x, top, PIPE_W, 10, C_GREEN);
  gfx->fillRect(x + 3, top + 2, 6, 5, C_LGREEN);
  gfx->drawRect(x, top, PIPE_W, 10, C_DGREEN);
}

/* =====================================================================
   DESENHO — CENARIO por tema com parallax
   ===================================================================== */
int wrap360(int v) {
  v %= 360; if (v < 0) v += 360;
  return v - 60;
}

int themeNow() {
  return (worldNum - 1 + themeOffset) % 3;
}

void drawScenery() {
  int th = themeNow();

  gfx->fillScreen(skyCols[th]);

  if (th == 1) {
    gfx->fillCircle(170, 58, 14, C_YELLOW);
    gfx->fillCircle(170, 58, 10, 0xFE60);
  }
  if (th == 2) {
    gfx->fillCircle(170, 55, 12, 0xFFF9);
    gfx->fillCircle(176, 50, 11, skyCols[2]);
    for (int i = 0; i < 6; i++)
      gfx->fillRect(starX[i], starY[i], 2, 2, C_WHITE);
  }

  gfx->fillRoundRect(wrap360(148 - (int)cloudOff), 40, 42, 14, 7, cloudCols[th]);
  gfx->fillRoundRect(wrap360(54  - (int)cloudOff), 66, 32, 11, 5, cloudCols[th]);

  gfx->fillCircle(wrap360(38  - (int)hillOff), GROUND_Y + 6,  26, hillCols[th]);
  gfx->fillCircle(wrap360(200 - (int)hillOff), GROUND_Y + 10, 34, hillCols[th]);

  gfx->fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, C_BROWN);
  gfx->fillRect(0, GROUND_Y, SCREEN_W, 5, grassCols[th]);

  for (int gx = -(int)groundOff; gx < SCREEN_W; gx += 24) {
    gfx->fillRect(gx, GROUND_Y + 12, 4, 3, C_DARK);
    gfx->fillRect(gx + 12, GROUND_Y + 24, 4, 3, C_DARK);
  }
}

/* =====================================================================
   DESENHO — HUD: moedas, pontos, coracoes e banner
   ===================================================================== */
void drawHeart(int x, int y) {
  gfx->fillCircle(x + 2, y + 2, 3, C_MRED);
  gfx->fillCircle(x + 8, y + 2, 3, C_MRED);
  gfx->fillTriangle(x - 1, y + 4, x + 11, y + 4, x + 5, y + 10, C_MRED);
}

void drawHUD() {
  int th = themeNow();
  uint16_t txt = (th == 1) ? C_DARK : C_WHITE;

  gfx->fillEllipse(84, 31, 5, 7, C_YELLOW);
  gfx->drawEllipse(84, 31, 5, 7, C_COINDK);
  gfx->setTextColor(txt);
  gfx->setTextSize(2);
  gfx->setCursor(94, 24);
  gfx->print("x");
  gfx->print(coinCount);
  gfx->setTextSize(1);
  gfx->setCursor(96, 44);
  gfx->print(score);

  // coracoes (vidas)
  int hx = 120 - lives * 7;
  for (int i = 0; i < lives; i++)
    drawHeart(hx + i * 15, 54);

  if (bannerTimer > 0) {
    int len = strlen(bannerText);
    gfx->setTextSize(2);
    gfx->setCursor(120 - len * 6, 74);
    gfx->print(bannerText);
  }
}

/* =====================================================================
   RELOGIO via Wi-Fi (opcional) — define o tema inicial pela hora real
   ===================================================================== */
void syncThemeWithClock() {
  if (strlen(WIFI_SSID) == 0) return;   // desativado

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 5000) delay(100);

  if (WiFi.status() == WL_CONNECTED) {
    configTime(TZ_OFFSET, 0, "pool.ntp.org");
    struct tm ti;
    if (getLocalTime(&ti, 4000)) {
      int h = ti.tm_hour;
      if      (h >= 6 && h < 17) themeOffset = 0;   // dia
      else if (h < 19)           themeOffset = 1;   // por do sol
      else                       themeOffset = 2;   // noite
    }
  }
  WiFi.disconnect(true);    // desliga o radio: libera memoria e bateria
  WiFi.mode(WIFI_OFF);
}

/* =====================================================================
   SETUP
   ===================================================================== */
void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  gfx->begin();
  gfx->fillScreen(C_BLACK);
  if (strlen(WIFI_SSID) > 0) {
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(54, 116);
    gfx->print("acertando o relogio...");
  }
  gfx->flush();

  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  prefs.begin("mario", false);
  bestScore = prefs.getInt("best", 0);

  syncThemeWithClock();

  randomSeed(esp_random());
  resetGame();
}

/* =====================================================================
   LOOP PRINCIPAL
   ===================================================================== */
void loop() {
  unsigned long now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;
  frameTick++;

  bool tapped   = justTapped();
  bool touching = wasTouching;   // estado atual do dedo (pos justTapped)

  /* ---------- LOGICA ---------- */
  if (state == STATE_READY) {
    // titulo animado: cenario andando + Mario correndo
    groundOff = fmodf(groundOff + 2.0f, 24.0f);
    hillOff   = fmodf(hillOff  + 0.6f, 360.0f);
    cloudOff  = fmodf(cloudOff + 0.25f, 360.0f);
    if (frameTick % 3 == 0) runFrame ^= 1;
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }
  else if (state == STATE_PLAY) {
    if (tapped && onGround) {
      marioVel = riding ? -9.8f : JUMP_VEL;   // Yoshi pula mais forte!
      onGround = false;
    }
    // PULO CONTROLADO: soltou o dedo enquanto sobe = corta o pulo
    if (!touching && marioVel < -3.5f) marioVel = -3.5f;

    marioVel += GRAVITY;
    marioY   += marioVel;
    if (marioY >= GROUND_Y - MARIO_H) {
      marioY   = GROUND_Y - MARIO_H;
      marioVel = 0;
      onGround = true;
    }
    int legPeriod = (worldSpeed >= 5.0f) ? 2 : 3;
    if (onGround && (frameTick % legPeriod == 0)) runFrame ^= 1;
    if (invTimer  > 0) invTimer--;
    if (starTimer > 0) starTimer--;

    updateWorld();
  }
  else if (state == STATE_OVER) {
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }

  /* ---------- DESENHO ---------- */
  drawScenery();

  for (int i = 0; i < MAX_PIPES; i++)
    if (pipes[i].active)
      drawPipe((int)pipes[i].x, pipes[i].h, pipes[i].hasPlant, pipes[i].plantPhase);

  for (int i = 0; i < MAX_QBLOCKS; i++)
    if (qblocks[i].active)
      drawQBlock((int)qblocks[i].x, qblocks[i].used, qblocks[i].bump, qblocks[i].coinPop);

  for (int i = 0; i < MAX_GOOMBAS; i++)
    if (goombas[i].active) drawGoomba((int)goombas[i].x, goombas[i].squash > 0);

  for (int i = 0; i < MAX_KOOPAS; i++) {
    if (!koopas[i].active) continue;
    if (koopas[i].mode == 0) drawKoopaWalk((int)koopas[i].x);
    else                     drawShell((int)koopas[i].x, koopas[i].mode == 2);
  }

  for (int i = 0; i < MAX_BULLETS; i++)
    if (bullets[i].active) drawBullet((int)bullets[i].x, bullets[i].y);

  for (int i = 0; i < MAX_COINS; i++)
    if (coinsArr[i].active) drawCoin((int)coinsArr[i].x, coinsArr[i].y);

  for (int i = 0; i < MAX_ITEMS; i++)
    if (items[i].active) drawItem(i);

  if (yoshiFleeX > -999)
    drawYoshi((int)yoshiFleeX, GROUND_Y - 24);

  // Mario (+ Yoshi embaixo, se montado); pisca quando invencivel de dano
  if (invTimer == 0 || (invTimer % 4) < 2) {
    if (riding && state == STATE_PLAY) {
      drawYoshi(MARIO_X - 2, (int)marioY + 8);
      drawMario(MARIO_X, (int)marioY - 12, onGround ? runFrame : 2);
    } else {
      drawMario(MARIO_X, (int)marioY, onGround ? runFrame : 2);
    }
  }

  if (state == STATE_PLAY) {
    drawHUD();
  }
  else if (state == STATE_READY) {
    int th = themeNow();
    uint16_t txt = (th == 1) ? C_DARK : C_WHITE;
    gfx->setTextColor(txt);
    gfx->setTextSize(3);
    gfx->setCursor(75, 62);
    gfx->print("MARIO");
    gfx->setTextSize(2);
    gfx->setCursor(84, 92);
    gfx->print("RUNNER");
    drawCoin(62, 70);
    drawCoin(180, 70);
    if ((frameTick / 16) % 2 == 0) {          // texto piscando
      gfx->setTextSize(1);
      gfx->setCursor(72, 120);
      gfx->print("toque para jogar");
    }
    gfx->setTextSize(1);
    gfx->setCursor(70, 136);
    gfx->print("recorde: ");
    gfx->print(bestScore);
  }
  else { // STATE_OVER
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(38, 76);
    gfx->print("GAME OVER");
    gfx->setTextSize(1);
    gfx->setCursor(80, 102);
    gfx->print("pontos: ");
    gfx->print(score);
    gfx->setCursor(80, 114);
    gfx->print("moedas: ");
    gfx->print(coinCount);
    gfx->setCursor(80, 126);
    gfx->print("mundo: ");
    gfx->print(worldNum);
    gfx->setCursor(80, 138);
    gfx->print("recorde: ");
    gfx->print(bestScore);
    gfx->setCursor(46, 154);
    gfx->print("toque p/ tentar de novo");
  }

  gfx->flush();
}
