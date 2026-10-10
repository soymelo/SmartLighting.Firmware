# SmartLightingFirmware no PlatformIO

Firmware para duas placas ESP32 DevKit / ESP-WROOM-32 com Arduino Core ESP32 3.3.11.
Abra esta pasta no VS Code com a extensão PlatformIO. O `platformio.ini` fixa
`esp32dev`, framework Arduino e pioarduino `55.03.311` (Arduino Core 3.3.11).
`src_dir = .` inclui o sketch na raiz; `build_src_filter` seleciona esse `.ino`,
seu `.ino.cpp` gerado automaticamente e os módulos `.cpp` em `src/`.
O `src/main.cpp` de exemplo, criado pelo PlatformIO, permanece no projeto,
mas não participa da compilação.

No terminal, execute `pio run -e esp32dev` para compilar sem gravar a placa.
No VS Code, use **PlatformIO: Build** ou o ícone de marca de verificação na
barra inferior. O monitor serial está configurado para 115200 baud. Nenhuma
porta de upload está configurada neste projeto.

`SmartLightingFirmware.ino` contém `setup()`, `loop()` e a tarefa de rede.
`src/Models.h` define a topologia usada pelos módulos:

- `src/Hardware/`: leitura dos sensores e PWM das luminárias.
- `src/Control/`: acionamento por zonas e comunicação serial entre placas em D13/D14.
- `src/Services/`: Wi-Fi, topologia/cache e telemetria/heartbeat.

Os arquivos `.h` declaram as interfaces; os `.cpp` contêm as implementações.

## Configuração local e GitHub

`DeviceConfig.example.h` e `NetworkConfig.example.h` são modelos públicos, com
valores vazios. O código inclui `DeviceConfig.h` e `NetworkConfig.h`, que devem
existir localmente e estão no `.gitignore`. Depois de clonar o repositório,
crie as cópias no terminal PowerShell, na raiz do projeto:

```powershell
Copy-Item DeviceConfig.example.h DeviceConfig.h
Copy-Item NetworkConfig.example.h NetworkConfig.h
```

Preencha apenas as cópias locais. Use em `DeviceId` o ID do dispositivo
cadastrado no Web; em `ApiBaseUrl`, o endereço da API acessível pelo ESP32; e
em `BoardNumber`, `1` ou `2`. Configure o Wi-Fi em `NetworkConfig.h`. Os
exemplos vazios permitem compilar, mas não conectam a placa à rede nem à API.

Antes de cada commit, confira os arquivos preparados com `git status --short`.
Não adicione os headers locais com `git add -f` e não envie a pasta inteira
como ZIP: `.gitignore` protege apenas o fluxo normal do Git. A pasta `.pio/`
contém os binários gerados pelo build e também está ignorada; esses binários
podem incorporar valores das configurações locais.

## Configurar cada placa

1. Crie as cópias locais dos arquivos de exemplo, caso ainda não existam.
2. Em `DeviceConfig.h`, informe o ID do dispositivo cadastrado no Web e
   `BoardNumber = 1` ou `2`. Grave uma placa por vez com seu próprio ID.
3. Confirme que `ApiBaseUrl` aponta para o computador que executa a API.
4. Em `NetworkConfig.h`, informe o SSID 2.4 GHz e a senha. Não publique esse
   arquivo com a senha.
5. Se usar dois controladores, mantenha GND comum e ligue D13 da placa 1 a D13
   da placa 2; ligue D14 da placa 2 a D14 da placa 1. Cada ligação tem somente
   uma saída e uma entrada.
6. Selecione ESP32 Dev Module e monitore a serial em 115200 baud.

Sem ID ou Wi-Fi configurado, o sketch compila e avisa pela Serial, mas não
consulta a API. Na primeira inicialização, a placa precisa buscar a topologia
ao menos uma vez. Depois disso, usa a última topologia válida guardada na flash
quando a rede ou a API estiverem indisponíveis.

## Comportamento

- A API fornece `slot` e `gpioNumber`; o firmware usa o GPIO recebido.
  Antes de configurar um pino, ele valida se pertence ao conjunto de pinos
  testado para sensores ou LEDs e se não aparece duas vezes.
- Sensor ativo em LOW. A ativação precisa permanecer estável por 100 ms e a
  liberação por 1 s antes de mudar de estado. Um evento é criado somente na
  transição confirmada de inativo para ativo.
- A API informa as zonas do projeto com os sensores e LEDs participantes e o
  dispositivo de cada componente. O firmware configura somente os componentes
  ligados à sua placa, mas usa todos os sensores do projeto para decidir quais
  LEDs locais acender. Um LED em várias zonas é configurado uma única vez.
- Quando um sensor detecta movimento, sua placa envia o ID dele pela UART nos
  GPIOs 13 e 14; enquanto estiver ativo, repete esse ID a cada segundo e envia
  a liberação ao terminar. A outra placa aplica as mesmas zonas mesmo sem
  consultar a API a cada detecção.
- Um LED permanece no brilho máximo enquanto qualquer sensor de suas zonas
  estiver ativo ou dentro do prazo de acionamento. A perda das mensagens da
  outra placa encerra esse prazo, sem manter o LED aceso indefinidamente.
- PWM em 5 kHz, 8 bits: brilho base 20/255, máximo 255/255, fade de um
  passo a cada 10 ms. O brilho começa a voltar ao base 5 s após o último
  movimento.
- Wi-Fi, consulta de topologia e envio de telemetria rodam em outra tarefa;
  o controle local não espera uma requisição HTTP. Telemetria usa lotes de
  até 50 eventos e mantém na RAM os eventos pendentes para tentar novamente.
  O relógio UTC precisa sincronizar por NTP antes do envio.

## Verificação em bancada

1. Confira na Serial os IDs, slots e GPIOs da topologia de cada placa.
2. Desligue a API e reinicie a placa: a topologia local deve carregar e os
   LEDs devem continuar respondendo aos sensores.
3. Configure duas zonas sobrepostas: S1 aciona L1, L2 e L3; S2 aciona L3,
   L4 e L5. Ative cada sensor em LOW e confirme que só os LEDs configurados
   sobem de brilho. L3 deve permanecer aceso enquanto S1 ou S2 estiver ativo.
4. Libere o último sensor e espere 5 s: confira o retorno ao brilho base.
5. Configure uma zona com sensor e LED em placas diferentes. Teste D13 e D14
   nos dois sentidos com GND comum. Depois desconecte a comunicação e confirme
   que LEDs remotos voltam ao brilho base após 5 s.
6. Religue a API: confira heartbeat, SensorEvent, mudanças de brilho e
   ausência de eventos duplicados no dashboard.

Os testes elétricos, os dois sentidos de comunicação e a integração com a API
exigem as placas conectadas e seus IDs e dados de rede configurados.
