# ⚡ Smart Meter IoT - Monitoramento de Energia Avançado (UFCG)

Este projeto é um sistema de medição de energia elétrica IoT de alta precisão, desenvolvido com uma arquitetura de microcontroladores duplos (ATMega + ESP8266) e o chip dedicado de metrologia polifásica **ATM90E36A**. 

O sistema coleta, processa e envia dados elétricos em tempo real para um servidor MQTT, armazenando as séries temporais em um banco de dados (PostgreSQL/TimescaleDB) para visualização e faturamento através do Grafana.

## 📸 Visão Geral do Dashboard (Grafana)

<!-- 
  INSTRUÇÃO: 
  1. Tire um print bem bonito do seu Grafana com dados rodando.
  2. Salve a imagem com o nome "dashboard.png" na mesma pasta deste README.
  3. O GitHub vai carregar a imagem automaticamente aqui embaixo! 👇
-->
![Painel de Monitoramento Grafana](/images/Screenshot_20260503_150725.png)
*Painel de controle exibindo as medições instantâneas e o faturamento blindado acumulado do mês.*

---

## 🏗️ Arquitetura do Sistema

O projeto utiliza o princípio de **Desacoplamento de Produtor e Consumidor**:
*   **ATMega (O Produtor/Metrologia):** Dedicado exclusivamente à leitura em alta velocidade dos registradores do ATM90E36A. Ele acumula os dados em uma janela de tempo fixa (ex: 60 segundos), calcula as médias reais, aplica filtros de ruído e envia um pacote JSON limpo via comunicação Serial.
*   **ESP8266 (O Consumidor/Gateway):** Atua como o cérebro de rede. Ele recebe o JSON do ATMega, gerencia a conexão Wi-Fi, publica os dados no broker MQTT (`Mosquitto`), escuta comandos de controle remotos e gerencia atualizações de firmware OTA (Over-The-Air).

## ✨ Principais Features

*   **📊 Medição Completa:** Monitoramento de Tensão (V), Corrente (A), Potência Ativa (W), Potência Reativa (var), Potência Aparente (VA), Fator de Potência e Frequência (Hz).
*   **🛡️ Filtro "No-Load" (Zona Morta):** Implementação via software para zerar ruídos elétricos de fundo dos sensores de corrente quando a rede está em vazio, evitando o cômputo de "consumo fantasma".
*   **⚙️ Calibração Remota Inteligente:** Ajuste de ganhos de Tensão e Corrente via comandos MQTT, com cálculo automático de multiplicadores e salvamento permanente na memória **EEPROM** do ATMega.
*   **☁️ Atualização OTA (Over-The-Air):** Capacidade de atualizar o firmware do ESP8266 remotamente via servidor HTTP, sem necessidade de conexão USB.
*   **📈 Dashboard Financeiro Blindado (Grafana):**
    *   Painéis dinâmicos para testes de bancada em tempo real.
    *   Métricas de faturamento (kWh e R$) com queries SQL "chumbadas" no início do mês.
    *   Lógica anti-apagão (`duracao_s < 300`) que impede anomalias matemáticas de consumo caso o medidor perca conexão com a internet ou sofra queda de energia.

---

## 🔬 Método de Calibração: O Teste do Ebulidor

Para garantir a precisão metrológica do sistema (equiparando-o aos medidores de concessionárias), desenvolvemos um método de calibração utilizando uma carga resistiva pura de alta potência: um **Ebulidor elétrico** (rabo quente).

### Por que um Ebulidor?
O ebulidor é uma resistência pura. Ao contrário de motores ou eletrônicos, ele não possui indutância ou capacitância significativa. Isso garante que o **Fator de Potência seja praticamente 1.0**. Em um cenário ideal de calibração, a Potência Ativa (W) se iguala à Potência Aparente (VA), facilitando a validação das leituras de Tensão e Corrente sem distorções harmônicas severas.

### Procedimento de Calibração (Passo a Passo)

1.  **Aferição de Referência:** O ebulidor é ligado à rede elétrica passando pelo nosso medidor. Utiliza-se um multímetro True-RMS de bancada confiável (ou alicate amperímetro de precisão) para medir a Tensão (V) e a Corrente (A) exatas que estão circulando no momento.
2.  **Comportamento Físico Esperado:** Ao ligar a carga pesada (~1000W a 2000W), observa-se imediatamente no painel Grafana:
    *   Salto instantâneo na Potência Ativa (W) e na Corrente (A).
    *   "Afundamento" natural da Tensão da rede (queda de alguns volts devido à resistência dos cabos).
    *   Fator de Potência cravado próximo a `1.0`.
3.  **Envio dos Comandos de Calibração:** Com a carga ligada e estabilizada, enviamos os comandos remotos via aplicativo MQTT (ex: MQTT Explorer) no tópico de controle do medidor. O processo é feito em duas etapas:
    *   **Calibração de Tensão:** Envia-se o comando `CALIB_VA:218.5` (Onde 218.5 é a leitura de tensão do multímetro).
    *   **Calibração de Corrente:** Envia-se o comando `CALIB_IA:6.85` (Onde 6.85 é a leitura de corrente do alicate amperímetro).
4.  **Processamento Interno (ATMega + ATM90E36A):**
    *   O ESP8266 recebe o comando MQTT puro e o repassa encapsulado para o ATMega via Serial (ex: `QCALIB_VA:218.5M`).
    *   O ATMega desativa temporariamente o *Watchdog Timer* (para evitar reinicializações durante o cálculo).
    *   Para o canal solicitado (Tensão ou Corrente), o sistema força o registrador do ATM90E36A para o "Ganho Padrão de Fábrica" e realiza `N` leituras brutas em loop (filtrando anomalias).
    *   Calcula-se a razão matemática entre o **Valor de Referência Informado** e a **Média Bruta Lida** pelo chip.
    *   Aplica-se uma regra de três para descobrir o novo Ganho Hexadecimal exato, respeitando o limite de 16-bits.
    *   O novo valor de ganho é gravado nos registradores do chip para efeito imediato e salvo na **EEPROM** (Endereço `0` para Tensão, Endereço `2` para Corrente) para sobreviver a reinicializações físicas.
5.  **Confirmação:** O sistema responde via MQTT (`{"INFO":"Calibracao VA Salva!"}` ou `{"INFO":"Calibracao IA Salva!"}`) e os gráficos no Grafana passam a refletir os valores exatos calibrados.

---

## 🛠️ Tecnologias e Bibliotecas Utilizadas
*   C++ (Arduino IDE)
*   ATM90E36A (Metrologia)
*   PubSubClient (MQTT)
*   ArduinoJson (Estruturação de Dados)
*   Mosquitto (Broker MQTT)
*   PostgreSQL / TimescaleDB (Time-Series Database)
*   Grafana (Visualização e Queries SQL Avançadas)
