# 🧪 Exemplos de Curl para Testar a API

Copie e cole os comandos abaixo no terminal para testar sua API.

**URL base:** `https://firebase-auth-three-sigma.vercel.app`

## 1️⃣ Testes de Status

### Verificar se o servidor está online

```bash
curl https://firebase-auth-three-sigma.vercel.app/health
```

**Resposta esperada:**
```json
{
  "status": "OK",
  "timestamp": "2025-11-01T20:01:45.491Z",
  "message": "Servidor Node.js está rodando"
}
```

### Ver informações completas da API

```bash
curl https://firebase-auth-three-sigma.vercel.app/
```

**Resposta esperada:**
```json
{
  "name": "Firebase Gateway - ESP32",
  "version": "1.0.0",
  "description": "Servidor intermediário entre ESP32 e Firebase Realtime Database",
  "status": "online",
  "endpoints": { ... }
}
```

---

## 2️⃣ Enviar Dados do ESP32

### Enviar dados simples

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30
  }'
```

### Enviar dados com mensagem personalizada

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{
    "temperatura": 28,
    "umidade": 75,
    "rssi": -22,
    "mensagem": "Sensor da sala"
  }'
```

### Enviar apenas temperatura

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 30, "umidade": 50, "rssi": -25}'
```

**Resposta esperada (se Firebase estiver configurado):**
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

---

## 3️⃣ Ler Dados Armazenados

### Ler últimos dados do ESP32

```bash
curl https://firebase-auth-three-sigma.vercel.app/api/esp32/dados
```

**Resposta esperada:**
```json
{
  "success": true,
  "data": {
    "timestamp": 1730483505491,
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30,
    "mensagem": "Dados do ESP32"
  }
}
```

---

## 4️⃣ Controle de Acesso (RFID)

### Registrar um acesso bem-sucedido

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos \
  -H "Content-Type: application/json" \
  -d '{
    "cardId": "ABC123",
    "acao": "acesso_concedido",
    "usuario": "João Silva"
  }'
```

### Registrar acesso negado

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos \
  -H "Content-Type: application/json" \
  -d '{
    "cardId": "INVALID",
    "acao": "acesso_negado",
    "usuario": "desconhecido"
  }'
```

### Registrar acesso com múltiplas informações

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos \
  -H "Content-Type: application/json" \
  -d '{
    "cardId": "DEF456",
    "acao": "acesso_concedido",
    "usuario": "Maria Santos"
  }'
```

**Resposta esperada:**
```json
{
  "success": true,
  "message": "Acesso registrado",
  "id": "-O1a2b3c4d5"
}
```

### Ler últimos 10 acessos

```bash
curl https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos
```

**Resposta esperada:**
```json
{
  "success": true,
  "data": {
    "-O1a2b3c4d5": {
      "cardId": "ABC123",
      "acao": "acesso_concedido",
      "usuario": "João Silva",
      "timestamp": 1730483505491
    },
    "-O6e7f8g9h0": {
      "cardId": "DEF456",
      "acao": "acesso_concedido",
      "usuario": "Maria Santos",
      "timestamp": 1730483510000
    }
  }
}
```

---

## 5️⃣ Testes com Valores Variados

### Teste 1: Temperatura alta

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 45, "umidade": 30, "rssi": -10}'
```

### Teste 2: Temperatura baixa

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 5, "umidade": 90, "rssi": -60}'
```

### Teste 3: Sinal WiFi fraco

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 22, "umidade": 55, "rssi": -80}'
```

### Teste 4: Sinal WiFi forte

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 22, "umidade": 55, "rssi": -5}'
```

---

## 6️⃣ Testes com Erros Intencionais

### Erro: Faltam campos obrigatórios

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 25}'
```

**Resposta esperada:**
```json
{
  "success": false,
  "error": "Faltam campos obrigatórios: temperatura, umidade, rssi"
}
```

### Erro: Registrar acesso sem cardId

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos \
  -H "Content-Type: application/json" \
  -d '{"acao": "acesso_concedido"}'
```

**Resposta esperada:**
```json
{
  "success": false,
  "error": "Faltam campos: cardId, acao"
}
```

---

## 7️⃣ Scripts para Teste Automático

### Enviar dados continuamente a cada 5 segundos (Linux/Mac)

```bash
while true; do
  curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/dados \
    -H "Content-Type: application/json" \
    -d "{\"temperatura\": $((RANDOM % 40 + 10)), \"umidade\": $((RANDOM % 100)), \"rssi\": $((RANDOM % 50 - 80))}"
  echo "✓ Dado enviado"
  sleep 5
done
```

### Verificar status a cada 10 segundos (Linux/Mac)

```bash
while true; do
  echo "=== Verificando status ==="
  curl https://firebase-auth-three-sigma.vercel.app/health
  echo ""
  sleep 10
done
```

### PowerShell (Windows)

```powershell
while ($true) {
    $temp = Get-Random -Minimum 10 -Maximum 40
    $umid = Get-Random -Minimum 0 -Maximum 100
    $rssi = Get-Random -Minimum -80 -Maximum 0

    $body = @{
        temperatura = $temp
        umidade = $umid
        rssi = $rssi
    } | ConvertTo-Json

    Invoke-WebRequest -Uri "https://firebase-auth-three-sigma.vercel.app/api/esp32/dados" `
        -Method Post `
        -Headers @{"Content-Type"="application/json"} `
        -Body $body

    Write-Host "✓ Dado enviado: Temp=$temp, Umid=$umid"
    Start-Sleep -Seconds 5
}
```

---

## 8️⃣ Usando com jq (para JSON formatado)

Se tiver `jq` instalado, pode formatar a resposta:

### Instalar jq

**Linux:**
```bash
sudo apt-get install jq
```

**Mac:**
```bash
brew install jq
```

**Windows:** Download em https://stedolan.github.io/jq/

### Usar jq para formatar resposta

```bash
curl https://firebase-auth-three-sigma.vercel.app/ | jq .
```

```bash
curl https://firebase-auth-three-sigma.vercel.app/api/esp32/dados | jq .
```

```bash
curl https://firebase-auth-three-sigma.vercel.app/api/esp32/acessos | jq .
```

---

## 9️⃣ Usando com Postman

Se preferir GUI ao invés de linha de comando:

1. Baixe Postman em https://www.postman.com/downloads/
2. Crie uma nova requisição
3. Defina como **POST**
4. Cole a URL: `https://firebase-auth-three-sigma.vercel.app/api/esp32/dados`
5. Vá para **Body** → **raw** → selecione **JSON**
6. Cole:
```json
{
  "temperatura": 25,
  "umidade": 60,
  "rssi": -30
}
```
7. Clique em **Send**

---

## 🔟 Verificar Tudo de Uma Vez

Script que testa todos os endpoints:

```bash
#!/bin/bash

BASE_URL="https://firebase-auth-three-sigma.vercel.app"

echo "📋 Testando API do Firebase Gateway"
echo ""

echo "1️⃣ Verificando status..."
curl $BASE_URL/health
echo -e "\n"

echo "2️⃣ Informações da API..."
curl $BASE_URL/
echo -e "\n"

echo "3️⃣ Enviando dados..."
curl -X POST $BASE_URL/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura": 25, "umidade": 60, "rssi": -30}'
echo -e "\n"

echo "4️⃣ Lendo dados..."
curl $BASE_URL/api/esp32/dados
echo -e "\n"

echo "5️⃣ Registrando acesso..."
curl -X POST $BASE_URL/api/esp32/acessos \
  -H "Content-Type: application/json" \
  -d '{"cardId": "TEST123", "acao": "acesso_concedido", "usuario": "Teste"}'
echo -e "\n"

echo "6️⃣ Lendo acessos..."
curl $BASE_URL/api/esp32/acessos
echo -e "\n"

echo "✅ Testes concluídos!"
```

Salve como `test-api.sh` e execute:
```bash
chmod +x test-api.sh
./test-api.sh
```

---

## 🆘 Troubleshooting

### Erro: "Couldn't resolve host name"
- Verifique sua conexão com internet
- Verifique a URL está correta

### Erro: "Connection refused"
- O servidor Vercel pode estar em redeploy
- Aguarde 1-2 minutos e tente novamente

### Erro: "Firebase not initialized"
- As variáveis de ambiente não foram configuradas
- Siga [VERCEL_ENV_CHECKLIST.md](VERCEL_ENV_CHECKLIST.md)

### Sucesso mas dados não aparecem no Firebase
- Verifique se `FIREBASE_DATABASE_URL` está configurado
- Verifique as Regras do Firebase permitem escrita
- Confirme no Firebase Console → Realtime Database

---

## 💡 Dica

Para copiar e colar facilmente, use:

```bash
# Copiar resposta para clipboard (Mac)
curl https://firebase-auth-three-sigma.vercel.app/ | pbcopy

# Copiar resposta para clipboard (Linux)
curl https://firebase-auth-three-sigma.vercel.app/ | xclip -selection clipboard

# Copiar resposta para clipboard (Windows PowerShell)
curl https://firebase-auth-three-sigma.vercel.app/ | Set-Clipboard
```

Bom teste! 🚀
