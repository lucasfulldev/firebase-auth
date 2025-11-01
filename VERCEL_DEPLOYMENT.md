# Deploy no Vercel - Guia Completo

Vercel permite fazer deploy de aplicações Node.js gratuitamente. Seu ESP32 poderá enviar dados de qualquer lugar do mundo!

```
ESP32 (Qualquer WiFi)
    ↓
    ↓ HTTPS
    ↓
[Vercel URL] → https://seu-projeto.vercel.app/api/esp32/dados
    ↓
    ↓ (Admin SDK)
    ↓
Firebase Realtime Database
```

## Pré-requisitos

- Conta no GitHub (gratuita)
- Conta no Vercel (gratuita)
- Service Account Key do Firebase (arquivo JSON)

## Passo 1: Preparar o Projeto Localmente

Seu projeto já está pronto! Verifique se tem:
- `server.js` (seu servidor Express)
- `api/handler.js` (versão serverless)
- `api/index.js`
- `package.json`
- `vercel.json` (arquivo de configuração)

## Passo 2: Criar Repositório no GitHub

### 2.1: Fazer Login no GitHub
Vá para https://github.com e faça login (ou crie conta)

### 2.2: Criar Novo Repositório
1. Clique em **"+"** (canto superior direito)
2. Selecione **"New repository"**
3. Nome: `firebase-auth` (ou outro)
4. Descrição: `Firebase Gateway para ESP32`
5. Deixe **"Public"** (pode deixar Private depois)
6. Clique **"Create repository"**

### 2.3: Enviar Código para GitHub

No seu terminal, na pasta do projeto:

```bash
cd /home/lucas/firebase-auth

# Inicializar git (se não tiver)
git init

# Adicionar origem do repositório
git remote add origin https://github.com/SEU_USUARIO/firebase-auth.git

# Criar arquivo .gitignore (para não enviar credenciais)
echo "node_modules/" > .gitignore
echo "serviceAccountKey.json" >> .gitignore
echo ".env" >> .gitignore
echo ".env.local" >> .gitignore

# Adicionar todos os arquivos
git add .

# Fazer commit
git commit -m "Initial commit: Firebase Gateway para ESP32"

# Enviar para GitHub
git branch -M main
git push -u origin main
```

## Passo 3: Obter Credenciais Firebase (Variáveis de Ambiente)

Você vai adicionar as credenciais do Firebase como variáveis de ambiente no Vercel (não quer colocar arquivo sensível no GitHub).

### 3.1: Obter Service Account Key

1. **Firebase Console** → **Configurações ⚙️**
2. Aba **"Contas de Serviço"**
3. Clique **"Gerar nova chave privada"**
4. Um JSON será baixado

### 3.2: Extrair Valores do JSON

Abra o arquivo JSON baixado e copie estes valores:

```json
{
  "type": "service_account",
  "project_id": "controle-de-acesso-tel-rfid2",
  "private_key_id": "abc123...",
  "private_key": "-----BEGIN PRIVATE KEY-----\nMIIE...\n-----END PRIVATE KEY-----\n",
  "client_email": "firebase-adminsdk-abc@controle-de-acesso-tel-rfid2.iam.gserviceaccount.com",
  "client_id": "123456789",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://oauth2.googleapis.com/token",
  "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
  "client_x509_cert_url": "https://www.googleapis.com/..."
}
```

## Passo 4: Fazer Deploy no Vercel

### 4.1: Conectar Vercel ao GitHub

1. Vá para https://vercel.com
2. Clique **"Sign Up"** (ou faça login)
3. Selecione **"Continue with GitHub"**
4. Autorize Vercel a acessar seu GitHub
5. Selecione seu repositório `firebase-auth`

### 4.2: Configurar Variáveis de Ambiente

No painel do Vercel, antes de fazer deploy:

1. Clique em **"Environment Variables"**
2. Adicione cada variável do seu JSON do Firebase:

```
FIREBASE_TYPE = service_account
FIREBASE_PROJECT_ID = controle-de-acesso-tel-rfid2
FIREBASE_PRIVATE_KEY_ID = abc123...
FIREBASE_PRIVATE_KEY = -----BEGIN PRIVATE KEY-----\nMIIE...\n-----END PRIVATE KEY-----\n
FIREBASE_CLIENT_EMAIL = firebase-adminsdk-abc@controle-de-acesso-tel-rfid2.iam.gserviceaccount.com
FIREBASE_CLIENT_ID = 123456789
FIREBASE_AUTH_URI = https://accounts.google.com/o/oauth2/auth
FIREBASE_TOKEN_URI = https://oauth2.googleapis.com/token
FIREBASE_AUTH_PROVIDER_CERT_URL = https://www.googleapis.com/oauth2/v1/certs
FIREBASE_CLIENT_CERT_URL = https://www.googleapis.com/...
FIREBASE_DATABASE_URL = https://controle-de-acesso-tel-rfid2-default-rtdb.asia-southeast1.firebasedatabase.app
```

⚠️ **IMPORTANTE**: Para `FIREBASE_PRIVATE_KEY`, copie exatamente como está no JSON, incluindo `\n` (quebras de linha).

### 4.3: Fazer Deploy

1. Clique em **"Deploy"**
2. Aguarde o build terminar (normalmente 30-60 segundos)
3. Você receberá uma URL como: `https://seu-projeto.vercel.app`

## Passo 5: Atualizar Código do ESP32

Seu ESP32 agora deve enviar dados para a URL do Vercel:

### Editar `esp32-node-gateway.ino`

Mude esta linha:
```cpp
// Antes (local):
const char* serverUrl = "http://192.168.X.X:3000";

// Depois (Vercel):
const char* serverUrl = "https://seu-projeto.vercel.app";
```

Exemplo completo:
```cpp
const char* serverUrl = "https://firebase-gateway-esp32.vercel.app";
```

## Passo 6: Testar

### Teste 1: Verificar se servidor está rodando

No navegador, vá para: `https://seu-projeto.vercel.app/health`

Você deve ver:
```json
{
  "status": "OK",
  "timestamp": "2024-01-15T10:30:45.123Z",
  "message": "Servidor Node.js está rodando no Vercel"
}
```

### Teste 2: Enviar dados manualmente

```bash
curl -X POST https://seu-projeto.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{"temperatura":25,"umidade":60,"rssi":-30}'
```

Você deve receber:
```json
{
  "success": true,
  "message": "Dados salvos com sucesso",
  "data": {
    "timestamp": 1634567890123,
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30,
    "mensagem": "Dados do ESP32"
  }
}
```

### Teste 3: ESP32 Real

1. Atualize o código do ESP32 com a URL do Vercel
2. Faça upload
3. Abra Serial Monitor
4. Você deve ver mensagens de sucesso
5. Verifique no Firebase Console se dados chegaram

## Estrutura de Arquivos Final

```
firebase-auth/
├── api/
│   ├── handler.js          (lógica serverless)
│   └── index.js            (entrada do Vercel)
├── server.js               (servidor local alternativo)
├── package.json
├── vercel.json             (configuração Vercel)
├── .gitignore
├── esp32-node-gateway.ino  (código ESP32 atualizado)
├── SETUP_NODE_GATEWAY.md
├── VERCEL_DEPLOYMENT.md
└── README.md
```

## Troubleshooting

### Erro: "Cannot find module 'firebase-admin'"

1. Verifique seu `package.json` tem:
```json
{
  "dependencies": {
    "express": "^4.18.2",
    "firebase-admin": "^11.0.0",
    "cors": "^2.8.5"
  }
}
```

2. Se não tiver, execute localmente:
```bash
npm install
```

3. Faça commit e push para GitHub:
```bash
git add package.json package-lock.json
git commit -m "update dependencies"
git push
```

### Erro: "Invalid Private Key"

A variável `FIREBASE_PRIVATE_KEY` não foi copiada corretamente. Certifique-se de:
1. Copiar exatamente como está no JSON original
2. Incluir as quebras de linha (`\n`)
3. Remover qualquer espaço extra no início/fim

### ESP32 recebe erro 403/401

A autenticação Firebase falhou. Verifique:
1. As variáveis de ambiente estão corretas
2. As Regras do Firebase permitem escrita (temporariamente):
```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

### Vercel mostra erro na página

Verifique os logs do Vercel:
1. Painel do Vercel → Seu projeto
2. Aba **"Deployments"**
3. Clique na deployment mais recente
4. Aba **"Function Logs"** ou **"Build Logs"**

## Dicas de Segurança

Para produção, proteja seu endpoint:

### Adicionar Autenticação

```javascript
app.post('/api/esp32/dados', (req, res) => {
  // Verificar token
  const token = req.headers['x-api-key'];
  if (token !== process.env.API_KEY) {
    return res.status(401).json({ error: 'Unauthorized' });
  }
  // ... resto do código
});
```

Então no ESP32:
```cpp
http.addHeader("x-api-key", "sua-chave-secreta-aqui");
```

### Limitar Taxa de Requisições

Use middleware como `express-rate-limit`.

## Próximos Passos

1. Dashboard em tempo real (React + Firebase)
2. Notificações por email
3. Autenticação de usuários
4. Gráficos de temperatura/umidade

Seu servidor está no ar! 🚀
