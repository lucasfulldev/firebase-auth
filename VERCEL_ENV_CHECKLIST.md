# ✅ Checklist de Variáveis de Ambiente - Vercel

## 📋 O Problema Atual

O erro `Cannot find module './serviceAccountKey.json'` significa que as variáveis de ambiente **NÃO foram configuradas** no Vercel.

## 🔧 Solução: Adicionar Variáveis de Ambiente

### Passo 1: Obter o Arquivo JSON do Firebase

1. **Firebase Console** → Seu projeto
2. **Configurações ⚙️** (canto superior direito)
3. Aba **"Contas de Serviço"**
4. Clique **"Gerar nova chave privada"**
5. Um arquivo será baixado (exemplo: `controle-de-acesso-tel-rfid2-xxxxx.json`)

### Passo 2: Abrir o Arquivo JSON

Abra o arquivo com um editor de texto e você verá:

```json
{
  "type": "service_account",
  "project_id": "controle-de-acesso-tel-rfid2",
  "private_key_id": "abc123def456...",
  "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvQIBAD...\n-----END PRIVATE KEY-----\n",
  "client_email": "firebase-adminsdk-abc123@controle-de-acesso-tel-rfid2.iam.gserviceaccount.com",
  "client_id": "123456789012345678901",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://oauth2.googleapis.com/token",
  "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
  "client_x509_cert_url": "https://www.googleapis.com/certificates/..."
}
```

### Passo 3: Ir para Vercel Dashboard

1. Vá para https://vercel.com/dashboard
2. Clique no projeto **`firebase-auth`**
3. Clique em **"Settings"** (aba superior)
4. Procure por **"Environment Variables"** (no menu esquerdo)

### Passo 4: Adicionar Variáveis (Uma por Uma)

Para cada linha abaixo, clique em **"Add New"** e preencha:

| Variable Name | Value | Origem |
|---|---|---|
| `FIREBASE_TYPE` | `service_account` | Fixo |
| `FIREBASE_PROJECT_ID` | Copie de `project_id` | JSON |
| `FIREBASE_PRIVATE_KEY_ID` | Copie de `private_key_id` | JSON |
| `FIREBASE_PRIVATE_KEY` | **VEJA ABAIXO** | JSON |
| `FIREBASE_CLIENT_EMAIL` | Copie de `client_email` | JSON |
| `FIREBASE_CLIENT_ID` | Copie de `client_id` | JSON |
| `FIREBASE_AUTH_URI` | Copie de `auth_uri` | JSON |
| `FIREBASE_TOKEN_URI` | Copie de `token_uri` | JSON |
| `FIREBASE_AUTH_PROVIDER_CERT_URL` | Copie de `auth_provider_x509_cert_url` | JSON |
| `FIREBASE_CLIENT_CERT_URL` | Copie de `client_x509_cert_url` | JSON |
| `FIREBASE_DATABASE_URL` | `https://controle-de-acesso-tel-rfid2-default-rtdb.asia-southeast1.firebasedatabase.app` | Fixo |

### ⚠️ CUIDADO ESPECIAL: `FIREBASE_PRIVATE_KEY`

O campo `private_key` do JSON tem quebras de linha. **Copie exatamente como está:**

```
-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC...
...mais linhas aqui...
-----END PRIVATE KEY-----
```

Quando você colar no Vercel, ele ficará assim:
```
-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQC...\n...mais linhas...\n-----END PRIVATE KEY-----\n
```

Isso é **CORRETO**! Não remova os `\n`!

## ✅ Checklist de Verificação

Depois de adicionar todas as variáveis, verifique:

- [ ] 11 variáveis de ambiente foram adicionadas
- [ ] `FIREBASE_PROJECT_ID` está preenchido
- [ ] `FIREBASE_PRIVATE_KEY` começa com `-----BEGIN PRIVATE KEY-----\n`
- [ ] `FIREBASE_PRIVATE_KEY` termina com `\n-----END PRIVATE KEY-----\n`
- [ ] Nenhuma variável tem espaços extras no início/fim
- [ ] Todas as variáveis são visíveis na lista

## 🔄 Fazer Deploy

1. Após adicionar todas as variáveis, clique em **"Settings"** e desça
2. Ou clique em **"Deployments"** (aba)
3. Encontre o último deployment (com erro)
4. Clique em **"Redeploy"** (botão de menu ou pequena seta)
5. Clique **"Redeploy"** novamente para confirmar

Vercel vai fazer novo build com as variáveis!

## 📊 Como Saber se Funcionou

Depois que o deploy terminar:

### Opção 1: Ver no Dashboard
- Deployment deve ter **status verde** (sucesso)
- Não deve ter erros de Firebase

### Opção 2: Testar a URL
No navegador, vá para:
```
https://seu-projeto.vercel.app/health
```

Você deve ver:
```json
{
  "status": "OK",
  "timestamp": "2024-01-15T10:30:45.123Z",
  "message": "Servidor Node.js está rodando no Vercel"
}
```

### Opção 3: Testar POST
```bash
curl -X POST https://seu-projeto.vercel.app/api/esp32/dados \
  -H "Content-Type: application/json" \
  -d '{
    "temperatura": 25,
    "umidade": 60,
    "rssi": -30
  }'
```

Resposta esperada:
```json
{
  "success": true,
  "message": "Dados salvos com sucesso",
  "data": { ... }
}
```

## 🐛 Troubleshooting

### Erro: "Cannot find module './serviceAccountKey.json'"
- Significa que variáveis de ambiente NÃO foram configuradas
- Verifique se adicionou as 11 variáveis no Vercel
- Faça novo Redeploy

### Erro: "Invalid Private Key"
- A chave privada não foi copiada corretamente
- Certifique-se de copiar **exatamente como está** no JSON
- Inclua os `\n` (quebras de linha)

### Erro: "FIREBASE_PROJECT_ID não configurado"
- Você só adicionou algumas variáveis, não todas
- Revise o checklist acima e adicione as faltando

### Vercel mostra erro 500
1. Clique no deployment
2. Vá para **"Logs"**
3. Procure pela mensagem de erro específica
4. Adicione a variável faltando e faça Redeploy

## 📝 Variáveis de Ambiente - Cópia e Cola

Se tiver dificuldade, aqui estão os campos que você precisa extrair do JSON:

```
FIREBASE_TYPE = service_account

FIREBASE_PROJECT_ID = [projeto_id]

FIREBASE_PRIVATE_KEY_ID = [private_key_id]

FIREBASE_PRIVATE_KEY = [private_key com quebras de linha \n incluídas]

FIREBASE_CLIENT_EMAIL = [client_email]

FIREBASE_CLIENT_ID = [client_id]

FIREBASE_AUTH_URI = https://accounts.google.com/o/oauth2/auth

FIREBASE_TOKEN_URI = https://oauth2.googleapis.com/token

FIREBASE_AUTH_PROVIDER_CERT_URL = https://www.googleapis.com/oauth2/v1/certs

FIREBASE_CLIENT_CERT_URL = [client_x509_cert_url]

FIREBASE_DATABASE_URL = https://controle-de-acesso-tel-rfid2-default-rtdb.asia-southeast1.firebasedatabase.app
```

## 🎯 Próximas Etapas

Depois que o deploy funcionar:

1. ✅ Testar a URL do Vercel
2. ✅ Atualizar código do ESP32 com nova URL
3. ✅ Fazer upload no ESP32
4. ✅ Verificar dados no Firebase Console

Sucesso! 🚀
