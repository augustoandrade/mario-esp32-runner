# 🍄 Mario Runner — ESP32 com tela redonda

Um mini-jogo estilo *endless runner* (como o dinossauro do Chrome) com
mecânicas de **Super Mario World**, rodando numa placa **ESP32-C3 com tela
redonda de 1.28"** e controlado 100% pelo **touch**.

Feito em um único sketch Arduino, sem imagens externas: todos os
personagens são **pixel art desenhada em código** (matrizes + paleta RGB565).

> 📸 *[adicione aqui uma foto ou GIF da placa rodando o jogo]*

## 🎮 Como se joga

| Ação | Como |
|------|------|
| Pular | Toque na tela — **toque curto** = pulo baixo, **segurar** = pulo alto |
| Cano verde | Desvie pulando ou **pouse em cima** (é plataforma!) |
| 🌱 Planta carnívora | Sobe e desce de alguns canos — olhe antes de pousar |
| 🍄 Goomba | Pise em cima pra esmagar (+20) |
| 🐢 Koopa | Pise 1x → vira casco; encoste no casco → **chute!** O casco destrói os inimigos à frente |
| 🚀 Bala Bill | Baixa = pule; alta = fique no chão (ou pise nela no ar!) |
| ❓ Bloco "?" | Bata **por baixo** → moeda, cogumelo, estrela ou ovo do Yoshi |
| 🪙 Moedas | +5 pts; a cada 30 moedas = **1UP** |
| 🍄 Cogumelo | Vira Super Mario com capa (aguenta 1 batida) |
| ⭐ Estrela | ~5s invencível — destrói TUDO, até canos |
| 🦖 Yoshi | Monte nele: pulo mais forte + proteção extra (ele foge quando você apanha, como no SNES) |

- **3 vidas** (corações no HUD), máximo 5 com 1UPs
- **Mundos**: a cada 150 pts o cenário muda — dia → pôr do sol → noite
- **Recorde salvo na flash** — sobrevive ao desligar
- A velocidade aumenta conforme você avança

## 🔩 Hardware

Placa **ESP32-2432S012** (também vendida como *JC ESP32-2424S012*) —
tudo-em-um, sem nenhuma solda ou fiação:

| Item  | Detalhe |
|-------|---------|
| MCU   | ESP32-C3-MINI-1U (RISC-V, 1 núcleo, 160 MHz) |
| RAM   | ~400 KB SRAM (sem PSRAM) |
| Flash | 4 MB |
| Tela  | 1.28" redonda IPS 240×240 — driver **GC9A01** (SPI) |
| Touch | **CST816** capacitivo (I2C, endereço 0x15) |
| Rádio | Wi-Fi 2.4 GHz + Bluetooth LE |

<details>
<summary>Pinagem interna da placa (já soldada)</summary>

| Sinal | GPIO |
|-------|------|
| TFT SCK | 6 |
| TFT MOSI | 7 |
| TFT DC | 2 |
| TFT CS | 10 |
| TFT RST | -1 (interno) |
| Backlight | 3 (HIGH = ligado) |
| Touch SDA | 4 |
| Touch SCL | 5 |

</details>

## 🛠️ Como compilar (Arduino IDE 2.x)

1. Instale o suporte a ESP32: **Settings → Additional boards manager URLs** →
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   e instale **esp32 (Espressif)** no Boards Manager.
2. Instale a biblioteca **GFX Library for Arduino** (moononournation) no
   Library Manager.
3. Configure em **Tools**:
   - Board: `ESP32C3 Dev Module`
   - USB CDC On Boot: `Enabled`
   - Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`
4. Abra `mario_esp32.ino` e clique em **Upload**.

> 💡 Se o upload falhar, segure o botão **BOOT** da placa ao conectar o
> USB e tente de novo.

### Opcional: cenário pelo horário real

Preencha `WIFI_SSID` / `WIFI_PASS` no topo do sketch (rede **2.4 GHz**).
Ao ligar, a placa acerta o relógio pela internet e o jogo começa com o
cenário da hora atual (dia / pôr do sol / noite). O Wi-Fi é desligado em
seguida — não gasta memória durante o jogo. Deixe `""` pra desativar.

## ⚙️ Como funciona por dentro

- **Sem flicker**: o quadro inteiro é desenhado num `Arduino_Canvas`
  (framebuffer de 240×240 na RAM, ~115 KB) e enviado de uma vez com
  `flush()` — roda a ~40 fps.
- **Sprites**: Mario, Goomba, Koopa, Yoshi e cogumelo são matrizes de
  pixel art (ex.: 16×16) com paleta RGB565, desenhadas ampliadas 2×.
- **Touch por polling**: o CST816 é lido a cada quadro via I2C
  (6 bytes a partir do registrador 0x01) — sem pino de interrupção.
- **Parallax**: nuvens (12%), colinas (30%) e chão (100%) rolam em
  velocidades diferentes.
- **Recorde**: gravado na NVS via `Preferences`, apenas quando melhora
  (pra poupar a flash).

## 🗺️ Ideias futuras

- [ ] Som (precisa de buzzer externo num GPIO livre)
- [ ] Suporte a controle Bluetooth (BLE — ex.: Xbox firmware 5.15+, via Bluepad32)

## 📜 Licença e créditos

Código sob licença [MIT](LICENSE). Este é um projeto de estudo/hobby,
feito do zero — **não** usa assets da Nintendo. Mario, Yoshi e Super
Mario World são marcas da Nintendo; este projeto não é afiliado nem
endossado por ela.

Desenvolvido com ajuda do [Claude Code](https://claude.com/claude-code). 🤖
