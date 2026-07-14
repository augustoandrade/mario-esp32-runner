/* =====================================================================
   MARIO RUNNER  —  ESP32-2432S012 (tela redonda 240x240, GC9A01)
   ---------------------------------------------------------------------
   Jogo estilo "dino do Chrome" com mecanicas de SUPER MARIO WORLD:

     - TOQUE na tela = pular
     - Canos verdes  = desviar (bateu = dano)
     - GOOMBA        = pule EM CIMA pra esmagar (+20 pts) ou desvie
     - KOOPA         = pise 1x -> vira casco; encoste no casco -> CHUTA!
                       o casco sai na frente destruindo inimigos (+20 cada)
     - BLOCO "?"     = bata POR BAIXO (pulando) -> solta moeda ou cogumelo
     - MOEDAS        = colete pra pontos (contador estilo SMW no topo)
     - COGUMELO      = vira SUPER MARIO com capa: aguenta 1 batida
     - MUNDOS        = a cada 150 pts muda o cenario:
                       dia -> por do sol -> noite (e repete)
     - RECORDE       = salvo na memoria flash (sobrevive ao desligar)

   Placa: ESP32-C3 (1 nucleo, ~400KB RAM, SEM PSRAM)
   Tela : GC9A01 240x240 SPI       Touch: CST816 I2C (0x15)

   >>> Ferramentas (Tools) no Arduino IDE:
       Board .............. ESP32C3 Dev Module
       USB CDC On Boot .... Enabled
       Partition Scheme ... Huge APP (3MB No OTA/1MB SPIFFS)

   >>> Bibliotecas (Library Manager):
       - GFX Library for Arduino  (moononournation)
       (Preferences e Wire ja vem no core ESP32)
   ===================================================================== */

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>

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
#define C_MRED    0xE185   // vermelho do Mario
#define C_SKIN    0xFDB8   // pele
#define C_MBLUE   0x22BC   // azul do macacao
#define C_BROWN   0x8A22   // marrom (cabelo, sapato, chao)
#define C_DARK    0x2124   // olho / sombra
#define C_GREEN   0x07E0   // verde do cano / casco
#define C_LGREEN  0x8FE0   // verde claro (brilho)
#define C_DGREEN  0x0480   // verde escuro
#define C_YELLOW  0xFFE0   // moeda / capa / bloco ?
#define C_COINDK  0xC300   // dourado escuro
#define C_GBODY   0xAA85   // corpo do goomba
#define C_GTAN    0xF652   // claro do goomba
#define C_KSKIN   0xFE60   // amarelo do koopa
#define C_UBLOCK  0x9284   // bloco "?" ja usado (marrom acinzentado)

/* ---------------------------------------------------------------------
   3) TIPOS (struct/enum SEMPRE no topo, antes das funcoes!)
   --------------------------------------------------------------------- */
enum GameState { STATE_READY, STATE_PLAY, STATE_OVER };

struct Pipe   { float x; int h; bool active; bool scored; };
struct Goomba { float x; bool active; bool scored; int squash; };
struct Koopa  { float x; bool active; bool scored; int mode; };  // 0 anda, 1 casco parado, 2 casco deslizando
struct Coin   { float x; int y; bool active; };
struct Shroom { float x; float baseY; bool active; };
struct QBlock { float x; bool active; bool used; int bump; int coinPop; };

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
   5) SPRITE DO GOOMBA (16x12, ampliado 2x -> 32x24)
        0=transp 1=corpo 2=claro 3=escuro 4=branco
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
   6) SPRITE DO KOOPA andando (16x15, ampliado 2x -> 32x30)
        0=transp 1=casco verde 2=pele amarela 3=escuro 4=branco 5=verde escuro
   --------------------------------------------------------------------- */
const uint16_t koopaPal[6] = { 0, C_GREEN, C_KSKIN, C_DARK, C_WHITE, C_DGREEN };

const uint8_t koopaSpr[15][16] = {
  {0,0,0,2,2,2,0,0,0,0,0,0,0,0,0,0},
  {0,0,2,2,2,2,2,0,0,0,0,0,0,0,0,0},
  {0,0,2,3,2,2,2,0,0,0,0,0,0,0,0,0},
  {0,0,2,2,2,2,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,2,2,0,0,1,1,1,1,1,0,0,0,0},
  {0,0,0,2,2,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,2,2,1,1,5,1,1,5,1,1,1,1,0,0},
  {0,0,2,2,1,1,1,1,1,1,1,1,1,1,1,0},
  {0,0,0,2,1,5,1,1,5,1,1,5,1,1,1,0},
  {0,0,0,2,1,1,1,1,1,1,1,1,1,1,1,0},
  {0,0,0,2,2,1,1,1,1,1,1,1,1,1,0,0},
  {0,0,0,0,2,4,4,4,4,4,4,4,4,0,0,0},
  {0,0,0,0,2,2,0,0,0,0,0,2,2,0,0,0},
  {0,0,0,2,2,2,0,0,0,0,0,2,2,2,0,0},
  {0,0,2,2,2,0,0,0,0,0,0,0,2,2,2,0},
};

/* ---------------------------------------------------------------------
   7) OBJETOS DE TELA
   --------------------------------------------------------------------- */
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
Arduino_GFX     *out = new Arduino_GC9A01(bus, TFT_RST, 0, true);
Arduino_Canvas  *gfx = new Arduino_Canvas(240, 240, out);

Preferences prefs;   // memoria flash pro recorde

/* ---------------------------------------------------------------------
   8) CONSTANTES DO MUNDO
   --------------------------------------------------------------------- */
const int SCREEN_W  = 240;
const int SCREEN_H  = 240;
const int GROUND_Y  = 178;
const int MARIO_X   = 44;
const int MARIO_W   = 32;
const int MARIO_H   = 32;
const int PIPE_W    = 26;
const int GO_W      = 32, GO_H = 24;    // goomba
const int KO_W      = 32, KO_H = 30;    // koopa andando
const int SH_W      = 24, SH_H = 16;    // casco
const int QB_SIZE   = 24;               // bloco "?"
const int QB_Y      = GROUND_Y - 94;    // topo do bloco (bater com a cabeca)

const float GRAVITY   = 0.65f;
const float JUMP_VEL  = -9.2f;

const int MAX_PIPES   = 3;
const int MAX_GOOMBAS = 3;
const int MAX_KOOPAS  = 2;
const int MAX_COINS   = 6;
const int MAX_QBLOCKS = 2;

// pontos pra trocar de mundo (dia -> por do sol -> noite)
const int WORLD_STEP = 150;

/* ---------------------------------------------------------------------
   9) TEMAS DOS MUNDOS (ceu, colina, nuvem, grama)
      0 = dia   1 = por do sol   2 = noite
   --------------------------------------------------------------------- */
const uint16_t skyCols[3]   = { 0x5D9F, 0xFCAC, 0x10C8 };
const uint16_t hillCols[3]  = { 0x0480, 0x52C3, 0x09E3 };
const uint16_t cloudCols[3] = { 0xFFFF, 0xFF39, 0xA534 };
const uint16_t grassCols[3] = { 0x07E0, 0x8DA7, 0x03E0 };

// estrelinhas da noite (posicoes fixas no ceu)
const uint8_t starX[6] = { 30, 90, 160, 210, 120, 60 };
const uint8_t starY[6] = { 60, 30,  50,  90,  14, 110 };

/* ---------------------------------------------------------------------
   10) VARIAVEIS DO JOGO
   --------------------------------------------------------------------- */
GameState state = STATE_READY;

float marioY;
float marioVel;
bool  onGround;
int   runFrame;

bool  superMario;
int   invTimer;

Pipe   pipes[MAX_PIPES];
Goomba goombas[MAX_GOOMBAS];
Koopa  koopas[MAX_KOOPAS];
Coin   coinsArr[MAX_COINS];
QBlock qblocks[MAX_QBLOCKS];
Shroom shroom;

float worldSpeed;
int   passedCount;
int   distSinceLast;
int   nextGap;
float coinDist;
int   coinGap;

int   score;
int   coinCount;
int   bestScore = 0;

int   worldNum     = 1;   // mundo atual (1, 2, 3...)
int   lastWorldNum = 1;
int   bannerTimer  = 0;   // mostra "MUNDO N" por alguns quadros

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
   RESET DO JOGO
   ===================================================================== */
void resetGame() {
  marioY   = GROUND_Y - MARIO_H;
  marioVel = 0;
  onGround = true;
  runFrame = 0;

  superMario = false;
  invTimer   = 0;

  for (int i = 0; i < MAX_PIPES;   i++) pipes[i].active    = false;
  for (int i = 0; i < MAX_GOOMBAS; i++) goombas[i].active  = false;
  for (int i = 0; i < MAX_KOOPAS;  i++) koopas[i].active   = false;
  for (int i = 0; i < MAX_COINS;   i++) coinsArr[i].active = false;
  for (int i = 0; i < MAX_QBLOCKS; i++) qblocks[i].active  = false;
  shroom.active = false;

  worldSpeed    = 3.0f;
  passedCount   = 0;
  distSinceLast = 0;
  nextGap       = 290;   // 1o obstaculo demora mais (tempo de se preparar)
  coinDist      = 0;
  coinGap       = 120;

  score        = 0;
  coinCount    = 0;
  worldNum     = 1;
  lastWorldNum = 1;
  bannerTimer  = 0;
}

/* =====================================================================
   DANO / GAME OVER (salva recorde na flash so quando melhora)
   ===================================================================== */
void takeHit() {
  if (invTimer > 0) return;
  if (superMario) {
    superMario = false;
    invTimer   = 45;
  } else {
    if (score > bestScore) {
      bestScore = score;
      prefs.putInt("best", bestScore);   // grava na flash
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
      // aquecimento: os 2 primeiros canos vem baixinhos
      int h = (passedCount < 2) ? random(26, 34) : random(26, 50);
      pipes[i] = { (float)(SCREEN_W + 10), h, true, false };
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

void spawnShroomAt(float x, float baseY) {
  shroom.active = true;
  shroom.x = x;
  shroom.baseY = baseY;
}

/* =====================================================================
   ATUALIZACAO DO MUNDO
   ===================================================================== */
void updateWorld() {
  // ---- linha do tempo de obstaculos ----
  distSinceLast += (int)worldSpeed;
  if (distSinceLast >= nextGap) {
    int r = random(0, 100);
    if      (passedCount >= 6 && r < 22) spawnKoopa();
    else if (passedCount >= 3 && r < 55) spawnGoomba();
    else                                  spawnPipe();

    if (!superMario && !shroom.active && invTimer == 0 && random(0, 100) < 10)
      spawnShroomAt(SCREEN_W + 70, GROUND_Y - 82);

    distSinceLast = 0;
    nextGap = random(115, 185);
  }

  // ---- moedas / blocos "?" em timeline separada ----
  coinDist += worldSpeed;
  if (coinDist >= coinGap) {
    if (random(0, 100) < 30) spawnQBlock();
    else                     spawnCoinArc();
    coinDist = 0;
    coinGap = random(150, 280);
  }

  // caixa do Mario
  int mL = MARIO_X + 5;
  int mR = MARIO_X + MARIO_W - 5;
  int mT = (int)marioY + 4;
  int mB = (int)marioY + MARIO_H;

  // ---- canos ----
  for (int i = 0; i < MAX_PIPES; i++) {
    if (!pipes[i].active) continue;
    pipes[i].x -= worldSpeed;

    if (!pipes[i].scored && pipes[i].x + PIPE_W < MARIO_X) {
      pipes[i].scored = true;
      score += 10;  passedCount++;
    }
    if (pipes[i].x < -30) { pipes[i].active = false; continue; }

    int pL = (int)pipes[i].x + 3;
    int pR = (int)pipes[i].x + PIPE_W - 3;
    int pT = GROUND_Y - pipes[i].h;
    if (mR > pL && mL < pR && mB > pT) takeHit();
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
      if (marioVel > 1.0f && mB < gT + 16) {       // pisao!
        goombas[i].squash = 10;
        goombas[i].scored = true;
        marioVel = JUMP_VEL * 0.75f;
        onGround = false;
        score += 20;  passedCount++;
      } else takeHit();
    }
  }

  // ---- koopas (anda / casco parado / casco deslizando) ----
  for (int i = 0; i < MAX_KOOPAS; i++) {
    if (!koopas[i].active) continue;

    if (koopas[i].mode == 0)      koopas[i].x -= worldSpeed + 0.3f;  // andando
    else if (koopas[i].mode == 1) koopas[i].x -= worldSpeed;         // casco parado (vai com o mundo)
    else                          koopas[i].x += 7.5f - worldSpeed;  // casco chutado: dispara pra frente

    // saiu da tela
    if (koopas[i].x < -45 || koopas[i].x > SCREEN_W + 20) {
      koopas[i].active = false;
      continue;
    }

    if (koopas[i].mode == 0 && !koopas[i].scored && koopas[i].x + KO_W < MARIO_X) {
      koopas[i].scored = true;
      score += 10;  passedCount++;
    }

    // casco deslizando DESTROI inimigos na frente
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
      // casco bate num cano -> some
      for (int p = 0; p < MAX_PIPES; p++) {
        if (!pipes[p].active) continue;
        if (sx + SH_W > pipes[p].x + 3 && sx < pipes[p].x + PIPE_W - 3) {
          koopas[i].active = false;
          break;
        }
      }
      continue;   // casco chutado nao machuca o Mario
    }

    // colisao com o Mario
    int kL = (int)koopas[i].x + 4;
    int kR = (int)koopas[i].x + ((koopas[i].mode == 0) ? KO_W : SH_W) - 4;
    int kT = GROUND_Y - ((koopas[i].mode == 0) ? KO_H : SH_H);
    if (mR > kL && mL < kR && mB > kT) {
      if (koopas[i].mode == 0) {
        if (marioVel > 1.0f && mB < kT + 16) {     // pisao -> vira casco
          koopas[i].mode = 1;
          koopas[i].scored = true;
          marioVel = JUMP_VEL * 0.75f;
          onGround = false;
          score += 20;  passedCount++;
        } else takeHit();
      } else {                                     // encostou no casco -> CHUTA!
        koopas[i].mode = 2;
        score += 10;
        if (marioVel > 1.0f) { marioVel = JUMP_VEL * 0.6f; onGround = false; }
      }
    }
  }

  // ---- blocos "?" ----
  for (int i = 0; i < MAX_QBLOCKS; i++) {
    if (!qblocks[i].active) continue;
    qblocks[i].x -= worldSpeed;
    if (qblocks[i].bump    > 0) qblocks[i].bump--;
    if (qblocks[i].coinPop > 0) qblocks[i].coinPop--;
    if (qblocks[i].x < -30) { qblocks[i].active = false; continue; }

    // cabecada por baixo: subindo + cabeca na altura da base do bloco
    if (!qblocks[i].used && marioVel < -2.0f) {
      int bL = (int)qblocks[i].x;
      int bR = (int)qblocks[i].x + QB_SIZE;
      int bBot = QB_Y + QB_SIZE;
      if (mR > bL + 3 && mL < bR - 3 && mT <= bBot + 4 && mT >= bBot - 8) {
        qblocks[i].used = true;
        qblocks[i].bump = 6;
        marioVel = 0.5f;                            // para de subir, comeca a cair
        if (!superMario && !shroom.active && random(0, 100) < 30) {
          spawnShroomAt(qblocks[i].x + 2, QB_Y - 22);  // premio: cogumelo!
        } else {
          qblocks[i].coinPop = 12;                  // premio: moeda pulando
          coinCount++;
          score += 5;
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
    }
  }

  // ---- cogumelo ----
  if (shroom.active) {
    shroom.x -= worldSpeed;
    if (shroom.x < -25) shroom.active = false;
    else {
      int sy = (int)(shroom.baseY + sinf(frameTick * 0.15f) * 3.0f);
      if (mR > shroom.x && mL < shroom.x + 20 && mB > sy && mT < sy + 20) {
        shroom.active = false;
        superMario = true;
        score += 10;
      }
    }
  }

  // ---- velocidade / mundo / parallax ----
  worldSpeed = 3.0f + passedCount * 0.1f;
  if (worldSpeed > 6.5f) worldSpeed = 6.5f;

  worldNum = 1 + score / WORLD_STEP;
  if (worldNum != lastWorldNum) {
    lastWorldNum = worldNum;
    bannerTimer = 45;                // mostra "MUNDO N"
  }
  if (bannerTimer > 0) bannerTimer--;

  groundOff = fmodf(groundOff + worldSpeed, 24.0f);
  hillOff   = fmodf(hillOff  + worldSpeed * 0.30f, 360.0f);
  cloudOff  = fmodf(cloudOff + worldSpeed * 0.12f, 360.0f);
}

/* =====================================================================
   DESENHO — MARIO (+ capa quando Super)
   ===================================================================== */
void drawMario(int x, int y, int frame) {
  if (superMario) {
    int flap = (frame == 2) ? 4 : (runFrame ? 2 : 0);
    gfx->fillTriangle(x + 8, y + 16, x - 4 - flap, y + 30, x + 10, y + 30, C_YELLOW);
  }
  for (int r = 0; r < 14; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = marioBody[r][c];
      if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2, marioPal[idx]);
    }
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = marioLegs[frame][r][c];
      if (idx) gfx->fillRect(x + c * 2, y + (14 + r) * 2, 2, 2, marioPal[idx]);
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
  for (int r = 0; r < 15; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = koopaSpr[r][c];
      if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2, koopaPal[idx]);
    }
}

void drawShell(int x, bool sliding) {
  int y = GROUND_Y - SH_H;
  gfx->fillRoundRect(x, y, SH_W, SH_H, 6, C_GREEN);
  gfx->drawRoundRect(x, y, SH_W, SH_H, 6, C_DGREEN);
  gfx->fillRoundRect(x + 2, y + SH_H - 6, SH_W - 4, 5, 3, C_WHITE);
  if (sliding) {          // riscos girando
    int o = (frameTick / 2) % 3;
    gfx->fillRect(x + 4 + o * 6, y + 3, 2, 7, C_DGREEN);
    gfx->fillRect(x + 10 + o * 4, y + 3, 2, 7, C_DGREEN);
  } else {
    gfx->fillRect(x + 8, y + 3, 2, 7, C_DGREEN);
    gfx->fillRect(x + 14, y + 3, 2, 7, C_DGREEN);
  }
}

/* =====================================================================
   DESENHO — MOEDA / COGUMELO / BLOCO "?"
   ===================================================================== */
void drawCoin(int cx, int cy) {
  static const int rxTab[4] = { 7, 5, 2, 5 };
  int rx = rxTab[(frameTick / 3) % 4];
  gfx->fillEllipse(cx, cy, rx, 8, C_YELLOW);
  gfx->drawEllipse(cx, cy, rx, 8, C_COINDK);
  if (rx > 4) gfx->fillRect(cx - 1, cy - 4, 2, 8, C_COINDK);
}

void drawShroom(int x, int y) {
  gfx->fillRoundRect(x, y, 20, 12, 5, C_MRED);
  gfx->fillCircle(x + 5,  y + 5, 2, C_WHITE);
  gfx->fillCircle(x + 14, y + 4, 2, C_WHITE);
  gfx->fillCircle(x + 10, y + 9, 2, C_WHITE);
  gfx->fillRect(x + 5, y + 12, 10, 8, C_SKIN);
  gfx->fillRect(x + 7, y + 14, 2, 3, C_DARK);
  gfx->fillRect(x + 11, y + 14, 2, 3, C_DARK);
}

void drawQBlock(int x, bool used, int bump, int coinPop) {
  int y = QB_Y - ((bump > 0) ? 4 : 0);       // da uma "pulada" quando batido

  if (coinPop > 0) {                          // moedinha saltando do bloco
    int cy = QB_Y - 12 - (12 - coinPop) * 3;
    drawCoin(x + QB_SIZE / 2, cy);
  }
  gfx->fillRoundRect(x, y, QB_SIZE, QB_SIZE, 3, used ? C_UBLOCK : C_YELLOW);
  gfx->drawRoundRect(x, y, QB_SIZE, QB_SIZE, 3, C_DARK);
  if (!used) {
    gfx->fillRect(x + 2, y + 2, 2, 2, C_DARK);              // rebites
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
   DESENHO — CANO
   ===================================================================== */
void drawPipe(int x, int h) {
  int top = GROUND_Y - h;
  gfx->fillRect(x + 3, top + 10, PIPE_W - 6, h - 10, C_GREEN);
  gfx->fillRect(x + 5, top + 10, 4, h - 10, C_LGREEN);
  gfx->drawRect(x + 3, top + 10, PIPE_W - 6, h - 10, C_DGREEN);
  gfx->fillRect(x, top, PIPE_W, 10, C_GREEN);
  gfx->fillRect(x + 3, top + 2, 6, 5, C_LGREEN);
  gfx->drawRect(x, top, PIPE_W, 10, C_DGREEN);
}

/* =====================================================================
   DESENHO — CENARIO por tema (dia / por do sol / noite) com parallax
   ===================================================================== */
int wrap360(int v) {
  v %= 360; if (v < 0) v += 360;
  return v - 60;
}

void drawScenery() {
  int th = (worldNum - 1) % 3;   // tema do mundo atual

  gfx->fillScreen(skyCols[th]);

  if (th == 1) {                 // por do sol: sol laranja
    gfx->fillCircle(170, 58, 14, C_YELLOW);
    gfx->fillCircle(170, 58, 10, 0xFE60);
  }
  if (th == 2) {                 // noite: lua crescente + estrelas
    gfx->fillCircle(170, 55, 12, 0xFFF9);
    gfx->fillCircle(176, 50, 11, skyCols[2]);   // recorte da lua
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
   DESENHO — HUD (moeda x N + pontos) e banner de mundo
   ===================================================================== */
void drawHUD() {
  int th = (worldNum - 1) % 3;
  uint16_t txt = (th == 1) ? C_DARK : C_WHITE;   // por do sol e claro

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

  if (bannerTimer > 0) {
    gfx->setTextSize(2);
    gfx->setCursor(72, 62);
    gfx->print("MUNDO ");
    gfx->print(worldNum);
  }
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
  gfx->flush();

  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  prefs.begin("mario", false);          // abre area "mario" na flash
  bestScore = prefs.getInt("best", 0);  // le o recorde salvo (0 se nao tem)

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

  bool tapped = justTapped();

  /* ---------- LOGICA ---------- */
  if (state == STATE_READY) {
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }
  else if (state == STATE_PLAY) {
    if (tapped && onGround) {
      marioVel = JUMP_VEL;
      onGround = false;
    }
    marioVel += GRAVITY;
    marioY   += marioVel;
    if (marioY >= GROUND_Y - MARIO_H) {
      marioY   = GROUND_Y - MARIO_H;
      marioVel = 0;
      onGround = true;
    }
    if (onGround && (frameTick % 4 == 0)) runFrame ^= 1;
    if (invTimer > 0) invTimer--;

    updateWorld();
  }
  else if (state == STATE_OVER) {
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }

  /* ---------- DESENHO ---------- */
  drawScenery();

  for (int i = 0; i < MAX_PIPES; i++)
    if (pipes[i].active) drawPipe((int)pipes[i].x, pipes[i].h);

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

  for (int i = 0; i < MAX_COINS; i++)
    if (coinsArr[i].active) drawCoin((int)coinsArr[i].x, coinsArr[i].y);

  if (shroom.active)
    drawShroom((int)shroom.x, (int)(shroom.baseY + sinf(frameTick * 0.15f) * 3.0f));

  if (invTimer == 0 || (invTimer % 4) < 2)
    drawMario(MARIO_X, (int)marioY, onGround ? runFrame : 2);

  if (state == STATE_PLAY) {
    drawHUD();
  }
  else if (state == STATE_READY) {
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(58, 88);
    gfx->print("MARIO");
    gfx->setTextSize(1);
    gfx->setCursor(40, 116);
    gfx->print("toque para jogar");
    gfx->setCursor(26, 130);
    gfx->print("pise nos inimigos, chute cascos");
    gfx->setCursor(30, 142);
    gfx->print("bata nos blocos ? por baixo!");
    gfx->setCursor(70, 156);
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
