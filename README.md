# SmartLightingFirmware no PlatformIO

Firmware para duas placas ESP32 DevKit / ESP-WROOM-32 com Arduino Core ESP32 3.3.11.
Abra esta pasta no VS Code com a extensão PlatformIO. O `platformio.ini` fixa
`esp32dev`, framework Arduino e pioarduino `55.03.311` (Arduino Core 3.3.11).
`src_dir = .` inclui o programa na raiz; `build_src_filter` seleciona
`SmartLightingFirmware.cpp` e os módulos `.cpp` em `src/`.

No terminal, execute `pio run -e esp32dev` para compilar sem gravar a placa.
No VS Code, use **PlatformIO: Build** ou o ícone de marca de verificação na
barra inferior. O monitor serial está configurado para 115200 baud. Nenhuma
porta de upload está configurada neste projeto.

`SmartLightingFirmware.cpp` contém `setup()`, `loop()` e a tarefa de rede.
`src/Models.h` define a topologia usada pelos módulos:

- `src/Hardware/`: leitura dos sensores e PWM das luminárias.
- `src/Control/`: acionamento por zonas e comunicação serial entre placas em D13/D14.
- `src/Services/`: Wi-Fi, topologia/cache e telemetria/heartbeat.

Os arquivos `.h` declaram as interfaces; os `.cpp` contêm as implementações.

### Zonas na topologia e cache local

A resposta de topologia da API já contém zonas com seus sensores e luminárias.
`TopologyService` agora guarda também, para cada zona, seu ID, uma máscara dos
sensores do projeto e uma máscara dos LEDs **desta placa**. Esse mapa permite
contar ativações por zona e combinar seus pedidos de repouso. A máscara que
liga sensores aos LEDs continua atendendo ao acionamento imediato. O limite
atual é de 12 zonas, seis sensores do projeto, três sensores e seis LEDs por
ESP32. São limites de armazenamento do firmware, não IDs ou vínculos fixos da
maquete.

Topologias novas são gravadas sob a chave `topology-v3` na flash. Ao iniciar,
o firmware tenta ler v3. Se encontrar apenas o cache antigo `topology-v2`,
recupera os sensores, LEDs e vínculos de acionamento imediato, com zero zonas
disponíveis para ML. Assim os sensores continuam funcionando em PWM de repouso
fixo 20 mesmo sem API. Quando a API voltar, a topologia completa é recebida e
salva em v3; o cache v2 permanece disponível como alternativa.

`ZoneActivityCounter` conta uma ativação por início de detecção e associa o
sensor às zonas da topologia. Cada placa registra suas detecções locais e envia
uma mensagem `E` por nova ativação à outra placa via UART. O receptor registra
`E` para o sensor remoto correspondente. As mensagens existentes `A` (ativo,
repetida enquanto houver movimento) e `R` (liberado) continuam controlando a
iluminação imediata e não entram na contagem. As duas placas precisam executar
esta versão para que as ativações remotas também sejam contadas.

`ZoneActivityCounter` mantém o total desde a última aplicação da topologia e,
se o relógio UTC estiver válido, separa as ativações por zona em janelas de
15 minutos. Guarda somente as duas últimas janelas completas, inclusive as
vazias, para fornecer a soma dos 30 minutos anteriores. A janela que já estava
em andamento ao ligar a placa ou sincronizar o relógio é descartada por ser
parcial. Se o relógio ficar inválido ou mudar de forma descontínua, o histórico
das janelas é reiniciado; o total acumulado continua disponível. Uma nova
topologia reinicia tanto o total quanto as janelas.

`ActivityClock` só aceita horário UTC a partir de 2025-01-01, como a validação
existente da telemetria. Converte a hora de cada janela para UTC-3 fixo, o fuso
usado nos cenários que treinaram os pesos atuais, e converte o dia da semana
para 0=segunda até 6=domingo. O monitor serial mostra a hora local, a contagem
da janela encerrada e, após duas janelas completas, a soma dos 30 minutos.
Se a placa iniciar às 19h08, 19h00–19h15 é parcial; as primeiras janelas
completas serão 19h15–19h30 e 19h30–19h45. A contagem de 30 minutos só estará
disponível às 19h45. A API não é consultada para fechar janelas.

O ponto de entrada é um `.cpp` comum, sem a conversão temporária de `.ino`.
Isso permite compilações incrementais sem depender de um arquivo gerado na raiz.

## Pesos preparados para o ML

`src/Control/ActivityModelWeights.h` contém os 27 pesos do modelo geral
escolhido no projeto `SmartLighting.ML`. Ele é **gerado** de
`SmartLighting.ML/config/firmware_model.json`, não editado manualmente.
Na raiz de `SmartLighting.ML`, regenere após uma mudança deliberada do modelo:

```powershell
$env:PYTHONPATH = "src"
.\.venv\Scripts\python.exe -m smartlighting_ml.export_cpp_weights --output ..\SmartLighting.Firmware\src\Control\ActivityModelWeights.h
```

O comentário no header traz o SHA-256 dos JSONs de modelo e política; cada
peso tem o nome da entrada correspondente. `ActivityPredictor::predict()`, em
`src/Control/ActivityPredictor.cpp`, recebe hora local (0–23), dia da semana
(0=segunda, 6=domingo) e ativações dos últimos 30 minutos. Devolve a média
prevista, a chance de atividade nos próximos 15 minutos e PWM 10/20/40.
Entradas de hora ou dia inválidas são rejeitadas. A fórmula reproduz o modelo
Python sem consultar a API nem ler JSON na placa.

`AdaptiveRestController` executa `ActivityPredictor::predict()` no início de
cada janela completa, usando a hora local, o dia da semana e as duas janelas
encerradas da zona. A chance prevista define PWM 10, 20 ou 40 para o repouso
dos próximos 15 minutos. Cada LED local recebe o **maior** pedido das zonas às
quais pertence. Um LED sem zona associada usa PWM 20. Uma zona sem sensor,
sem duas janelas completas ou sem horário válido pede PWM 20. Após uma mudança
de topologia ou do relógio, o repouso volta a 20 até haver histórico válido.
O sensor continua tendo prioridade: quando há movimento, o alvo é PWM 255;
o fade de um ponto a cada 10 ms continua valendo para todas as transições.

Exemplo às 03h: Z01 com zero ativações recentes pede PWM 10; Z02 com três
ativações pede PWM 20. Um LED compartilhado pelas duas fica em PWM 20. Às
18h, o modelo revisado pede PWM 20 com zero ativações recentes no caso de
referência; às 19h, com uma ativação nos 30 minutos anteriores, pede PWM 40.
O horário também entra na previsão. A API não é consultada para tomar
essas decisões depois que a topologia estiver disponível na placa.

Para conferir o cálculo fora da placa, a partir da raiz deste projeto em um
terminal Ubuntu/WSL com `g++` e `python3`:

```bash
mkdir -p .pio
g++ -std=c++17 -O2 -Wall -Wextra -Werror src/Control/ActivityPredictor.cpp test/activity_predictor_cli.cpp -o .pio/activity_predictor_native
python3 ../SmartLighting.ML/src/smartlighting_ml/verify_cpp_predictor.py --reference ../SmartLighting.ML/config/firmware_reference.json --program .pio/activity_predictor_native
```

O programa de teste em `test/` não entra no firmware. Ele executa os oito casos
congelados em `firmware_reference.json`; a verificação exige probabilidades
próximas (tolerância de 1e-9) e PWM idêntico. `pio run -e esp32dev` verifica
também a compilação para a placa, mas não executa o modelo fisicamente.

Para conferir o histórico das janelas no WSL, sem esperar 30 minutos reais:

```bash
mkdir -p .pio
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itest/support src/Control/ActivityClock.cpp src/Control/ZoneActivityCounter.cpp test/zone_activity_counter_test.cpp -o .pio/zone_activity_counter_test
.pio/zone_activity_counter_test
```

Esse teste verifica início parcial, janela vazia, soma das duas janelas,
ausência de contagem futura e reinício após mudança do relógio.

Para conferir a integração entre histórico, modelo e LEDs compartilhados:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itest/support src/Control/ActivityClock.cpp src/Control/ActivityPredictor.cpp src/Control/ZoneActivityCounter.cpp src/Control/AdaptiveRestController.cpp test/adaptive_rest_controller_test.cpp -o .pio/adaptive_rest_controller_test
.pio/adaptive_rest_controller_test
```

Esse teste verifica PWM padrão sem histórico ou horário válido, PWM baixo e
padrão em zonas distintas, maior pedido no LED compartilhado e PWM alto em
horário de pico.

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

Sem ID ou Wi-Fi configurado, o programa compila e avisa pela Serial, mas não
consulta a API. Na primeira inicialização, a placa precisa buscar a topologia
ao menos uma vez. Depois disso, usa a última topologia válida guardada na flash
quando a rede ou a API estiverem indisponíveis.

### Monitorar as duas placas

Com ambas conectadas por USB, liste as portas COM no PowerShell. Se houver
dúvida sobre qual porta pertence a cada placa, desconecte uma, liste novamente
e observe qual porta desapareceu:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device list
```

Abra dois terminais, cada um com uma porta diferente:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM5 -b 115200
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor -p COM6 -b 115200
```

Substitua `COM5` e `COM6` pelas portas encontradas. Confirme que os monitores
mostram **DeviceIds diferentes**. Dois trechos com a mesma porta e o mesmo
DeviceId são leituras da mesma placa; não validam a comunicação entre placas.
Feche o monitor da porta antes de gravar o respectivo ESP32.

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
- Quando um sensor inicia uma detecção, sua placa envia `E` com o ID dele pela
  UART nos GPIOs 13 e 14 para a contagem remota. As mensagens `A` e `R`
  continuam indicando atividade e liberação; `A` é repetida a cada segundo
  enquanto o sensor estiver ativo. A outra placa aplica as mesmas zonas sem
  consultar a API a cada detecção.
- Um LED permanece no brilho máximo enquanto qualquer sensor de suas zonas
  estiver ativo ou dentro do prazo de acionamento. A perda das mensagens da
  outra placa encerra esse prazo, sem manter o LED aceso indefinidamente.
- PWM em 5 kHz, 8 bits: repouso adaptativo 10/255, 20/255 ou 40/255;
  máximo 255/255 durante o acionamento. O alvo de repouso volta a valer 5 s
  após o último movimento, com fade de um passo a cada 10 ms.
- Wi-Fi, consulta de topologia e envio de telemetria rodam em outra tarefa;
  o controle local não espera uma requisição HTTP. Telemetria usa lotes de
  até 50 eventos e mantém na RAM os eventos pendentes para tentar novamente.
  O relógio UTC precisa sincronizar por NTP antes do envio.

## Verificação em bancada

1. Com a API disponível, confira na Serial os IDs, slots e GPIOs da topologia
   recebida por cada placa.
2. Ative um sensor em LOW e confirme que os LEDs das suas zonas sobem de brilho.
   A placa local e a remota devem contar uma ativação cada, sem contar as
   mensagens `A` repetidas como novas passagens.
3. Em zonas sobrepostas, confirme que o LED compartilhado responde a qualquer
   sensor associado. Libere o último sensor e espere 5 s para observar o fade
   até o repouso vigente.
4. Teste um sensor e um LED em placas diferentes, nos dois sentidos da UART,
   com GND comum. Se desconectar a UART, o LED remoto deve deixar de receber
   novos acionamentos; reconecte-a antes de continuar.
5. Com o relógio sincronizado, acompanhe duas janelas completas de 15 min na
   Serial. Confira a soma de 30 min, a chance prevista por zona e o maior PWM
   escolhido para cada LED compartilhado. Uma janela inicial parcial é
   descartada.
6. Desligue apenas a API, sem reiniciar as placas. Confira que as novas janelas
   e o acionamento dos sensores continuam localmente.
7. Ainda com a API desligada, reinicie uma placa: a topologia em cache deve
   carregar e os sensores devem funcionar. O histórico recomeça vazio; sem
   horário válido, o repouso permanece PWM 20.
8. Religue a API e confira heartbeat, SensorEvent, mudanças de brilho e
   ausência de eventos duplicados no dashboard.

Os testes elétricos, os dois sentidos de comunicação e a integração com a API
exigem as placas conectadas e seus IDs e dados de rede configurados.
