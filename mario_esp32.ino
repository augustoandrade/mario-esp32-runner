/* =====================================================================
   MARIO RUNNER  —  ESP32-2432S012 (tela redonda 240x240, GC9A01)
   ---------------------------------------------------------------------
   Jogo estilo "dino do Chrome" com mecanicas de SUPER MARIO WORLD:

     - TOQUE na tela = pular
     - Canos verdes  = desviar (bateu = dano)
     - GOOMBA        = pule EM CIMA pra esmagar (+20 pts) ou desvie
     - MOEDAS        = colete pra pontos (contador estilo SMW no topo)
     - COGUMELO      = vira SUPER MARIO com capa: aguenta 1 batida
                       (perde a capa e pisca invencivel por um instante)
     - Parallax: colinas e nuvens em velocidades diferentes (cara de SNES)

   Placa: ESP32-C3 (1 nucleo, ~400KB RAM, SEM PSRAM)
   Tela : GC9A01 240x240 SPI       Touch: CST816 I2C (0x15)

   >>> Ferramentas (Tools) no Arduino IDE:
       Board .............. ESP32C3 Dev Module
       USB CDC On Boot .... Enabled
       Partition Scheme ... Huge APP (3MB No OTA/1MB SPIFFS)

   >>> Bibliotecas (Library Manager):
       - GFX Library for Arduino  (moononournation)
   ===================================================================== */

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <math.h>

/* ---------------------------------------------------------------------
   1) PINOS (fixos, internos da placa — NAO mexer)
   --------------------------------------------------------------------- */
#define TFT_SCK   6
#define TFT_MOSI  7
#define TFT_DC    2
#define TFT_CS    10
#define TFT_RST   -1     // reset interno
#define TFT_BL    3      // backlight: ligar em HIGH

#define TOUCH_SDA 4
#define TOUCH_SCL 5
#define TOUCH_ADDR 0x15

/* ---------------------------------------------------------------------
   2) CORES  (Arduino_GFX nao define RED/BLUE — definimos na mao!)
      Formato RGB565.  Paleta "quentinha" estilo Super Nintendo.
   --------------------------------------------------------------------- */
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_MRED    0xE185   // vermelho do Mario (bone/camisa)
#define C_SKIN    0xFDB8   // tom de pele
#define C_MBLUE   0x22BC   // azul do macacao
#define C_BROWN   0x8A22   // marrom (cabelo, sapato, chao)
#define C_DARK    0x2124   // olho / sombra
#define C_GREEN   0x07E0   // verde do cano
#define C_LGREEN  0x8FE0   // verde claro (brilho do cano)
#define C_DGREEN  0x0480   // verde escuro (borda / colinas)
#define C_SKY     0x5D9F   // azul ceu
#define C_YELLOW  0xFFE0   // moeda / capa
#define C_COINDK  0xC300   // contorno da moeda (dourado escuro)
#define C_GBODY   0xAA85   // corpo do goomba (marrom avermelhado)
#define C_GTAN    0xF652   // "rosto" claro do goomba

/* ---------------------------------------------------------------------
   3) TIPOS (struct/enum SEMPRE no topo, antes das funcoes!)
   --------------------------------------------------------------------- */
enum GameState { STATE_READY, STATE_PLAY, STATE_OVER };

struct Pipe   { float x; int h; bool active; bool scored; };
struct Goomba { float x; bool active; bool scored; int squash; }; // squash>0 = esmagado (contando p/ sumir)
struct Coin   { float x; int y; bool active; };
struct Shroom { float x; float baseY; bool active; };

/* ---------------------------------------------------------------------
   4) SPRITE DO MARIO  (pixel art 16x16, ampliado 2x -> 32x32)
        0 = transparente   1 = vermelho   2 = pele   3 = marrom
        4 = azul           5 = escuro     6 = branco
   --------------------------------------------------------------------- */
const uint16_t marioPal[7] = { 0, C_MRED, C_SKIN, C_BROWN, C_MBLUE, C_DARK, C_WHITE };

// Corpo (linhas 0..13) — igual em todos os quadros
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

// Pernas (linhas 14..15) — muda por quadro: 0=corre A, 1=corre B, 2=pulo
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
        0 = transparente  1 = corpo  2 = claro  3 = escuro  4 = branco
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
   6) OBJETOS DE TELA
   --------------------------------------------------------------------- */
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
Arduino_GFX     *out = new Arduino_GC9A01(bus, TFT_RST, 0 /*rotacao*/, true /*IPS*/);
// Canvas = desenhamos tudo na memoria e mandamos pra tela de uma vez (sem piscar)
Arduino_Canvas  *gfx = new Arduino_Canvas(240, 240, out);

/* ---------------------------------------------------------------------
   7) CONSTANTES DO MUNDO
   --------------------------------------------------------------------- */
const int SCREEN_W  = 240;
const int SCREEN_H  = 240;
const int GROUND_Y  = 178;   // linha do chao
const int MARIO_X   = 44;    // Mario fica parado nesse X (o mundo vem ate ele)
const int MARIO_W   = 32;
const int MARIO_H   = 32;
const int PIPE_W    = 26;
const int GO_W      = 32;    // goomba
const int GO_H      = 24;

const float GRAVITY   = 0.65f;
const float JUMP_VEL  = -9.2f;

const int MAX_PIPES   = 3;
const int MAX_GOOMBAS = 3;
const int MAX_COINS   = 6;

/* ---------------------------------------------------------------------
   8) VARIAVEIS DO JOGO
   --------------------------------------------------------------------- */
GameState state = STATE_READY;

float marioY;
float marioVel;
bool  onGround;
int   runFrame;

bool  superMario;    // pegou cogumelo -> tem capa, aguenta 1 batida
int   invTimer;      // frames de invencibilidade (piscando) apos perder a capa

Pipe   pipes[MAX_PIPES];
Goomba goombas[MAX_GOOMBAS];
Coin   coinsArr[MAX_COINS];
Shroom shroom;

float worldSpeed;
int   passedCount;    // obstaculos vencidos (controla a velocidade)
int   distSinceLast;  // distancia desde o ultimo obstaculo
int   nextGap;
float coinDist;       // distancia desde o ultimo grupo de moedas
int   coinGap;

int   score;
int   coinCount;
int   bestScore = 0;

// parallax (fundo se move mais devagar que o chao — cara de SNES)
float hillOff = 0, cloudOff = 0, groundOff = 0;

bool  wasTouching = false;
unsigned long lastFrame = 0;
const unsigned long FRAME_MS = 24;   // ~40 quadros por segundo
uint32_t frameTick = 0;              // contador de quadros (animacoes)

/* =====================================================================
   TOUCH  — le o CST816 por polling (6 bytes a partir do reg 0x01)
   ===================================================================== */
bool isTouched() {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom(TOUCH_ADDR, 6);
  if (Wire.available() < 6) return false;

  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  return (d[1] > 0);   // reg 0x02 = numero de dedos
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

  for (int i = 0; i < MAX_PIPES;   i++) pipes[i].active   = false;
  for (int i = 0; i < MAX_GOOMBAS; i++) goombas[i].active = false;
  for (int i = 0; i < MAX_COINS;   i++) coinsArr[i].active = false;
  shroom.active = false;

  worldSpeed    = 3.0f;
  passedCount   = 0;
  distSinceLast = 0;
  nextGap       = 150;
  coinDist      = 0;
  coinGap       = 120;

  score     = 0;
  coinCount = 0;
}

/* =====================================================================
   DANO — decide se perde a capa ou se e game over
   ===================================================================== */
void takeHit() {
  if (invTimer > 0) return;          // piscando = intocavel
  if (superMario) {                  // tinha capa: perde ela e pisca
    superMario = false;
    invTimer   = 45;
  } else {                           // pequeno: fim de jogo
    if (score > bestScore) bestScore = score;
    state = STATE_OVER;
  }
}

/* =====================================================================
   SPAWN — canos, goombas, moedas e cogumelo
   ===================================================================== */
void spawnPipe() {
  for (int i = 0; i < MAX_PIPES; i++) {
    if (!pipes[i].active) {
      pipes[i].active = true;
      pipes[i].scored = false;
      pipes[i].x = SCREEN_W + 10;
      pipes[i].h = random(26, 50);
      return;
    }
  }
}

void spawnGoomba() {
  for (int i = 0; i < MAX_GOOMBAS; i++) {
    if (!goombas[i].active) {
      goombas[i].active = true;
      goombas[i].scored = false;
      goombas[i].squash = 0;
      goombas[i].x = SCREEN_W + 10;
      return;
    }
  }
}

// grupo de 3 moedas: arco alto (pegar pulando) ou fileira baixa (correndo)
void spawnCoinArc() {
  // se tem cano perto da borda direita, evita moedas baixas "dentro" dele
  bool pipeNear = false;
  for (int i = 0; i < MAX_PIPES; i++)
    if (pipes[i].active && pipes[i].x > SCREEN_W - 60) pipeNear = true;

  bool high = pipeNear ? true : (random(0, 2) == 0);
  float x0 = SCREEN_W + 20;

  int placed = 0;
  for (int i = 0; i < MAX_COINS && placed < 3; i++) {
    if (coinsArr[i].active) continue;
    coinsArr[i].active = true;
    if (high) {   // arco de pulo
      coinsArr[i].x = x0 + placed * 20;
      coinsArr[i].y = (placed == 1) ? GROUND_Y - 86 : GROUND_Y - 72;
    } else {      // fileira rente ao chao
      coinsArr[i].x = x0 + placed * 20;
      coinsArr[i].y = GROUND_Y - 26;
    }
    placed++;
  }
}

void spawnShroom() {
  shroom.active = true;
  shroom.x = SCREEN_W + 70;          // deslocado pra nao nascer sobre o cano
  shroom.baseY = GROUND_Y - 82;      // altura de pulo
}

/* =====================================================================
   ATUALIZACAO DO MUNDO
   ===================================================================== */
void updateWorld() {
  // ---- linha do tempo de obstaculos ----
  distSinceLast += (int)worldSpeed;
  if (distSinceLast >= nextGap) {
    // goomba comeca a aparecer depois de 3 obstaculos vencidos
    if (passedCount >= 3 && random(0, 100) < 40) spawnGoomba();
    else                                          spawnPipe();

    // cogumelo: chance rara, so se nao for Super ainda
    if (!superMario && !shroom.active && invTimer == 0 && random(0, 100) < 12)
      spawnShroom();

    distSinceLast = 0;
    nextGap = random(115, 185);
  }

  // ---- moedas em timeline separada ----
  coinDist += worldSpeed;
  if (coinDist >= coinGap) {
    spawnCoinArc();
    coinDist = 0;
    coinGap = random(150, 280);
  }

  // caixa do Mario (um pouco menor que o sprite, mais justo)
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
      score += 10;
      passedCount++;
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
    goombas[i].x -= worldSpeed + 0.4f;   // anda um tiquinho mais rapido que o mundo

    if (goombas[i].squash > 0) {         // esmagado: so conta o tempo e some
      if (--goombas[i].squash == 0) goombas[i].active = false;
      continue;
    }
    if (!goombas[i].scored && goombas[i].x + GO_W < MARIO_X) {
      goombas[i].scored = true;
      score += 10;
      passedCount++;
    }
    if (goombas[i].x < -40) { goombas[i].active = false; continue; }

    int gL = (int)goombas[i].x + 4;
    int gR = (int)goombas[i].x + GO_W - 4;
    int gT = GROUND_Y - GO_H;
    if (mR > gL && mL < gR && mB > gT) {
      // caindo por cima = PISAO (esmaga e quica)
      if (marioVel > 1.0f && mB < gT + 16) {
        goombas[i].squash = 10;
        goombas[i].scored = true;
        marioVel = JUMP_VEL * 0.75f;     // quique
        onGround = false;
        score += 20;
        passedCount++;
      } else {
        takeHit();
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
      int sy = (int)(shroom.baseY + sinf(frameTick * 0.15f) * 3.0f);  // flutua
      if (mR > shroom.x && mL < shroom.x + 20 && mB > sy && mT < sy + 20) {
        shroom.active = false;
        superMario = true;           // GANHOU A CAPA!
        score += 10;
      }
    }
  }

  // ---- velocidade sobe com obstaculos vencidos (com limite) ----
  worldSpeed = 3.0f + passedCount * 0.1f;
  if (worldSpeed > 6.5f) worldSpeed = 6.5f;

  // ---- parallax ----
  groundOff = fmodf(groundOff + worldSpeed, 24.0f);
  hillOff   = fmodf(hillOff  + worldSpeed * 0.30f, 360.0f);
  cloudOff  = fmodf(cloudOff + worldSpeed * 0.12f, 360.0f);
}

/* =====================================================================
   DESENHO — MARIO (frame: 0/1 corre, 2 pula) + capa quando Super
   ===================================================================== */
void drawMario(int x, int y, int frame) {
  // capa amarela ATRAS do corpo (desenhada antes do sprite)
  if (superMario) {
    int flap = (frame == 2) ? 4 : (runFrame ? 2 : 0);   // capa "balanca"
    gfx->fillTriangle(x + 8, y + 16, x - 4 - flap, y + 30, x + 10, y + 30, C_YELLOW);
  }
  // corpo
  for (int r = 0; r < 14; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = marioBody[r][c];
      if (idx) gfx->fillRect(x + c * 2, y + r * 2, 2, 2, marioPal[idx]);
    }
  // pernas
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 16; c++) {
      uint8_t idx = marioLegs[frame][r][c];
      if (idx) gfx->fillRect(x + c * 2, y + (14 + r) * 2, 2, 2, marioPal[idx]);
    }
}

/* =====================================================================
   DESENHO — GOOMBA (squashed = achatado no chao)
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
    int y = GROUND_Y - 12;           // panqueca: metade da altura
    for (int r = 0; r < 12; r++)
      for (int c = 0; c < 16; c++) {
        uint8_t idx = goombaSpr[r][c];
        if (idx) gfx->fillRect(x + c * 2, y + r, 2, 1, goombaPal[idx]);
      }
  }
}

/* =====================================================================
   DESENHO — MOEDA girando (a largura oscila = parece girar)
   ===================================================================== */
void drawCoin(int cx, int cy) {
  static const int rxTab[4] = { 7, 5, 2, 5 };
  int rx = rxTab[(frameTick / 3) % 4];
  gfx->fillEllipse(cx, cy, rx, 8, C_YELLOW);
  gfx->drawEllipse(cx, cy, rx, 8, C_COINDK);
  if (rx > 4) gfx->fillRect(cx - 1, cy - 4, 2, 8, C_COINDK);  // risco central
}

/* =====================================================================
   DESENHO — COGUMELO
   ===================================================================== */
void drawShroom(int x, int y) {
  gfx->fillRoundRect(x, y, 20, 12, 5, C_MRED);        // chapeu
  gfx->fillCircle(x + 5,  y + 5, 2, C_WHITE);         // bolinhas
  gfx->fillCircle(x + 14, y + 4, 2, C_WHITE);
  gfx->fillCircle(x + 10, y + 9, 2, C_WHITE);
  gfx->fillRect(x + 5, y + 12, 10, 8, C_SKIN);        // "rostinho"
  gfx->fillRect(x + 7, y + 14, 2, 3, C_DARK);         // olhos
  gfx->fillRect(x + 11, y + 14, 2, 3, C_DARK);
}

/* =====================================================================
   DESENHO — CANO VERDE (com sombreado)
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
   DESENHO — CENARIO com parallax (nuvens 12%, colinas 30%, chao 100%)
   ===================================================================== */
int wrap360(int v) {          // mantem a posicao dando a volta na tela
  v %= 360; if (v < 0) v += 360;
  return v - 60;
}

void drawScenery() {
  gfx->fillScreen(C_SKY);

  // nuvens (bem devagar)
  gfx->fillRoundRect(wrap360(148 - (int)cloudOff), 40, 42, 14, 7, C_WHITE);
  gfx->fillRoundRect(wrap360(54  - (int)cloudOff), 66, 32, 11, 5, C_WHITE);

  // colinas (devagar) — metade fica escondida pelo chao
  gfx->fillCircle(wrap360(38  - (int)hillOff), GROUND_Y + 6,  26, C_DGREEN);
  gfx->fillCircle(wrap360(200 - (int)hillOff), GROUND_Y + 10, 34, C_DGREEN);

  // chao
  gfx->fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, C_BROWN);
  gfx->fillRect(0, GROUND_Y, SCREEN_W, 5, C_GREEN);   // graminha

  // marquinhas rolando no chao (sensacao de velocidade)
  for (int gx = -(int)groundOff; gx < SCREEN_W; gx += 24) {
    gfx->fillRect(gx, GROUND_Y + 12, 4, 3, C_DARK);
    gfx->fillRect(gx + 12, GROUND_Y + 24, 4, 3, C_DARK);
  }
}

/* =====================================================================
   DESENHO — HUD estilo SMW (moeda x N + pontos)
   ===================================================================== */
void drawHUD() {
  gfx->fillEllipse(84, 31, 5, 7, C_YELLOW);
  gfx->drawEllipse(84, 31, 5, 7, C_COINDK);
  gfx->setTextColor(C_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(94, 24);
  gfx->print("x");
  gfx->print(coinCount);
  gfx->setTextSize(1);
  gfx->setCursor(96, 44);
  gfx->print(score);
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

  /* ---------- LOGICA por estado ---------- */
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

    updateWorld();   // move tudo, colisoes, dano
  }
  else if (state == STATE_OVER) {
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }

  /* ---------- DESENHO ---------- */
  drawScenery();

  for (int i = 0; i < MAX_PIPES; i++)
    if (pipes[i].active) drawPipe((int)pipes[i].x, pipes[i].h);

  for (int i = 0; i < MAX_GOOMBAS; i++)
    if (goombas[i].active) drawGoomba((int)goombas[i].x, goombas[i].squash > 0);

  for (int i = 0; i < MAX_COINS; i++)
    if (coinsArr[i].active) drawCoin((int)coinsArr[i].x, coinsArr[i].y);

  if (shroom.active)
    drawShroom((int)shroom.x, (int)(shroom.baseY + sinf(frameTick * 0.15f) * 3.0f));

  // Mario pisca quando invencivel (some a cada 2 quadros)
  if (invTimer == 0 || (invTimer % 4) < 2)
    drawMario(MARIO_X, (int)marioY, onGround ? runFrame : 2);

  if (state == STATE_PLAY) {
    drawHUD();
  }
  else if (state == STATE_READY) {
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(58, 92);
    gfx->print("MARIO");
    gfx->setTextSize(1);
    gfx->setCursor(40, 120);
    gfx->print("toque para jogar");
    gfx->setCursor(30, 134);
    gfx->print("pule nos goombas, pegue moedas");
    gfx->setCursor(38, 146);
    gfx->print("cogumelo = capa (1 vida)");
  }
  else { // STATE_OVER
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(38, 80);
    gfx->print("GAME OVER");
    gfx->setTextSize(1);
    gfx->setCursor(80, 106);
    gfx->print("pontos: ");
    gfx->print(score);
    gfx->setCursor(80, 118);
    gfx->print("moedas: ");
    gfx->print(coinCount);
    gfx->setCursor(80, 130);
    gfx->print("recorde: ");
    gfx->print(bestScore);
    gfx->setCursor(46, 146);
    gfx->print("toque p/ tentar de novo");
  }

  gfx->flush();
}
