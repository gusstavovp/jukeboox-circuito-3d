# Jukeboox

Jukebox com ESP32, leitor RFID RC522 e DFPlayer Mini. Cada tag seleciona uma faixa ou playlist do cartão microSD. O painel da ESP32 mostra a faixa e permite ajustar o volume e avançar a reprodução.

## Onde encontrar cada parte

| Quero abrir | Link |
| --- | --- |
| Código da ESP32 | [Pasta do firmware atual](Jukeboox_Final/) |
| Circuito 3D e ligações | [Circuito 3D](https://gusstavovp.github.io/jukeboox-circuito-3d/) |
| Simulação de tags e falhas | [Simulador RFID](https://gusstavovp.github.io/jukeboox-circuito-3d/simulacao.html) |
| Demonstração do painel | [Volume e músicas](https://gusstavovp.github.io/jukeboox-circuito-3d/prototipo.html) |
| Documentação, testes e roteiro | [Entrega do projeto](https://gusstavovp.github.io/jukeboox-circuito-3d/entrega.html) |

## Gravar na ESP32

1. Baixe [Jukeboox_Final.ino](Jukeboox_Final/Jukeboox_Final.ino) e [Painel.h](Jukeboox_Final/Painel.h). Salve os dois na mesma pasta, chamada `Jukeboox_Final`.
2. Abra o `.ino` no Arduino IDE. Selecione **DOIT ESP32 DEVKIT V1** ou **ESP32 Dev Module** e instale as bibliotecas **MFRC522** e **DFRobotDFPlayerMini**.
3. Preencha `SENHA_WIFI` com uma senha de 8 a 63 caracteres e grave o sketch.
4. No microSD em FAT32, coloque `0001.mp3` a `0004.mp3` na pasta `MP3`.
5. Conecte-se à rede **Jukeboox** e abra [o painel da ESP32](http://192.168.4.1/).

| Tag RFID | Reprodução |
| --- | --- |
| `23 6B D3 BD` | Faixas 1, 2 e 3 |
| `95 A8 B8 89` | Faixa 4 |

A pasta `firmware/Jukeboox_Web` é de uma revisão anterior. Para uma nova gravação, comece pela pasta [Jukeboox_Final](Jukeboox_Final/).
