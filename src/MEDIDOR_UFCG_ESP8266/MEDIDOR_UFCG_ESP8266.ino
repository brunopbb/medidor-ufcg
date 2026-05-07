#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "secrets.h"

#define FIRMWARE_VERSION "1.0.6" 

const char *SSID = SECRET_SSID;
const char *PASSWORD = SECRET_PASSWORD;
const char *BROKER = SECRET_BROKER;
const char *URL_VERSAO = SECRET_URL_VERSAO;
const char *URL_FIRMWARE = SECRET_URL_FIRMWARE;

const char *CLIENT_ID = "MEDIDOR_UFCG_LABMET";
const char *TOPIC_PUBLISH = "/UFCG/pwrc/";
const char *TOPIC_LOG = "/UFCG/pwrc/log";
const char *TOPIC_SUBSCRIBE = "MEDIDOR_UFCG_CONTROLE_LAB";

String Leitura = "";
int tentativa;
unsigned long Agora;

void ConnectWifi(void);
void connectMQTT(void); 
void Callback(char* topic, byte* payload, unsigned int length);
void checkAndDownloadUpdate(void);

WiFiClient WifiClient;
PubSubClient client(BROKER, 1883, WifiClient);

void setup() {
  Serial.begin(19200);
  Serial.setTimeout(250);
  
  // Linha em branco para limpar lixo do boot no Monitor Serial
  Serial.println();
  Serial.println("=== BOOT: MEDIDOR LABMET ===");
  
  client.setBufferSize(512);
  client.setCallback(Callback);
  
  ConnectWifi();
  connectMQTT();

  String msgBoot = "Medidor Iniciado. Versao: " + String(FIRMWARE_VERSION);
  client.publish(TOPIC_LOG, msgBoot.c_str());
  Serial.println(msgBoot);
  delay(1000);
}

void Callback(char* topic, byte * payload, unsigned int length) {
  String mensagem = "";
  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  if (mensagem == "RESET_ESP") {
    Serial.println("Comando via MQTT: Reiniciando ESP8266...");
    ESP.reset();
  }
  else if (mensagem == "RESET") {
    Serial.print("QRESETM");
  }
  else if (mensagem == "ATUALIZAR") {
    Serial.println("Comando via MQTT: Iniciar OTA");
    client.publish(TOPIC_LOG, "Iniciando checagem de OTA...");
    delay(1000);
    checkAndDownloadUpdate();
  }
  else if (mensagem.startsWith("QCALIB")) {
    Serial.print(mensagem); 
    Serial.println("Aviso: Comando de Calibracao recebido e repassado!");
    client.publish(TOPIC_LOG, "Comando de Calibracao repassado ao ATMega...");
  }
  else {
    client.publish(TOPIC_LOG, "MEDIDOR_UFCG_OK");
  }
}

void loop() {
  if (Serial.available() > 0) {
    Leitura = Serial.readStringUntil('\n'); 
    Leitura.trim(); 
  }

  // --- GUARDIÃO DE CONEXÃO WI-FI ---
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("AVISO: Conexao Wi-Fi perdida! Tentando reconectar...");
    ConnectWifi();
    connectMQTT();
  }

  client.loop();

  if (Leitura != "") {
    if (!client.connected()) {
      Serial.println("AVISO: Cliente MQTT desconectado! Reconectando antes de enviar...");
      connectMQTT();
    }
    
    // --- O GUARDIÃO DO JSON ---
    if (Leitura.startsWith("{")) {
      // É um JSON válido! Pode ir para o banco de dados.
      client.publish(TOPIC_PUBLISH, Leitura.c_str());
      Serial.println("JSON enviado com sucesso!");
    } else {
      // É um texto, log ou erro do Arduino. Vai para o tópico de log!
      client.publish(TOPIC_LOG, Leitura.c_str());
    }
    
    Leitura = "";
  }
}

void ConnectWifi(void) {
  tentativa = 0;
  Serial.print("Conectando a rede Wi-Fi: ");
  Serial.println(SSID);
  
  WiFi.begin(SSID , PASSWORD);
  
  while ((WiFi.status() != WL_CONNECTED) && (tentativa < 15)) {
    tentativa++;
    Serial.print("Tentativa de conexao Wi-Fi ");
    Serial.print(tentativa);
    Serial.println("/15...");
    delay(1000);
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERRO CRITICO: Falha na conexao Wi-Fi. Reiniciando modulo...");
    delay(500);
    ESP.reset();
  } else {
    Serial.println("SUCESSO: Wi-Fi conectado!");
    Serial.print("Endereco IP local: ");
    Serial.println(WiFi.localIP());
  }
}

void connectMQTT(void) {
  Serial.print("Conectando ao broker MQTT: ");
  Serial.println(BROKER);
  
  Agora = millis();
  
  while ((!client.connected()) && ((millis() - Agora) < 55000)) {
    Serial.println("Tentativa de conexao MQTT...");
    if (client.connect(CLIENT_ID, SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
      Serial.println("SUCESSO: Conectado ao broker MQTT!");
      client.subscribe(TOPIC_SUBSCRIBE);
      Serial.print("Inscrito no topico: ");
      Serial.println(TOPIC_SUBSCRIBE);
    } else {
      Serial.print("Falha na conexao MQTT. Codigo de erro: ");
      Serial.print(client.state());
      Serial.println(" - Tentando novamente em 1s...");
      delay(1000);
    }
  }
  
  if ((millis() - Agora) > 55000) {
    Serial.println("ERRO CRITICO: Tempo limite de conexao MQTT esgotado. Reiniciando...");
    ESP.reset();
  }
}

void checkAndDownloadUpdate(void) {
  WiFiClient updateClient; 
  HTTPClient http;
  
  Serial.println("Buscando atualizacoes OTA...");
  http.begin(updateClient, URL_VERSAO);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String serverVersion = http.getString();
    serverVersion.trim(); 
    Serial.print("Versao no servidor: ");
    Serial.println(serverVersion);
    
    if (serverVersion != FIRMWARE_VERSION) {
      Serial.println("Nova versao encontrada! Iniciando download...");
      client.publish(TOPIC_LOG, "Baixando nova versao...");
      client.disconnect(); 
      t_httpUpdate_return ret = ESPhttpUpdate.update(updateClient, URL_FIRMWARE);
      if (ret == HTTP_UPDATE_OK) {
        Serial.println("Atualizacao OTA concluida com sucesso!");
        client.publish(TOPIC_LOG, "Sucesso!");
      } else {
        Serial.println("Erro na atualizacao OTA.");
      }
    } else {
      Serial.println("O firmware ja esta na versao mais recente.");
      client.publish(TOPIC_LOG, "Firmware atualizado.");
    }
  } else {
    Serial.print("Erro ao verificar versao. HTTP Code: ");
    Serial.println(httpCode);
  }
  http.end();
}