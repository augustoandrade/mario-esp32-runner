/* =====================================================================
   MARIO RUNNER  —  ESP32-2432S012 (tela redonda 240x240, GC9A01)
   ---------------------------------------------------------------------
   Jogo estilo "dino do Chrome": o Mario corre sozinho e voce TOCA
   na tela pra ele pular os canos verdes. Bateu no cano = game over.
   Toque de novo pra recomecar.

   >>> VISUAL estilo SUPER NINTENDO (Super Mario World):
       o Mario e um SPRITE de pixel art (matriz + paleta), desenhado
       ampliado 2x. Canos com sombreado e colinas no fundo.

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

/* ---------------------------------------------------------------------
   3) TIPOS (struct/enum SEMPRE no topo, antes das funcoes!)
   --------------------------------------------------------------------- */
enum GameState { STATE_READY, STATE_PLAY, STATE_OVER };

struct Pipe {
  float x;        // posicao horizontal (canto esquerdo)
  int   h;        // altura do cano
  bool  active;   // esta em uso?
  bool  scored;   // ja contou ponto ao passar pelo Mario?
};

/* ---------------------------------------------------------------------
   4) SPRITE DO MARIO  (pixel art 16x16, desenhado ampliado 2x -> 32x32)
      Cada numero e um indice de cor na paleta abaixo:
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
   5) OBJETOS DE TELA
   --------------------------------------------------------------------- */
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
Arduino_GFX     *out = new Arduino_GC9A01(bus, TFT_RST, 0 /*rotacao*/, true /*IPS*/);
// Canvas = desenhamos tudo na memoria e mandamos pra tela de uma vez (sem piscar)
Arduino_Canvas  *gfx = new Arduino_Canvas(240, 240, out);

/* ---------------------------------------------------------------------
   6) CONSTANTES DO MUNDO
   --------------------------------------------------------------------- */
const int SCREEN_W  = 240;
const int SCREEN_H  = 240;
const int GROUND_Y  = 178;   // linha do chao (onde os pes encostam)
const int MARIO_X   = 44;    // Mario fica parado nesse X (o mundo vem ate ele)
const int MARIO_W   = 32;    // sprite 16px x2
const int MARIO_H   = 32;
const int PIPE_W    = 26;    // largura do cano

const float GRAVITY   = 0.65f;  // puxa o Mario pra baixo
const float JUMP_VEL  = -9.2f;  // impulso do pulo (negativo = sobe)

/* ---------------------------------------------------------------------
   7) VARIAVEIS DO JOGO
   --------------------------------------------------------------------- */
GameState state = STATE_READY;

float marioY;        // topo do Mario (Y)
float marioVel;      // velocidade vertical
bool  onGround;
int   runFrame;      // alterna as perninhas correndo (0/1)

Pipe  pipes[4];      // ate 4 canos na tela
float worldSpeed;    // velocidade que o mundo se move
int   distSinceLast; // distancia desde o ultimo cano
int   nextGap;       // distancia ate soltar o proximo cano
int   score;
int   bestScore = 0;

bool  wasTouching = false;   // pra detectar o "clique" novo

unsigned long lastFrame = 0;
const unsigned long FRAME_MS = 24;   // ~40 quadros por segundo

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

// true APENAS no instante em que o dedo encosta (borda de subida)
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

  for (int i = 0; i < 4; i++) pipes[i].active = false;

  worldSpeed    = 3.0f;
  distSinceLast = 0;
  nextGap       = 140;
  score         = 0;
}

/* =====================================================================
   CANOS — nascimento e movimento
   ===================================================================== */
void spawnPipe() {
  for (int i = 0; i < 4; i++) {
    if (!pipes[i].active) {
      pipes[i].active = true;
      pipes[i].scored = false;
      pipes[i].x = SCREEN_W + 10;
      pipes[i].h = random(26, 50);
      return;
    }
  }
}

void updatePipes() {
  distSinceLast += (int)worldSpeed;
  if (distSinceLast >= nextGap) {
    spawnPipe();
    distSinceLast = 0;
    nextGap = random(105, 175);
  }

  for (int i = 0; i < 4; i++) {
    if (!pipes[i].active) continue;
    pipes[i].x -= worldSpeed;

    // passou pelo Mario -> ponto + acelera devagar (com limite)
    if (!pipes[i].scored && pipes[i].x + PIPE_W < MARIO_X) {
      pipes[i].scored = true;
      score++;
      worldSpeed = 3.0f + score * 0.12f;
      if (worldSpeed > 7.0f) worldSpeed = 7.0f;
    }

    if (pipes[i].x < -30) pipes[i].active = false;
  }
}

/* =====================================================================
   COLISAO  (retangulo do Mario x retangulo do cano)
   ===================================================================== */
bool hitPipe() {
  int mL = MARIO_X + 5;
  int mR = MARIO_X + MARIO_W - 5;
  int mT = (int)marioY + 4;
  int mB = (int)marioY + MARIO_H;

  for (int i = 0; i < 4; i++) {
    if (!pipes[i].active) continue;
    int pL = (int)pipes[i].x + 3;
    int pR = (int)pipes[i].x + PIPE_W - 3;
    int pT = GROUND_Y - pipes[i].h;
    int pB = GROUND_Y;

    if (mR > pL && mL < pR && mB > pT && mT < pB) return true;
  }
  return false;
}

/* =====================================================================
   DESENHO — MARIO (sprite ampliado 2x)  frame: 0/1 corre, 2 pula
   ===================================================================== */
void drawMario(int x, int y, int frame) {
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
   DESENHO — CANO VERDE (com sombreado)
   ===================================================================== */
void drawPipe(int x, int h) {
  int top = GROUND_Y - h;
  // corpo
  gfx->fillRect(x + 3, top + 10, PIPE_W - 6, h - 10, C_GREEN);
  gfx->fillRect(x + 5, top + 10, 4, h - 10, C_LGREEN);      // brilho vertical
  gfx->drawRect(x + 3, top + 10, PIPE_W - 6, h - 10, C_DGREEN);
  // boca (mais larga)
  gfx->fillRect(x, top, PIPE_W, 10, C_GREEN);
  gfx->fillRect(x + 3, top + 2, 6, 5, C_LGREEN);            // brilho na boca
  gfx->drawRect(x, top, PIPE_W, 10, C_DGREEN);
}

/* =====================================================================
   DESENHO — CENARIO (ceu, nuvens, colinas, chao)
   ===================================================================== */
void drawScenery() {
  gfx->fillScreen(C_SKY);

  // nuvens
  gfx->fillRoundRect(148, 40, 42, 14, 7, C_WHITE);
  gfx->fillRoundRect(54,  66, 32, 11, 5, C_WHITE);

  // colinas ao fundo (metade de baixo fica escondida pelo chao)
  gfx->fillCircle(38,  GROUND_Y + 6,  26, C_DGREEN);
  gfx->fillCircle(200, GROUND_Y + 10, 34, C_DGREEN);

  // chao
  gfx->fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, C_BROWN);
  gfx->fillRect(0, GROUND_Y, SCREEN_W, 5, C_GREEN);   // graminha em cima
}

/* =====================================================================
   DESENHO — PLACAR
   ===================================================================== */
void drawScore() {
  gfx->setTextColor(C_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(96, 26);
  gfx->print(score);
}

/* =====================================================================
   SETUP
   ===================================================================== */
void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  gfx->begin();               // o Canvas ja inicializa o display por dentro
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
  if (now - lastFrame < FRAME_MS) return;   // ritmo constante
  lastFrame = now;

  bool tapped = justTapped();

  /* ---------- LOGICA por estado ---------- */
  if (state == STATE_READY) {
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }
  else if (state == STATE_PLAY) {
    if (tapped && onGround) {          // pular
      marioVel = JUMP_VEL;
      onGround = false;
    }
    marioVel += GRAVITY;               // fisica
    marioY   += marioVel;
    if (marioY >= GROUND_Y - MARIO_H) {
      marioY   = GROUND_Y - MARIO_H;
      marioVel = 0;
      onGround = true;
    }
    static int frameCount = 0;         // anima perninhas so no chao
    if (onGround && (++frameCount % 4 == 0)) runFrame ^= 1;

    updatePipes();

    if (hitPipe()) {
      if (score > bestScore) bestScore = score;
      state = STATE_OVER;
    }
  }
  else if (state == STATE_OVER) {
    if (tapped) { resetGame(); state = STATE_PLAY; }
  }

  /* ---------- DESENHO ---------- */
  drawScenery();

  for (int i = 0; i < 4; i++)
    if (pipes[i].active) drawPipe((int)pipes[i].x, pipes[i].h);

  drawMario(MARIO_X, (int)marioY, onGround ? runFrame : 2);

  if (state == STATE_PLAY) {
    drawScore();
  }
  else if (state == STATE_READY) {
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(58, 92);
    gfx->print("MARIO");
    gfx->setTextSize(1);
    gfx->setCursor(40, 120);
    gfx->print("toque para jogar");
  }
  else { // STATE_OVER
    gfx->setTextColor(C_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(38, 84);
    gfx->print("GAME OVER");
    gfx->setTextSize(1);
    gfx->setCursor(80, 108);
    gfx->print("pontos: ");
    gfx->print(score);
    gfx->setCursor(80, 120);
    gfx->print("recorde: ");
    gfx->print(bestScore);
    gfx->setCursor(46, 136);
    gfx->print("toque p/ tentar de novo");
  }

  gfx->flush();   // manda o quadro pronto pra tela
}
