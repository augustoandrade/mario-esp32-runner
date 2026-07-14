# Testar no simulador Wokwi (sem a placa!)

O [Wokwi](https://wokwi.com) simula o ESP32-C3 e a tela redonda GC9A01
direto no navegador. O touch CST816 nao existe no simulador, entao o
jogo tambem aceita um **botao** como pulo (no simulador = botao virtual
"PULAR"; na placa real = botao BOOT).

## Passo a passo

1. Abra <https://wokwi.com/projects/new/esp32-c3> (crie conta gratis se pedir)
2. Na aba **sketch.ino**: apague o conteudo e cole TODO o conteudo de
   `mario_esp32.ino`
3. Na aba **diagram.json**: apague e cole o conteudo de `wokwi/diagram.json`
4. Crie um arquivo novo chamado **libraries.txt** (botao "+" nas abas) e
   cole o conteudo de `wokwi/libraries.txt`
5. Aperte o **play** (botao verde) e aguarde compilar (~1 min na 1a vez)
6. Clique no botao vermelho **PULAR** pra jogar!

## Limitacoes do simulador

- Sem touch: so o botao PULAR funciona
- Pode rodar mais devagar que a placa real (o jogo fica em "camera lenta"
  se o navegador estiver pesado) — o timing real so na placa
- Cores podem variar um pouco em relacao a tela IPS fisica
- Wi-Fi/NTP: o Wokwi ate simula internet, mas deixe WIFI_SSID vazio pra
  nao atrasar o boot

## Dica

O botao BOOT da placa real tambem pula agora! Util pra testar sem
engordurar a tela. :)
