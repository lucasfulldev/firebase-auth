# 🧪 Teste da Aplicação no Vercel

Sua aplicação está rodando em: **https://firebase-auth-three-sigma.vercel.app**

## ✅ Testes Rápidos

### 1. Verificar se o servidor está rodando

```bash
curl https://firebase-auth-three-sigma.vercel.app/health
```

Resposta esperada:
```json
{
  "status": "OK",
  "timestamp": "2025-11-01T20:01:45.491Z",
  "message": "Servidor Node.js está rodando"
}
```

### 2. Ver informações da API

```bash
curl https://firebase-auth-three-sigma.vercel.app/
```

Você verá todos os endpoints disponíveis.

### 3. Enviar dados de teste

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30
  }'
```

Resposta esperada (se Firebase estiver configurado):
```json
{
  "success": true,
  "message": "Dados salvos com sucesso",
  "data": {
    "timestamp": 1730483505491,
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30,
    "mensagem": "Dados do ESP32"
  }
}
```

### 4. Ler últimos dados

```bash
curl https://firebase-auth-three-sigma.vercel.app/api/esp32/dados
```

### 5. Registrar um acesso (RFID)

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos \
  -H "Content-Type: application/json" \
  -d '{
    "cardId": "ABC123",
    "acao": "acesso_concedido",
    "usuario": "João"
  }'
```

### 6. Ver acessos registrados

```bash
curl https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos
```

## 📱 Usar no ESP32

Atualize o código do ESP32 com a URL:

```cpp
const char* serverUrl = "https://firebase-auth-three-sigma.vercel.app";
```

No arquivo `esp32-node-gateway.ino`:

```cpp
void sendDataToNodeJS() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não está conectado!");
    return;
  }

  HTTPClient http;

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["temperatura"] = random(20, 35);
  jsonDoc["umidade"] = random(40, 80);
  jsonDoc["rssi"] = WiFi.RSSI();
  jsonDoc["mensagem"] = "Dados do ESP32";

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  String url = String(serverUrl) + "/api/esp32/dados";

  Serial.println("Enviando para: " + url);

  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    Serial.println("✓ Sucesso!");
  } else {
    Serial.println("✗ Erro: " + String(httpResponseCode));
  }

  http.end();
}
```

## 🔍 Possíveis Problemas

### Erro: "Firebase not initialized"
- As variáveis de ambiente não foram configuradas no Vercel
- Siga o guia [VERCEL_ENV_CHECKLIST.md](VERCEL_ENV_CHECKLIST.md)

### Erro: "404 NOT FOUND"
- O Vercel está fazendo redeploy
- Aguarde 2-3 minutos e tente novamente

### Erro: "CORS error" no ESP32
- CORS está habilitado, não deve haver problema
- Se persistir, adicione header adicional no ESP32

## 📊 Verificar Logs no Vercel

1. Vá para https://vercel.com/dashboard
2. Clique no projeto `firebase-auth`
3. Clique em **"Deployments"**
4. Selecione o deployment mais recente
5. Vá para **"Logs"** ou **"Function Logs"**
6. Procure por erros específicos

## ✨ Próximos Passos

1. ✅ Fazer upload do código no ESP32
2. ✅ Abrir Serial Monitor (115200 baud)
3. ✅ Verificar dados chegando no Firebase Console
4. ✅ Criar dashboard para visualizar dados em tempo real

## 🎉 Sucesso!

Se todos os testes acima funcionarem, sua aplicação está **100% pronta** para o ESP32!

**URL para o ESP32 usar:**
```
https://firebase-auth-three-sigma.vercel.app
```
