# Firebase Gateway para ESP32

Intermediária segura entre ESP32 e Firebase Realtime Database usando Node.js.

## 📋 O que é?

Uma aplicação Node.js que funciona como gateway entre seu ESP32 (microcontrolador) e Firebase. O ESP32 envia dados simples via HTTP, e o servidor autentica e envia para Firebase de forma segura.

```
┌─────────┐       HTTP         ┌──────────────┐      Firebase      ┌──────────────┐
│  ESP32  │ ────────────────→ │ Node.js      │ ──────────────→ │ Firebase RT  │
│ (WiFi)  │      POST JSON    │ (Gateway)    │   Admin SDK    │   Database   │
└─────────┘                    └──────────────┘                └──────────────┘
```

## 🚀 Quick Start

### Opção 1: Local (Desenvolvimento)

```bash
# Instalar dependências
npm install

# Criar arquivo de credenciais
# (obtém em Firebase Console → Configurações → Contas de Serviço)
cp serviceAccountKey.json.example serviceAccountKey.json

# Iniciar servidor
npm start
```

Servidor rodará em `http://localhost:3000`

### Opção 2: Vercel (Produção)

Veja [VERCEL_DEPLOYMENT.md](VERCEL_DEPLOYMENT.md) para guia completo.

## 📚 Documentação

- **[SETUP_NODE_GATEWAY.md](SETUP_NODE_GATEWAY.md)** - Setup local com Node.js
- **[VERCEL_DEPLOYMENT.md](VERCEL_DEPLOYMENT.md)** - Deploy no Vercel
- **[ESP32_SETUP.md](ESP32_SETUP.md)** - Configuração do ESP32

## 🔌 Endpoints da API

### Enviar Dados
```
POST /api/esp32/dados
Content-Type: application/json

{
  "temperatura": 25,
  "umidade": 60,
  "rssi": -30,
  "mensagem": "Dados do ESP32"
}

Resposta:
{
  "success": true,
  "message": "Dados salvos com sucesso",
  "data": { ... }
}
```

### Registrar Acesso (RFID)
```
POST /api/esp32/acessos
Content-Type: application/json

{
  "cardId": "ABC123",
  "acao": "acesso_concedido",
  "usuario": "João"
}

Resposta:
{
  "success": true,
  "message": "Acesso registrado",
  "id": "-O1a2b3c4d5"
}
```

### Ler Dados
```
GET /api/esp32/dados

Resposta:
{
  "success": true,
  "data": {
    "timestamp": 1635789456000,
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30,
    "mensagem": "Dados do ESP32"
  }
}
```

### Ver Acessos
```
GET /api/esp32/acessos

Resposta:
{
  "success": true,
  "data": {
    "-O1a2b3c4d5": { ... },
    "-O6e7f8g9h0": { ... }
  }
}
```

### Status
```
GET /health

Resposta:
{
  "status": "OK",
  "timestamp": "2024-01-15T10:30:45.123Z",
  "message": "Servidor Node.js está rodando"
}
```

## 🔑 Configuração de Credenciais

### Local (desenvolvimento)

1. Obter Service Account Key no Firebase Console
2. Salvar como `serviceAccountKey.json`
3. Arquivo será carregado automaticamente

⚠️ **Nunca commite este arquivo no Git!**

### Vercel (produção)

Variáveis de ambiente são configuradas no dashboard Vercel:

```
FIREBASE_TYPE=service_account
FIREBASE_PROJECT_ID=seu-projeto
FIREBASE_PRIVATE_KEY=...
FIREBASE_CLIENT_EMAIL=...
FIREBASE_DATABASE_URL=...
```

## 💻 Código ESP32

Use o arquivo `esp32-node-gateway.ino`:

```cpp
const char* serverUrl = "http://192.168.1.100:3000"; // Local
// ou
const char* serverUrl = "https://seu-projeto.vercel.app"; // Produção
```

## 📁 Estrutura de Arquivos

```
firebase-auth/
├── api/
│   ├── handler.js          # Lógica serverless (Vercel)
│   └── index.js            # Entrada Vercel
├── server.js               # Servidor Express (local)
├── package.json            # Dependências Node.js
├── vercel.json             # Configuração Vercel
├── .gitignore              # Arquivos ignorados por Git
├── esp32-node-gateway.ino  # Código Arduino para ESP32
├── README.md               # Este arquivo
├── SETUP_NODE_GATEWAY.md   # Guia setup local
└── VERCEL_DEPLOYMENT.md    # Guia deploy Vercel
```

## 🔒 Segurança

### ✅ Boas Práticas Implementadas

- ✅ Credenciais Firebase nunca são expostas
- ✅ CORS habilitado apenas em produção
- ✅ Validação de entrada de dados
- ✅ Admin SDK para operações seguras
- ✅ Timestamps do servidor (não confia no ESP32)

### 🛡️ Para Produção Adicione

```javascript
// Autenticação com API Key
app.use((req, res, next) => {
  const apiKey = req.headers['x-api-key'];
  if (apiKey !== process.env.API_KEY) {
    return res.status(401).json({ error: 'Unauthorized' });
  }
  next();
});

// Rate limiting
const rateLimit = require('express-rate-limit');
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 100
});
app.use(limiter);
```

## 🧪 Testes

### Testar Localmente

```bash
# Terminal 1: Iniciar servidor
npm start

# Terminal 2: Enviar dados de teste
curl -X POST http://localhost:3000/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30
  }'

# Verificar no Firebase Console
# Realtime Database → devices → esp32-test
```

### Com ESP32 Real

1. Atualizar `serverUrl` no código
2. Fazer upload para ESP32
3. Abrir Serial Monitor (115200 baud)
4. Verificar logs e dados no Firebase

## 📊 Estrutura de Dados Firebase

```json
{
  "devices": {
    "esp32-test": {
      "timestamp": 1635789456000,
      "temperatura": 25,
      "umidade": 60,
      "rssi": -30,
      "mensagem": "Dados do ESP32"
    }
  },
  "acessos": {
    "-O1a2b3c4d5": {
      "cardId": "ABC123",
      "acao": "acesso_concedido",
      "usuario": "João",
      "timestamp": 1635789500000
    },
    "-O6e7f8g9h0": { ... }
  }
}
```

## 🐛 Troubleshooting

### "Cannot find module 'firebase-admin'"
```bash
npm install
```

### "ENOENT: serviceAccountKey.json not found"
- Obter em Firebase Console → Configurações → Contas de Serviço
- Renomear para `serviceAccountKey.json`
- Adicionar ao `.gitignore`

### "CORS error" no ESP32
Verifique se CORS está habilitado no `server.js`:
```javascript
app.use(cors());
```

### Vercel: "Invalid Private Key"
A variável de ambiente `FIREBASE_PRIVATE_KEY` precisa incluir as quebras de linha:
```
-----BEGIN PRIVATE KEY-----\nMIIE...\n-----END PRIVATE KEY-----\n
```

## 📈 Próximos Passos

- [ ] Dashboard React em tempo real
- [ ] Notificações por email
- [ ] Autenticação de usuários
- [ ] Gráficos de temperatura/umidade
- [ ] Banco de dados PostgreSQL para histórico
- [ ] WebSocket para updates em tempo real

## 📝 Licença

MIT

## 👨‍💻 Autor

Lucas

---

**Precisa de ajuda?** Verifique a documentação completa nos arquivos .md
