# 📱 Integração com Telegram - Confirmação de Cadastro RFID

Seu sistema agora pode enviar notificações ao Telegram pedindo confirmação antes de cadastrar novos cartões RFID!

## 🔄 Fluxo

```
ESP32 detecta novo cartão
    ↓
ESP32 entra em modo de cadastro (10s no botão)
    ↓
ESP32 lê UID do cartão
    ↓
ESP32 envia para Node.js: "Confirmar cadastro?"
    ↓
Node.js armazena requisição no Firebase
    ↓
Bot Telegram envia mensagem para você
    ↓
Você responde "1" (confirmar) ou "0" (cancelar)
    ↓
Bot atualiza Firebase com sua resposta
    ↓
ESP32 verifica resposta
    ↓
SE confirmado: Cadastra cartão ✅
SE cancelado: Descarta cartão ❌
```

## 🤖 Configuração do Bot Telegram

### Passo 1: Criar o Bot

1. Abra o Telegram
2. Procure por **@BotFather**
3. Clique em "Iniciar" ou envie `/start`
4. Envie `/newbot`
5. Escolha um nome (ex: "RFID Access Bot")
6. Escolha um username (ex: "rfid_access_telematica_bot")
7. **Copie o token** fornecido (exemplo: `123456789:ABCdefGHijKlmnOPqrsTUVwxyz`)

### Passo 2: Obter seu Chat ID

1. Abra uma conversa com seu bot
2. Envie qualquer mensagem (ex: `/start` ou `oi`)
3. Abra seu navegador e acesse:
   ```
   https://api.telegram.org/bot<SEU_TOKEN>/getUpdates
   ```
   Substitua `<SEU_TOKEN>` pelo token do passo anterior

4. Na resposta, procure por `"chat"` e copie o número de `"id"`
   Exemplo: `"id": 123456789`

## 🔧 Configurar Variáveis de Ambiente

No seu servidor (ou Vercel), adicione as variáveis de ambiente:

```
TELEGRAM_BOT_TOKEN=123456789:ABCdefGHijKlmnOPqrsTUVwxyz
TELEGRAM_CHAT_ID=123456789
```

### No Vercel:

1. Dashboard → Seu projeto → Settings
2. Environment Variables
3. Adicione:
   - Name: `TELEGRAM_BOT_TOKEN`, Value: seu token
   - Name: `TELEGRAM_CHAT_ID`, Value: seu Chat ID
4. Save
5. Redeploy

### Localmente:

Crie um arquivo `.env`:

```env
TELEGRAM_BOT_TOKEN=123456789:ABCdefGHijKlmnOPqrsTUVwxyz
TELEGRAM_CHAT_ID=123456789
FIREBASE_PROJECT_ID=controle-de-acesso-tel-rfid2
FIREBASE_PRIVATE_KEY=...
# ... outras variáveis
```

## 📝 Como Usar

### Modo Cadastro:

1. **Mantenha o botão pressionado por 10 segundos**
   - LED de indicação começará a piscar
   - Serial Monitor: "MODO DE CADASTRO ATIVADO"

2. **Aproxime um novo cartão RFID**
   - ESP32 lerá o UID
   - Serial Monitor: "Enviando confirmação Telegram..."

3. **Verifique o Telegram**
   - Você receberá uma mensagem do bot:
   ```
   🔐 Novo Cartão RFID Detectado

   Card ID: 1A2B3C4D
   Timestamp: 01/11/2025 17:30:45

   Deseja cadastrar este cartão?
   Responda:
   • 1 para confirmar e cadastrar
   • 0 para cancelar
   ```

4. **Responda no Telegram**
   - Envie `1` para **confirmar e cadastrar**
   - Envie `0` para **cancelar**

5. **Resultado**
   - **Se respondeu 1**: Cartão é cadastrado, feedback de sucesso (LED + som)
   - **Se respondeu 0**: Cartão é descartado, feedback de cancelamento
   - Você recebe notificação de confirmação no Telegram

## 📲 Exemplos de Mensagens

### Pedido de Confirmação

```
🔐 Novo Cartão RFID Detectado

Card ID: 1A2B3C4D
Timestamp: 01/11/2025 17:30:45

Deseja cadastrar este cartão?

Responda:
• 1 para confirmar e cadastrar
• 0 para cancelar
```

### Confirmado

```
✅ Cadastro Confirmado

Card: 1A2B3C4D
```

### Cancelado

```
❌ Cadastro Cancelado

Card: 1A2B3C4D
```

## 🧪 Testes

### Teste 1: Verificar Bot Telegram

```bash
curl "https://api.telegram.org/bot<SEU_TOKEN>/getMe"
```

Deve retornar informações do bot.

### Teste 2: Simular Requisição ESP32

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/telegram/ask \
  -H "Content-Type: application/json" \
  -d '{"cardUID": "TEST1234", "action": "ask_confirmation"}'
```

Resposta esperada:

```json
{
  "success": true,
  "registrationId": "TEST1234_1730483505491",
  "message": "Confirmação enviada para Telegram"
}
```

### Teste 3: Verificar Status

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/telegram/check \
  -H "Content-Type: application/json" \
  -d '{"registrationId": "TEST1234_1730483505491"}'
```

Resposta esperada (enquanto aguarda):

```json
{
  "status": "waiting",
  "message": "Aguardando resposta do Telegram"
}
```

### Teste 4: Simular Resposta Telegram

```bash
curl -X POST https://firebase-auth-three-sigma.vercel.app/api/esp32/telegram/respond \
  -H "Content-Type: application/json" \
  -d '{"registrationId": "TEST1234_1730483505491", "response": "1"}'
```

## 📁 Código Usado

### Arquivo do ESP32:

[esp32-rfid-telegram.ino](esp32-rfid-telegram.ino)

### Integração Node.js:

Endpoints adicionados em `server.js`:
- `POST /api/esp32/telegram/ask` - Pedir confirmação
- `POST /api/esp32/telegram/check` - Verificar resposta
- `POST /api/esp32/telegram/respond` - Registrar resposta

## 🔒 Segurança

⚠️ **Importante:**
- Nunca envie tokens em mensagens públicas
- Guarde o `TELEGRAM_BOT_TOKEN` seguro
- Use variáveis de ambiente, nunca código-fonte

## 🐛 Troubleshooting

### Bot não envia mensagens

1. Verifique `TELEGRAM_BOT_TOKEN`
2. Verifique `TELEGRAM_CHAT_ID`
3. Teste manualmente:
   ```bash
   curl "https://api.telegram.org/bot<TOKEN>/sendMessage?chat_id=<CHAT_ID>&text=Teste"
   ```

### Timeout na resposta

- Aumentar `REGISTRATION_TIMEOUT` em `esp32-rfid-telegram.ino`
- Padrão: 5 minutos (300000 ms)

### Cartão não é cadastrado

1. Verificar Serial Monitor do ESP32
2. Verificar WiFi está conectado
3. Verificar Firebase está configurado
4. Testar manualmente com curl acima

## 📈 Estrutura de Dados Firebase

Após usar Telegram, você verá em Firebase:

```json
{
  "registrations": {
    "1A2B3C4D_1730483505491": {
      "cardUID": "1A2B3C4D",
      "status": "confirmed",
      "response": "1",
      "timestamp": 1730483505491,
      "respondedAt": 1730483510000
    }
  },
  "acessos": {
    "-O1a2b3c4d5": {
      "cardId": "1A2B3C4D",
      "acao": "cartao_cadastrado",
      "usuario": "Nova Pessoa",
      "timestamp": 1730483510000
    }
  }
}
```

## ✅ Checklist de Configuração

- [ ] Bot Telegram criado (@BotFather)
- [ ] Token do bot copiado
- [ ] Chat ID obtido
- [ ] Variáveis de ambiente adicionadas (Vercel ou .env)
- [ ] `esp32-rfid-telegram.ino` faz upload no ESP32
- [ ] Testado com curl acima
- [ ] Testado com cartão real

## 🎯 Próximas Melhorias

- [ ] Notificações de acesso via Telegram
- [ ] Histórico de cartões no Telegram
- [ ] Bloqueio/desbloqueio via Telegram
- [ ] Alertas de acesso negado
- [ ] Relatórios diários via Telegram

Tudo pronto! 🚀
