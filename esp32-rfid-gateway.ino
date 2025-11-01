#include <SPI.h>
#include <MFRC522.h>
#include "WiFi.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =================================================================
// 1. PINAGEM E CONFIGURAÇÕES DE HARDWARE
// =================================================================

#define RST_PIN 2  // Pino RST (Reset) do RC522
#define SS_PIN 5   // Pino SDA (Slave Select/CS) do RC522

#define BTN_PIN 33         // Pino do Botão de Cadastro (D33)
#define INDICATION_LED 25  // LED de Indicação/Cadastro (D25)
#define ACTUATOR_PIN 26    // Atuador de Acesso (D26 - LED Verde/Relé)
#define DENIED_LED 27
#define BUZZER_PIN 13
#define RELAY_PIN 32

// =================================================================
// 2. CONFIGURAÇÕES WiFi
// =================================================================

#define WIFI_SSID "lucasnote"      // Nome da rede WiFi
#define WIFI_PASSWORD "123456789"  // Senha da rede WiFi
#define WIFI_TIMEOUT 20000         // Timeout em milissegundos (20 segundos)

// =================================================================
// 3. CONFIGURAÇÕES DO SERVIDOR NODE.JS GATEWAY
// =================================================================

const char* serverUrl = "https://firebase-auth-three-sigma.vercel.app";

// =================================================================
// 4. OBJETOS E VARIÁVEIS GLOBAIS
// =================================================================

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Cria uma instância MFRC522

// Lista de Cartões Autorizados (carregada do servidor)
String authorizedCards[10];  // Suporta até 10 cartões
int totalAuthorizedCards = 0;

// Variáveis para o Botão e Cadastro
unsigned long buttonPressStartTime = 0;
const long requiredPressTime = 10000;  // 10 segundos em milissegundos
bool isEnrollmentMode = false;
unsigned long previousMillisBlink = 0;
const long blinkInterval = 250;  // Pisca a cada 250ms

// Flag para controlar o carregamento inicial
bool cardsLoaded = false;

// =================================================================
// 5. FUNÇÕES AUXILIARES
// =================================================================

bool temInternet() {
    WiFiClient client;
    // Tenta conectar a um servidor confiável (Google DNS)
    if (client.connect("8.8.8.8", 53)) {
        client.stop();
        return true;
    }
    return false;
}

void connectToWiFi() {
  Serial.println(F("\n--- Iniciando conexão WiFi ---"));
  Serial.print(F("Conectando à rede: "));
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  int dotCount = 0;

  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
    dotCount++;

    if (dotCount % 20 == 0) {
      Serial.println();
    }
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("✓ CONECTADO AO WIFI COM SUCESSO!"));
    Serial.print(F("SSID: "));
    Serial.println(WiFi.SSID());
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("Sinal: "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
  } else {
    Serial.println(F("✗ FALHA NA CONEXÃO WiFi!"));
    Serial.println(F("Verifique SSID e senha."));
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Conexão WiFi perdida. Reconectando..."));
    connectToWiFi();
  }
}

String getCardUID() {
  String uidString = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uidString += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    uidString += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();
  return uidString;
}

boolean isAuthorized(String cardUID) {
  for (int i = 0; i < totalAuthorizedCards; i++) {
    if (authorizedCards[i] == cardUID) {
      return true;
    }
  }
  return false;
}

// =================================================================
// 6. COMUNICAÇÃO COM SERVIDOR NODE.JS
// =================================================================

/**
 * Envia um acesso para o servidor Node.js
 */
void sendAccessToServer(String cardID, String acao, String usuario) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Não é possível enviar acesso."));
    return;
  }

  HTTPClient http;

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["cardId"] = cardID;
  jsonDoc["acao"] = acao;
  jsonDoc["usuario"] = usuario;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  String url = String(serverUrl) + "/api/esp32/acessos";

  Serial.println("\n--- Enviando acesso para servidor ---");
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
    Serial.println("✓ Acesso registrado no servidor!");
    Serial.println("Resposta: " + response);
  } else {
    Serial.print("✗ Erro ao registrar acesso: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

/**
 * Carrega cartões autorizados do servidor Node.js
 */
void loadAuthorizedCardsFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Não é possível carregar cartões."));
    return;
  }

  HTTPClient http;
  String url = String(serverUrl) + "/api/esp32/acessos";

  Serial.println(F("\n--- Carregando cartões autorizados do servidor ---"));

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("✓ Resposta recebida do servidor:");
    Serial.println(response);

    // Parse JSON
    StaticJsonDocument<512> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, response);

    if (!error) {
      JsonObject data = jsonDoc["data"];
      if (!data.isNull()) {
        // Aqui você poderia processar os dados de acessos anteriores
        Serial.println("✓ Cartões carregados com sucesso!");
      }
    } else {
      Serial.println(F("Erro ao fazer parse do JSON"));
    }
  } else {
    Serial.print("✗ Erro ao carregar cartões: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

/**
 * Registra um novo cartão no servidor
 */
void saveCardToServer(String cardUID, String usuario) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Cartão não salvo no servidor."));
    return;
  }

  HTTPClient http;

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["cardId"] = cardUID;
  jsonDoc["acao"] = "cartao_cadastrado";
  jsonDoc["usuario"] = usuario;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  String url = String(serverUrl) + "/api/esp32/acessos";

  Serial.println("\n--- Salvando cartão no servidor ---");
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
    Serial.println("✓ Cartão salvo no servidor!");
    Serial.println("Resposta: " + response);
  } else {
    Serial.print("✗ Erro ao salvar cartão: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

// =================================================================
// 7. LÓGICA DE ACESSO E CADASTRO
// =================================================================

void enrollNewCard() {
  String cardUID = getCardUID();

  // Salva na memória local
  if (totalAuthorizedCards < 10) {
    authorizedCards[totalAuthorizedCards] = cardUID;
    totalAuthorizedCards++;
  }

  // Salva no servidor
  saveCardToServer(cardUID, "Nova Pessoa");

  Serial.print(F("### CARTAO CADASTRADO COM SUCESSO: "));
  Serial.println(cardUID);
  Serial.println(F("Sistema retornando ao modo de acesso normal."));

  // Feedback visual e sonoro
  digitalWrite(INDICATION_LED, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(INDICATION_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  delay(200);
  digitalWrite(INDICATION_LED, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(INDICATION_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void grantAccess(String cardUID) {
  Serial.println(F(">>> ACESSO PERMITIDO! <<<"));

  // Envia para servidor
  sendAccessToServer(cardUID, "acesso_concedido", "Usuario");

  // Ativa relé
  digitalWrite(ACTUATOR_PIN, HIGH);
  digitalWrite(RELAY_PIN, HIGH);

  delay(500);

  digitalWrite(ACTUATOR_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // Feedback visual
  digitalWrite(INDICATION_LED, HIGH);
  delay(100);
  digitalWrite(INDICATION_LED, LOW);
}

void denyAccess(String cardUID) {
  Serial.println(F(">>> ACESSO NEGADO! <<<"));

  // Envia para servidor
  sendAccessToServer(cardUID, "acesso_negado", "Desconhecido");

  // Feedback sonoro e visual
  digitalWrite(DENIED_LED, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(DENIED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

// =================================================================
// 8. SETUP
// =================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  SPI.begin();
  mfrc522.PCD_Init();

  delay(1000);
  Serial.println("Setup done");

  // Configura Pinos
  pinMode(ACTUATOR_PIN, OUTPUT);
  pinMode(INDICATION_LED, OUTPUT);
  pinMode(DENIED_LED, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Inicializa os pinos
  digitalWrite(ACTUATOR_PIN, LOW);
  digitalWrite(INDICATION_LED, LOW);
  digitalWrite(DENIED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  Serial.println(F("\n\n===== INICIANDO SISTEMA ====="));
  Serial.println(F("Sistema de Acesso RFID com Gateway Node.js iniciado."));

  // Conecta ao WiFi
  connectToWiFi();

  delay(2000);

  // Carrega cartões do servidor
  loadAuthorizedCardsFromServer();

  Serial.println(F("\nSistema pronto para uso."));
}

// =================================================================
// 9. LOOP PRINCIPAL
// =================================================================

void loop() {
  // Verifica conexão Wi-Fi periodicamente
  checkWiFiConnection();

  // DETECÇÃO DE PRESSÃO LONGA DO BOTÃO
  if (digitalRead(BTN_PIN) == LOW) {
    if (buttonPressStartTime == 0) {
      buttonPressStartTime = millis();
      Serial.println(F("Botao pressionado. Segurando por 10s..."));
    }

    if (millis() - buttonPressStartTime >= requiredPressTime && !isEnrollmentMode) {
      isEnrollmentMode = true;
      Serial.println(F(">>> MODO DE CADASTRO ATIVADO! <<<"));
      Serial.println(F("Aproxime o novo cartao."));
      buttonPressStartTime = 0;
    }
  } else {
    if (buttonPressStartTime != 0 && !isEnrollmentMode) {
      Serial.println(F("Botao solto. Tempo insuficiente para cadastro."));
    }
    buttonPressStartTime = 0;
  }

  // LÓGICA DO MODO DE CADASTRO
  if (isEnrollmentMode) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillisBlink >= blinkInterval) {
      previousMillisBlink = currentMillis;
      digitalWrite(INDICATION_LED, !digitalRead(INDICATION_LED));
    }

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      enrollNewCard();
      isEnrollmentMode = false;
      digitalWrite(INDICATION_LED, LOW);

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
    return;
  }

  // LÓGICA DE ACESSO NORMAL (FORA DO MODO CADASTRO)
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("UID do Cartao: "));
  String cardUID = getCardUID();
  Serial.println(cardUID);

  if (isAuthorized(cardUID)) {
    grantAccess(cardUID);
  } else {
    denyAccess(cardUID);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
