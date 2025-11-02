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

// Sensor Ultrassônico HCSR04
#define TRIG_PIN 16   // Pino Trig do sensor ultrassônico (GPIO 34)
#define ECHO_PIN 4    // Pino Echo do sensor ultrassônico (GPIO 4 - FUNCIONANDO!)

// Variáveis do sensor ultrassônico
unsigned long lastUltrasonicReadTime = 0;
const long ULTRASONIC_READ_INTERVAL = 1000;  // Ler a cada 1 segundo (foi 500ms)
float lastDistance = 0;  // Última distância medida em cm

// Variáveis de controle de alertas da porta
bool doorOpen = false;                      // Rastreia se a porta está aberta (distância > 5cm)
bool accessAuthorized = false;              // Flag que acesso foi autorizado
unsigned long redBlinkMillis = 0;           // Controle do pisca LED
bool currentLedIsGreen = true;              // true = verde, false = amarelo
int blinkCount = 0;                         // Contador de piscadas
const long BLINK_ON_TIME = 100;             // Tempo que LED fica aceso (100ms)
const long BLINK_OFF_TIME = 100;            // Tempo entre piscadas (100ms)
const int BLINKS_PER_COLOR = 2;             // 2 piscadas por cor
int currentBlink = 0;                       // Qual piscada estamos (0 ou 1)

// Variáveis para controle de tempo de acesso com porta fechada
bool accessGrantedWhileClosed = false;      // Flag que acesso foi concedido com porta fechada
unsigned long accessGrantedTime = 0;        // Tempo quando acesso foi concedido
const long ACCESS_TIMEOUT = 10000;          // 10 segundos em milissegundos
bool personEnteredAfterAccess = false;      // Flag que pessoa entrou após acesso autorizado

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
  // Se WiFi está desconectado, usar cache local como fallback
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi desconectado. Usando cache local..."));
    for (int i = 0; i < totalAuthorizedCards; i++) {
      if (authorizedCards[i] == cardUID) {
        return true;
      }
    }
    return false;
  }

  // Fazer requisição ao servidor para validar cartão
  const int MAX_RETRIES = 2;
  int retryCount = 0;
  boolean authorized = false;

  while (retryCount < MAX_RETRIES && !authorized) {
    HTTPClient http;

    StaticJsonDocument<256> jsonDoc;
    jsonDoc["cardId"] = cardUID;

    String jsonString;
    serializeJson(jsonDoc, jsonString);

    String url = String(serverUrl) + "/api/esp32/check-authorization";

    Serial.println("\n--- Verificando autorização ---");
    Serial.println("URL: " + url);
    Serial.println("Card: " + cardUID);

    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonString);

    Serial.print("Status: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode > 0 && httpResponseCode < 400) {
      String response = http.getString();
      Serial.println("✓ Resposta recebida do servidor");

      StaticJsonDocument<256> responseDoc;
      if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
        authorized = responseDoc["authorized"].as<bool>();

        if (authorized) {
          Serial.println("✓ Cartão AUTORIZADO pelo servidor!");
        } else {
          Serial.println("✗ Cartão NÃO AUTORIZADO pelo servidor!");
        }

        http.end();
        return authorized;
      }
    } else {
      retryCount++;
      if (retryCount < MAX_RETRIES) {
        Serial.print("Tentativa ");
        Serial.print(retryCount);
        Serial.println(" falhou. Retentando...");
        delay(500);
      } else {
        Serial.print("✗ Erro ao verificar autorização (2 tentativas): ");
        Serial.println(http.errorToString(httpResponseCode));

        // Fallback: usar cache local se houver erro na requisição
        Serial.println("Usando cache local como fallback...");
        for (int i = 0; i < totalAuthorizedCards; i++) {
          if (authorizedCards[i] == cardUID) {
            http.end();
            return true;
          }
        }
      }
    }

    http.end();
  }

  return false;
}

/**
 * Gerencia o timeout de acesso quando a porta está fechada
 * - Se 10 segundos passaram sem pessoa entrar: reseta o acesso
 * - Se pessoa entrou (distância > 5): mantém acesso até fechar de novo
 */
void checkAccessTimeout() {
  // Verificar se há qualquer tipo de acesso ativo
  if (!accessGrantedWhileClosed && !personEnteredAfterAccess && !accessAuthorized) {
    return;
  }

  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - accessGrantedTime;

  // Se pessoa entrou (distância > 5cm), marcar que entrou
  if (doorOpen && !personEnteredAfterAccess && accessGrantedWhileClosed) {
    personEnteredAfterAccess = true;
    Serial.println(F("✓ Pessoa entrou! LED verde mantém-se aceso."));
  }

  // Se a porta fechou APÓS ter sido aberta com acesso autorizado
  if (!doorOpen && (personEnteredAfterAccess || accessAuthorized)) {
    Serial.println(F("🔒 Porta fechada após acesso. Resetando."));
    accessGrantedWhileClosed = false;
    personEnteredAfterAccess = false;
    accessAuthorized = false;
    digitalWrite(ACTUATOR_PIN, LOW);
    return;
  }

  // Timeout de 10 segundos: se pessoa NÃO entrou e tempo expirou (apenas para accessGrantedWhileClosed)
  if (accessGrantedWhileClosed && !personEnteredAfterAccess && (elapsedTime > ACCESS_TIMEOUT)) {
    Serial.println(F("⏱ Timeout: Acesso expirou (10s sem entrar). Voltando ao fluxo normal."));
    accessGrantedWhileClosed = false;
    personEnteredAfterAccess = false;
    accessAuthorized = false;
    digitalWrite(ACTUATOR_PIN, LOW);
  }
}

/**
 * Controla os alertas de LED quando a porta está aberta
 * - Porta aberta SEM acesso: LED vermelho pisca
 * - Porta aberta COM acesso: LED verde fica aceso
 * - Porta fechada COM acesso recente: mantém LED verde
 * - Porta fechada: LEDs desligam
 */
void handleDoorAlerts() {
  // Se acesso foi concedido com porta fechada, manter LED verde aceso
  if (accessGrantedWhileClosed && !doorOpen) {
    Serial.println(F("DEBUG handleDoorAlerts - Mantendo LED verde aceso"));
    Serial.print(F("DEBUG - Estado do pino ACTUATOR_PIN ANTES: "));
    Serial.println(digitalRead(ACTUATOR_PIN));

    redBlinkMillis = millis();  // Resetar timer
    digitalWrite(ACTUATOR_PIN, HIGH);   // LED verde

    Serial.print(F("DEBUG - Estado do pino ACTUATOR_PIN DEPOIS: "));
    Serial.println(digitalRead(ACTUATOR_PIN));

    digitalWrite(DENIED_LED, LOW);      // Desligar LED vermelho
    digitalWrite(INDICATION_LED, LOW);  // Desligar LED amarelo
    digitalWrite(BUZZER_PIN, LOW);      // Desligar buzzer
    return;
  }

  // Se porta não está aberta, desligar todos LEDs e buzzer
  if (!doorOpen) {
    Serial.println(F("DEBUG handleDoorAlerts - Porta fechada, desligando LEDs"));
    redBlinkMillis = millis();  // Resetar timer
    digitalWrite(ACTUATOR_PIN, LOW);
    digitalWrite(DENIED_LED, LOW);
    digitalWrite(INDICATION_LED, LOW);  // Desligar LED amarelo
    digitalWrite(BUZZER_PIN, LOW);      // Desligar buzzer
    return;
  }

  // PORTA ESTÁ ABERTA
  if (accessAuthorized || personEnteredAfterAccess) {
    // Acesso foi autorizado - manter LED verde aceso
    redBlinkMillis = millis();  // Resetar timer
    digitalWrite(ACTUATOR_PIN, HIGH);   // LED verde
    digitalWrite(DENIED_LED, LOW);      // Desligar LED vermelho
    digitalWrite(INDICATION_LED, LOW);  // Desligar LED amarelo
    digitalWrite(BUZZER_PIN, LOW);      // Desligar buzzer
  } else {
    // Acesso NÃO autorizado - LED verde e amarelo alternam (efeito polícia com 2 piscadas)
    unsigned long currentMillis = millis();

    // Inicializar timer se for a primeira vez
    if (redBlinkMillis == 0) {
      redBlinkMillis = currentMillis;
    }

    unsigned long elapsedTime = currentMillis - redBlinkMillis;

    // Calcular em qual fase estamos
    // Fase 0: Verde ON (0-100ms)
    // Fase 1: Verde OFF (100-200ms)
    // Fase 2: Verde ON (200-300ms)
    // Fase 3: Verde OFF (300-400ms)
    // Fase 4: Amarelo ON (400-500ms)
    // Fase 5: Amarelo OFF (500-600ms)
    // Fase 6: Amarelo ON (600-700ms)
    // Fase 7: Amarelo OFF (700-800ms) -> depois volta para fase 0

    int totalCycleTime = BLINK_ON_TIME * 8;  // 800ms total
    int phase = (elapsedTime / BLINK_ON_TIME) % 8;

    // Desligar LED vermelho sempre
    digitalWrite(DENIED_LED, LOW);

    if (phase < 4) {
      // Piscadas do vermelho (fases 0-3)
      if (phase == 0 || phase == 2) {
        // Vermelho LIGADO + Buzzer
        digitalWrite(DENIED_LED, HIGH);
        digitalWrite(ACTUATOR_PIN, LOW);
        digitalWrite(INDICATION_LED, LOW);
        digitalWrite(BUZZER_PIN, HIGH);
      } else {
        // Vermelho DESLIGADO + Buzzer OFF
        digitalWrite(DENIED_LED, LOW);
        digitalWrite(ACTUATOR_PIN, LOW);
        digitalWrite(INDICATION_LED, LOW);
        digitalWrite(BUZZER_PIN, LOW);
      }
    } else {
      // Piscadas do verde (fases 4-7)
      if (phase == 4 || phase == 6) {
        // Verde LIGADO + Buzzer
        digitalWrite(DENIED_LED, LOW);
        digitalWrite(ACTUATOR_PIN, HIGH);
        digitalWrite(INDICATION_LED, LOW);
        digitalWrite(BUZZER_PIN, HIGH);
      } else {
        // Verde DESLIGADO + Buzzer OFF
        digitalWrite(DENIED_LED, LOW);
        digitalWrite(ACTUATOR_PIN, LOW);
        digitalWrite(INDICATION_LED, LOW);
        digitalWrite(BUZZER_PIN, LOW);
      }
    }
  }
}

/**
 * Mede a distância usando o sensor ultrassônico HCSR04
 * Retorna a distância em centímetros
 */
float measureDistance() {
  // Garantir que TRIG está LOW antes de começar
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Enviar pulso trigger (10µs)
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW); 
  // Medir tempo de retorno do eco
  // timeout de 60ms = ~10 metros de alcance
  unsigned long duration = pulseIn(ECHO_PIN, HIGH);

  
  // Calcular distância usando a fórmula do YouTube
  // distancia = tempoSom / 58
  // (equivalente a: (tempo / 2) * 0.0343)
  float distancia = duration / 58.0;

  return distancia;
}

 

/**
 * Verifica distância e executa ações se necessário
 */
void checkUltrasonicSensor() {
  unsigned long currentTime = millis();

  // Ler sensor a cada 1 segundo
  if (currentTime - lastUltrasonicReadTime >= ULTRASONIC_READ_INTERVAL) {
    lastUltrasonicReadTime = currentTime;

    float distance = measureDistance();

    // Verificar se a leitura é válida
    if (distance > 0) {
      // Aceitar leituras entre 2cm e 4 metros
      if (distance >= 2 && distance <= 400) {
        lastDistance = distance;

        Serial.print("📏 Distância: ");
        Serial.print(distance, 2);  // 2 casas decimais
        Serial.println(" cm");

        // ===== LÓGICA DE DETECÇÃO DE PORTA =====
        // Detectar mudança de estado da porta
        bool wasDoorOpen = doorOpen;
        doorOpen = (distance > 5.0);  // Porta aberta se distância > 5cm

        // Se porta estava fechada e agora abriu
        if (!wasDoorOpen && doorOpen) {
          Serial.println("🔓 PORTA ABERTA! Aguardando autorização...");
          accessAuthorized = false;  // Resetar autorização quando porta abre
        }

        // Se porta está fechando (voltando de aberta para fechada)
        if (wasDoorOpen && !doorOpen) {
          Serial.println("🔒 Porta fechada. Sistema resetado.");
          // Não resetar as flags de accessGrantedWhileClosed e personEnteredAfterAccess aqui
          // Elas são resetadas pela função checkAccessTimeout()
          accessAuthorized = false;  // Resetar autorização
          digitalWrite(DENIED_LED, LOW);
        }

      } else if (distance > 400) {
        Serial.println("📏 Objeto muito longe (>400cm)");
      } else {
        Serial.println("📏 Objeto muito perto (<2cm)");
        Serial.print(distance, 2);
      }
    }
    // Se falhar, apenas fica silencioso (sem "⚠ Erro")
  }
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
    isEnrollmentMode = false;
    return;
  }

  const int MAX_RETRIES = 3;
  int retryCount = 0;
  bool success = false;

  while (retryCount < MAX_RETRIES && !success) {
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

    if (httpResponseCode > 0 && httpResponseCode < 400) {
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
      success = true;
    } else {
      retryCount++;
      if (retryCount < MAX_RETRIES) {
        Serial.print("Tentativa ");
        Serial.print(retryCount);
        Serial.println(" falhou. Retentando em 1s...");
        delay(1000);
      } else {
        Serial.print("✗ Erro ao enviar confirmação Telegram (3 tentativas): ");
        Serial.println(http.errorToString(httpResponseCode));
        isEnrollmentMode = false;
        digitalWrite(INDICATION_LED, LOW);
      }
    }

    http.end();
  }
}

/**
 * Verifica resposta do Telegram
 */
void checkTelegramResponse() {
  if (!waitingForTelegramConfirmation || registrationId.length() == 0) {
    return;
  }

  // Verificar timeout (5 minutos)
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
    Serial.println(F("WiFi desconectado. Aguardando reconexão..."));
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

  if (httpResponseCode > 0 && httpResponseCode < 400) {
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
  } else if (httpResponseCode > 0) {
    Serial.print("⚠ Resposta inesperada do servidor: ");
    Serial.println(httpResponseCode);
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

  const int MAX_RETRIES = 3;
  int retryCount = 0;
  bool success = false;

  Serial.println(F("\n--- Carregando cartões autorizados do servidor ---"));

  while (retryCount < MAX_RETRIES && !success) {
    HTTPClient http;
    String url = String(serverUrl) + "/api/esp32/acessos";

    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    http.begin(url);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0 && httpResponseCode < 400) {
      String response = http.getString();
      Serial.println("✓ Resposta recebida do servidor");

      StaticJsonDocument<512> jsonDoc;
      if (deserializeJson(jsonDoc, response) == DeserializationError::Ok) {
        Serial.println("✓ Cartões carregados com sucesso!");
        success = true;
      }
    } else {
      retryCount++;
      if (retryCount < MAX_RETRIES) {
        Serial.print("Tentativa ");
        Serial.print(retryCount);
        Serial.println(" falhou. Retentando em 2s...");
        delay(2000);
      } else {
        Serial.print("✗ Erro ao carregar cartões (3 tentativas): ");
        Serial.println(http.errorToString(httpResponseCode));
      }
    }

    http.end();
  }
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

  // Configura Sensor Ultrassônico
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

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

  delay(1000); 

  Serial.println(F("\nSistema pronto para uso."));
}

// =================================================================
// 9. LOOP PRINCIPAL
// =================================================================

void loop() {
  checkWiFiConnection();

  // Verificar sensor ultrassônico continuamente
  checkUltrasonicSensor();

  // Verificar timeout de acesso com porta fechada
  checkAccessTimeout();

  // Controlar alertas de LED quando porta está aberta
  handleDoorAlerts();

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

  // Feedback visual imediato - LEDs verde e vermelho juntos indicam processamento
  digitalWrite(ACTUATOR_PIN, HIGH);   // LED verde
  digitalWrite(DENIED_LED, HIGH);     // LED vermelho
  digitalWrite(BUZZER_PIN, HIGH);
  delay(50);  // Beep curto
  digitalWrite(BUZZER_PIN, LOW);

  if (isAuthorized(cardUID)) {
    // Desligar LED vermelho após autenticação bem-sucedida (verde continua)
    digitalWrite(DENIED_LED, LOW);
    Serial.println(F(">>> ACESSO PERMITIDO! <<<"));
    sendAccessToServer(cardUID, "acesso_concedido", "Usuario");

    // Se porta está fechada, ativar sistema de timeout de 10 segundos
    if (!doorOpen) {
      Serial.println(F("🚪 Porta fechada. LED verde ficará aceso por 10 segundos."));
      accessGrantedWhileClosed = true;
      accessGrantedTime = millis();
      personEnteredAfterAccess = false;

      Serial.print(F("DEBUG - accessGrantedWhileClosed: "));
      Serial.println(accessGrantedWhileClosed);
      Serial.print(F("DEBUG - doorOpen: "));
      Serial.println(doorOpen);
      Serial.print(F("DEBUG - accessGrantedTime: "));
      Serial.println(accessGrantedTime);

      // Manter LED verde aceso
      digitalWrite(ACTUATOR_PIN, HIGH);
      digitalWrite(RELAY_PIN, HIGH);
      delay(100);
      digitalWrite(RELAY_PIN, LOW);
      // Não desligar o ACTUATOR_PIN aqui, pois ele deve ficar aceso por 10 segundos
    } else {
      // Porta está aberta - autorizar normalmente
      accessAuthorized = true;
      personEnteredAfterAccess = true;

      // Desligar buzzer e LED vermelho imediatamente se estava apitando
      digitalWrite(BUZZER_PIN, LOW);
      digitalWrite(DENIED_LED, LOW);
      digitalWrite(ACTUATOR_PIN, HIGH);  // Ligar LED verde imediatamente

      digitalWrite(RELAY_PIN, HIGH);
      delay(100);
      digitalWrite(RELAY_PIN, LOW);
      // LED verde fica aceso (controlado por handleDoorAlerts)
    }
  } else {
    // Desligar LED verde após autenticação falhar (vermelho continua)
    digitalWrite(ACTUATOR_PIN, LOW);

    Serial.println(F(">>> ACESSO NEGADO! <<<"));
    sendAccessToServer(cardUID, "acesso_negado", "Desconhecido");

    // LED vermelho já está aceso, apenas ativar buzzer
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
