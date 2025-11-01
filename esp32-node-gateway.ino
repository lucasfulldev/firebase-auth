#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ============================================================
// CONFIGURAÇÃO WiFi
// ============================================================
const char* ssid = "lucasnote";
const char* password = "123456789";

// ============================================================
// CONFIGURAÇÃO DO SERVIDOR NODE.JS
// ============================================================
// Se for usar em rede local, coloque o IP do seu computador
// Exemplo: "http://192.168.1.100:3000"
const char* serverUrl = "http://192.168.43.1:3000"; // Mude para o IP do seu PC!

// ============================================================
// PINOS
// ============================================================
const int ledPin = 2;       // LED de sucesso
const int ledErrorPin = 4;  // LED de erro

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== ESP32 com Node.js Gateway ===\n");

  pinMode(ledPin, OUTPUT);
  pinMode(ledErrorPin, OUTPUT);

  // Conectar ao WiFi
  connectToWiFi();
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Enviar dados para Node.js a cada 10 segundos
    sendDataToNodeJS();
    delay(10000);
  } else {
    Serial.println("WiFi desconectado. Reconectando...");
    connectToWiFi();
    delay(5000);
  }
}

// ============================================================
// FUNÇÕES
// ============================================================

/**
 * Conecta ao WiFi
 */
void connectToWiFi() {
  Serial.println("Conectando ao WiFi: " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi conectado!");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("   RSSI: ");
    Serial.println(WiFi.RSSI());
    Serial.println("");
  } else {
    Serial.println("\n✗ Falha ao conectar WiFi");
  }
}

/**
 * Envia dados para o servidor Node.js
 */
void sendDataToNodeJS() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;

  // Criar JSON com dados
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["temperatura"] = random(20, 35);
  jsonDoc["umidade"] = random(40, 80);
  jsonDoc["rssi"] = WiFi.RSSI();
  jsonDoc["mensagem"] = "Dados do ESP32";

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Construir URL
  String url = String(serverUrl) + "/api/esp32/dados";

  Serial.println("\n--- Enviando dados para Node.js ---");
  Serial.println("URL: " + url);
  Serial.println("Dados: " + jsonString);

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  Serial.print("Status: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("✓ Sucesso!");
    Serial.println("Resposta: " + response);

    // Piscar LED de sucesso
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
  } else {
    Serial.print("✗ Erro: ");
    Serial.println(http.errorToString(httpResponseCode));

    // Piscar LED de erro
    for(int i = 0; i < 3; i++) {
      digitalWrite(ledErrorPin, HIGH);
      delay(100);
      digitalWrite(ledErrorPin, LOW);
      delay(100);
    }
  }

  http.end();
}

/**
 * Envia um acesso (para RFID)
 * Chame isso quando uma tag RFID for lida
 */
void sendAccessLog(String cardId, String action, String username) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["cardId"] = cardId;
  jsonDoc["acao"] = action;
  jsonDoc["usuario"] = username;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  String url = String(serverUrl) + "/api/esp32/acessos";

  Serial.println("\n--- Registrando acesso ---");
  Serial.println("Card: " + cardId);
  Serial.println("Ação: " + action);
  Serial.println("Usuário: " + username);

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.println("✓ Acesso registrado!");
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(ledPin, LOW);
  } else {
    Serial.println("✗ Erro ao registrar acesso");
  }

  http.end();
}

/**
 * Lê dados armazenados no Firebase via Node.js
 */
void readDataFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;
  String url = String(serverUrl) + "/api/esp32/dados";

  Serial.println("\n--- Lendo dados do servidor ---");

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("✓ Dados recebidos:");
    Serial.println(response);
  } else {
    Serial.println("✗ Erro ao ler dados");
  }

  http.end();
}
