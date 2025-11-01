# 🔐 Integração ESP32 RFID com Node.js Gateway

Seu código ESP32 agora está integrado com o servidor Node.js Gateway!

## 📋 O que foi alterado

### Antes (Firebase direto)
```cpp
❌ #include <FirebaseESP32.h>
❌ FirebaseData fbdo;
❌ Firebase.begin(&config, &auth);
```

### Agora (Node.js Gateway)
```cpp
✅ #include <HTTPClient.h>
✅ #include <ArduinoJson.h>
✅ Envia para: https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos
```

## 🔄 Fluxo de Dados

```
┌─────────────────┐
│  ESP32 + RFID   │
│  - Lê cartão    │
│  - Registra     │
└────────┬────────┘
         │ HTTP POST
         ↓
┌────────────────────────────────┐
│  Node.js Gateway (Vercel)      │
│  - Recebe dados                │
│  - Valida dados                │
│  - Envia para Firebase         │
└────────┬───────────────────────┘
         │ Admin SDK
         ↓
┌─────────────────────────────────┐
│  Firebase Realtime Database     │
│  - Armazena acessos             │
│  - Armazena cartões             │
└─────────────────────────────────┘
```

## 📝 Funções Principais

### 1. Enviar Acesso (Login/Logout)

```cpp
sendAccessToServer(String cardID, String acao, String usuario);
```

**Exemplo:**
```cpp
sendAccessToServer("1A2B3C4D", "acesso_concedido", "João Silva");
```

**Enviará para:**
```json
POST /api/esp32/acessos
{
  "cardId": "1A2B3C4D",
  "acao": "acesso_concedido",
  "usuario": "João Silva"
}
```

### 2. Registrar Novo Cartão

```cpp
saveCardToServer(String cardUID, String usuario);
```

**Exemplo:**
```cpp
saveCardToServer("5E6F7A8B", "Nova Pessoa");
```

### 3. Carregar Cartões Autorizados

```cpp
loadAuthorizedCardsFromServer();
```

Chamada automaticamente no `setup()`.

## 🔧 Configuração

### URL do Servidor

No arquivo, mude esta linha com sua URL Vercel:

```cpp
const char* serverUrl = "https://firebase-auth-three-sigma.vercel.app";
```

### WiFi

```cpp
#define WIFI_SSID "lucasnote"      // Seu WiFi
#define WIFI_PASSWORD "123456789"  // Sua senha
```

## 📊 Dados Enviados

### Acesso Concedido
```json
{
  "cardId": "1A2B3C4D",
  "acao": "acesso_concedido",
  "usuario": "João Silva",
  "timestamp": 1730483505491
}
```

### Acesso Negado
```json
{
  "cardId": "INVALID",
  "acao": "acesso_negado",
  "usuario": "Desconhecido",
  "timestamp": 1730483505491
}
```

### Cartão Cadastrado
```json
{
  "cardId": "5E6F7A8B",
  "acao": "cartao_cadastrado",
  "usuario": "Nova Pessoa",
  "timestamp": 1730483505491
}
```

## 🚀 Fazer Upload no ESP32

1. Abra `esp32-rfid-gateway.ino` no Arduino IDE
2. **Tools** → **Board** → Selecione **ESP32 Dev Module**
3. **Tools** → **Port** → Selecione a porta COM do ESP32
4. Clique em **Upload** (seta para a direita)
5. Abra **Serial Monitor** (115200 baud)

## 📱 Teste no Serial Monitor

Você verá algo como:

```
===== INICIANDO SISTEMA =====
Sistema de Acesso RFID com Gateway Node.js iniciado.

--- Iniciando conexão WiFi ---
Conectando à rede: lucasnote
.....
✓ CONECTADO AO WIFI COM SUCESSO!
SSID: lucasnote
IP: 192.168.43.125
Sinal: -22 dBm

--- Carregando cartões autorizados do servidor ---
✓ Resposta recebida do servidor:
{"success": true, "data": {...}}

Sistema pronto para uso.
```

## 🔐 Modo de Cadastro

1. **Segure o botão por 10 segundos**
2. LED de indicação começará a piscar
3. Aproxime um novo cartão RFID
4. Cartão será cadastrado
5. Sistema retorna ao modo normal

## ✅ Verificar no Firebase

1. Vá para **Firebase Console** → **Realtime Database**
2. Verifique a seção `/acessos`
3. Você deve ver algo como:

```json
{
  "acessos": {
    "-O1a2b3c4d5": {
      "cardId": "1A2B3C4D",
      "acao": "acesso_concedido",
      "usuario": "João Silva",
      "timestamp": 1730483505491
    }
  }
}
```

## 🐛 Troubleshooting

### Erro: "WiFi desconectado"
- Verifique SSID e senha
- Verifique se o ESP32 consegue se conectar (tente outro WiFi)

### Erro: "Não é possível enviar acesso"
- Verifique se o servidor Vercel está online
- Teste a URL no navegador: `https://firebase-auth-three-sigma.vercel.app/health`

### Cartão não é reconhecido
- Verifique as conexões SPI (SCK, MOSI, MISO)
- Verifique os pinos RST e SS
- Teste com o exemplo original da biblioteca MFRC522

### Dados não aparecem no Firebase
- Verifique as variáveis de ambiente no Vercel
- Verifique as Regras do Firebase (devem permitir escrita)
- Verifique os logs do Vercel

## 📈 Estrutura de Dados Final no Firebase

```json
{
  "devices": {
    "esp32-test": {
      "timestamp": 1730483505491,
      "temperatura": 25,
      "umidade": 60,
      "rssi": -30,
      "mensagem": "Dados do ESP32"
    }
  },
  "acessos": {
    "-O1a2b3c4d5": {
      "cardId": "1A2B3C4D",
      "acao": "acesso_concedido",
      "usuario": "João Silva",
      "timestamp": 1730483505491
    },
    "-O6e7f8g9h0": {
      "cardId": "5E6F7A8B",
      "acao": "cartao_cadastrado",
      "usuario": "Nova Pessoa",
      "timestamp": 1730483510000
    },
    "-O2k3l4m5n6": {
      "cardId": "INVALID",
      "acao": "acesso_negado",
      "usuario": "Desconhecido",
      "timestamp": 1730483515000
    }
  }
}
```

## 🎯 Próximas Implementações

- [ ] Integrar com sensor de temperatura/umidade
- [ ] Notificações por email para acessos negados
- [ ] Dashboard em tempo real
- [ ] Histórico de acessos com filtros
- [ ] Controle de horários (acesso permitido apenas em certos horários)
- [ ] Bloqueio automático de cartões suspeitos

## 📞 Suporte

Se tiver problemas:

1. Verifique o Serial Monitor para logs
2. Teste os endpoints com curl (veja CURL_EXAMPLES.md)
3. Verifique as Regras do Firebase
4. Verifique as variáveis de ambiente no Vercel

Tudo funcionando! 🚀
