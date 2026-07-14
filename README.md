# Mario Runner — ESP32-2432S012 (tela redonda)

Mini-jogo estilo "dino do Chrome" com visual **Super Nintendo (Super Mario World)**:
o Mario corre sozinho e voce **toca na tela pra pular** os canos verdes.
Bateu no cano = game over; toque de novo pra recomecar.

![placa](https://img.shields.io) <!-- coloque uma foto da tela aqui se quiser -->

## Hardware

Placa **ESP32-2432S012** (JCZN / JC ESP32-2424S012) — tudo-em-um (MCU + tela + touch).

| Item  | Detalhe |
|-------|---------|
| MCU   | ESP32-C3-MINI-1U (RISC-V, 1 nucleo) |
| RAM   | ~400 KB SRAM · **sem PSRAM** |
| Tela  | 1.28" redonda IPS 240x240, driver **GC9A01** (SPI) |
| Touch | **CST816** capacitivo (I2C, 0x15) |
| Radio | Wi-Fi 2.4 GHz + **Bluetooth LE** (sem Bluetooth Classic) |

## Como compilar (Arduino IDE 2.x)

**Ferramentas (Tools):**
- Board: `ESP32C3 Dev Module`
- USB CDC On Boot: `Enabled`
- Partition Scheme: `Huge APP (3MB No OTA/1MB SPIFFS)`

**Biblioteca (Library Manager):**
- GFX Library for Arduino (moononournation)

Se o upload falhar: segure **BOOT** ao conectar o USB e tente de novo.

## Como jogar

- Tela inicial → toque para comecar
- **Pulo controlado**: toque curto = pulo baixinho; SEGURE = pulo alto
- **Cano verde**: desvie pulando ou POUSE em cima (plataforma!)
- **Planta carnivora**: sobe e desce de alguns canos — olhe antes de pousar!
- **Goomba**: pule EM CIMA pra esmagar (+20 pts) ou desvie
- **Koopa**: pise 1x → vira casco; encoste no casco → CHUTA! O casco
  sai na frente destruindo inimigos (+20 cada). Casco some ao bater em cano.
- **Bala Bill**: voa baixo (pule!) ou alto (fique no chao que ela passa) —
  tambem da pra pisar nela no ar!
- **Bloco "?"**: bata POR BAIXO → moeda, cogumelo, ESTRELA ou OVO DO YOSHI
- **Moedas**: +5 pts cada; a cada 30 moedas ganha **1UP** (vida extra)
- **Cogumelo**: Super Mario com capa — aguenta 1 batida
- **Estrela**: invencivel ~5s, cores piscando; destroi TUDO (ate canos!)
- **Yoshi**: monta nele — pulo mais forte + aguenta 1 batida
  (o Yoshi foge quando voce apanha, igualzinho ao SNES)
- **Vidas**: 3 coracoes no HUD (max 5 com 1UPs)
- **Mundos**: a cada 150 pts o cenario muda — dia → por do sol → noite
- **Recorde**: salvo na flash, sobrevive ao desligar a placa
- A velocidade aumenta conforme voce vence obstaculos

## Opcional: tema pelo horario real

Preencha `WIFI_SSID` / `WIFI_PASS` no topo do sketch (rede **2.4 GHz**).
Ao ligar, a placa acerta o relogio pela internet e o jogo comeca com o
cenario correspondente a hora (dia / por do sol / noite). O Wi-Fi e
desligado logo em seguida (nao gasta memoria durante o jogo).
Deixe `""` pra desativar (padrao).

## Detalhes tecnicos

- Sem GIF / sem Wi-Fi: Mario e canos desenhados na hora.
- Mario e um **sprite de pixel art 16x16** (matriz + paleta), ampliado 2x.
- **Arduino_Canvas** 240x240: desenha o quadro na RAM e envia de uma vez
  (`flush()`) -> sem flicker (~40 fps).
- Touch por **polling** do CST816.

## Ideias / proximas versoes

- [x] Moedas girando (pontos extras)
- [x] Inimigo goomba (com pisao + esmagado)
- [x] Power-up cogumelo -> capa (aguenta 1 hit)
- [x] Parallax (nuvens/colinas/chao em velocidades diferentes)
- [x] Bloco "?" pra bater por baixo (moeda/cogumelo/estrela/yoshi)
- [x] Koopa com casco chutavel
- [x] Recorde salvo na flash (Preferences)
- [x] Mundos com temas (dia / por do sol / noite)
- [x] Pulo controlado pela duracao do toque
- [x] Planta carnivora nos canos
- [x] Bala Bill (baixa e alta)
- [x] Estrela de invencibilidade
- [x] Yoshi montavel (foge ao apanhar)
- [x] Vidas (3 coracoes) + 1UP a cada 30 moedas
- [x] Titulo animado
- [x] Tema inicial pelo horario real (Wi-Fi/NTP opcional)
- [ ] Som (precisa de buzzer externo)
- [ ] Suporte a controle Bluetooth (BLE — ex.: Xbox firmware 5.15+)
