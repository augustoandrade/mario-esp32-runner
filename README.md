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
- Toque em qualquer lugar da tela = pular
- A velocidade aumenta conforme voce pontua

## Detalhes tecnicos

- Sem GIF / sem Wi-Fi: Mario e canos desenhados na hora.
- Mario e um **sprite de pixel art 16x16** (matriz + paleta), ampliado 2x.
- **Arduino_Canvas** 240x240: desenha o quadro na RAM e envia de uma vez
  (`flush()`) -> sem flicker (~40 fps).
- Touch por **polling** do CST816.

## Ideias / proximas versoes

- [ ] Moedas girando (pontos extras)
- [ ] Som (se houver buzzer)
- [ ] Inimigo goomba
- [ ] Recorde salvo na flash (Preferences)
- [ ] Suporte a controle Bluetooth (BLE — ex.: Xbox firmware 5.15+)
