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

// Variáveis para controle de leitura de cartão
unsigned long lastCardReadTime = 0;
const long CARD_READ_COOLDOWN = 1500;  // Esperar 1,5 segundos antes de ler novo cartão

// Variáveis para Telegram
String pendingCardUID = "";  // Cartão pendente de confirmação
String registrationId = "";  // ID da requisição no servidor
unsigned long registrationTimeout = 0;  // Timeout da requisição
const long REGISTRATION_TIMEOUT = 300000;  // 5 minutos em milissegundos
bool waitingForTelegramConfirmation = false;

// Flag para controlar o carregamento inicial
bool cardsLoaded = false;

// =================================================================
// 5. FUNÇÕES AUXILIARES
// =================================================================

bool temInternet() {
    WiFiClient client;
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
// 6. COMUNICAÇÃO COM SERVIDOR NODE.JS - TELEGRAM
// =================================================================

/**
 * Envia requisição de confirmação Telegram ao servidor
 */
void askTelegramConfirmation(String cardUID) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Não é possível enviar confirmação."));
    return;
  }

  HTTPClient http;

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["cardUID"] = cardUID;
  jsonDoc["action"] = "ask_confirmation";

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  String url = String(serverUrl) + "/api/esp32/telegram/ask";

  Serial.println("\n--- Enviando confirmação Telegram ---");
  Serial.println("URL: " + url);
  Serial.println("Card: " + cardUID);

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  Serial.print("Status: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("✓ Requisição enviada ao Telegram!");

    // Parse resposta para obter o ID da requisição
    StaticJsonDocument<256> responseDoc;
    deserializeJson(responseDoc, response);

    registrationId = responseDoc["registrationId"].as<String>();
    pendingCardUID = cardUID;
    waitingForTelegramConfirmation = true;
    registrationTimeout = millis();

    Serial.println("Aguardando resposta Telegram...");
  } else {
    Serial.print("✗ Erro ao enviar: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

/**
 * Verifica resposta do Telegram
 */
void checkTelegramResponse() {
  if (!waitingForTelegramConfirmation || registrationId.length() == 0) {
    return;
  }

  // Verificar timeout
  if (millis() - registrationTimeout > REGISTRATION_TIMEOUT) {
    Serial.println(F("❌ Timeout: Confirmação Telegram expirou"));
    pendingCardUID = "";
    registrationId = "";
    waitingForTelegramConfirmation = false;
    isEnrollmentMode = false;
    digitalWrite(INDICATION_LED, LOW);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  String url = String(serverUrl) + "/api/esp32/telegram/check";

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["registrationId"] = registrationId;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    String response = http.getString();
    StaticJsonDocument<256> responseDoc;

    if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
      String status = responseDoc["status"].as<String>();

      if (status == "confirmed") {
        bool confirmed = responseDoc["confirmed"].as<bool>();

        if (confirmed) {
          Serial.println(F("✅ Cadastro CONFIRMADO pelo Telegram!"));
          completeEnrollment();
        } else {
          Serial.println(F("❌ Cadastro CANCELADO pelo Telegram"));
          cancelEnrollment();
        }

        // Limpar estado
        pendingCardUID = "";
        registrationId = "";
        waitingForTelegramConfirmation = false;
      }
    }
  }

  http.end();
}

/**
 * Completa o cadastro do cartão
 */
void completeEnrollment() {
  if (totalAuthorizedCards < 10) {
    authorizedCards[totalAuthorizedCards] = pendingCardUID;
    totalAuthorizedCards++;
  }

  // Salvar no servidor
  saveCardToServer(pendingCardUID, "Nova Pessoa");

  Serial.print(F("### CARTAO CADASTRADO COM SUCESSO: "));
  Serial.println(pendingCardUID);

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

  isEnrollmentMode = false;
  waitingForTelegramConfirmation = false;
}

/**
 * Cancela o cadastro
 */
void cancelEnrollment() {
  Serial.println(F("Cadastro cancelado pelo usuário Telegram"));

  // Feedback visual
  digitalWrite(DENIED_LED, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(300);
  digitalWrite(DENIED_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  isEnrollmentMode = false;
  waitingForTelegramConfirmation = false;
  pendingCardUID = "";
  registrationId = "";
  digitalWrite(INDICATION_LED, LOW);
}

// =================================================================
// 7. COMUNICAÇÃO COM SERVIDOR NODE.JS - ACESSO
// =================================================================

void sendAccessToServer(String cardID, String acao, String usuario) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Não é possível enviar acesso."));
    return;
  }

  const int MAX_RETRIES = 3;
  int retryCount = 0;
  bool success = false;

  while (retryCount < MAX_RETRIES && !success) {
    HTTPClient http;

    StaticJsonDocument<256> jsonDoc;
    jsonDoc["cardId"] = cardID;
    jsonDoc["acao"] = acao;
    jsonDoc["usuario"] = usuario;

    String jsonString;
    serializeJson(jsonDoc, jsonString);

    String url = String(serverUrl) + "/api/esp32/acessos";

    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0 && httpResponseCode < 400) {
      Serial.println("✓ Acesso registrado no servidor!");
      success = true;
    } else {
      retryCount++;
      if (retryCount < MAX_RETRIES) {
        Serial.print("Tentativa ");
        Serial.print(retryCount);
        Serial.println(" falhou. Retentando...");
        delay(500);  // Esperar 500ms antes de retentar
      } else {
        Serial.print("✗ Erro ao registrar acesso (3 tentativas): ");
        Serial.println(http.errorToString(httpResponseCode));
      }
    }

    http.end();
  }
}

void saveCardToServer(String cardUID, String usuario) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Cartão não salvo no servidor."));
    return;
  }

  const int MAX_RETRIES = 3;
  int retryCount = 0;
  bool success = false;

  while (retryCount < MAX_RETRIES && !success) {
    HTTPClient http;

    StaticJsonDocument<256> jsonDoc;
    jsonDoc["cardId"] = cardUID;
    jsonDoc["acao"] = "cartao_cadastrado";
    jsonDoc["usuario"] = usuario;

    String jsonString;
    serializeJson(jsonDoc, jsonString);

    String url = String(serverUrl) + "/api/esp32/acessos";

    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0 && httpResponseCode < 400) {
      Serial.println("✓ Cartão salvo no servidor!");
      success = true;
    } else {
      retryCount++;
      if (retryCount < MAX_RETRIES) {
        Serial.print("Tentativa ");
        Serial.print(retryCount);
        Serial.println(" falhou. Retentando...");
        delay(500);
      } else {
        Serial.print("✗ Erro ao salvar cartão (3 tentativas): ");
        Serial.println(http.errorToString(httpResponseCode));
      }
    }

    http.end();
  }
}

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
    Serial.println("✓ Resposta recebida do servidor");

    StaticJsonDocument<512> jsonDoc;
    if (deserializeJson(jsonDoc, response) == DeserializationError::Ok) {
      Serial.println("✓ Cartões carregados com sucesso!");
    }
  } else {
    Serial.print("✗ Erro ao carregar cartões: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
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
  Serial.println(F("Sistema de Acesso RFID com Gateway Node.js + Telegram iniciado."));

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
  checkWiFiConnection();

  // Verificar resposta do Telegram se estiver aguardando
  if (waitingForTelegramConfirmation) {
    checkTelegramResponse();
    delay(2000);  // Verificar a cada 2 segundos
    return;
  }

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
      String cardUID = getCardUID();
      Serial.println(F("Cartão detectado. Enviando para confirmação Telegram..."));

      // Enviar para confirmar via Telegram
      askTelegramConfirmation(cardUID);

      // Aguardar resposta
      waitingForTelegramConfirmation = true;
      registrationTimeout = millis();

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
    return;
  }

  // LÓGICA DE ACESSO NORMAL
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Verificar cooldown para evitar leitura duplicada
  unsigned long currentTime = millis();
  if (currentTime - lastCardReadTime < CARD_READ_COOLDOWN) {
    Serial.println(F("Cartão ainda muito próximo. Aguardando..."));
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }

  lastCardReadTime = currentTime;

  Serial.print(F("UID do Cartao: "));
  String cardUID = getCardUID();
  Serial.println(cardUID);

  if (isAuthorized(cardUID)) {
    Serial.println(F(">>> ACESSO PERMITIDO! <<<"));
    sendAccessToServer(cardUID, "acesso_concedido", "Usuario");

    digitalWrite(ACTUATOR_PIN, HIGH);
    digitalWrite(RELAY_PIN, HIGH);
    delay(500);
    digitalWrite(ACTUATOR_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
  } else {
    Serial.println(F(">>> ACESSO NEGADO! <<<"));
    sendAccessToServer(cardUID, "acesso_negado", "Desconhecido");

    digitalWrite(DENIED_LED, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(DENIED_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // Aguardar tempo mínimo antes de aceitar novo cartão
  delay(1000);
}
