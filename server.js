const express = require('express');
const admin = require('firebase-admin');
const cors = require('cors');
const dotenv = require('dotenv');

// Carregar variáveis de ambiente
dotenv.config();

const app = express();
const PORT = process.env.PORT || 3000;

// Middlewares
app.use(cors());
app.use(express.json());

// Inicializar Firebase Admin SDK
let serviceAccount;

// Detectar ambiente
const isProduction = process.env.VERCEL || process.env.NODE_ENV === 'production';

if (isProduction || process.env.FIREBASE_PROJECT_ID) {
  // Modo produção (Vercel/variáveis de ambiente)
  console.log('📦 Usando variáveis de ambiente para Firebase');

  if (!process.env.FIREBASE_PROJECT_ID) {
    console.error('❌ ERRO: FIREBASE_PROJECT_ID não configurado!');
    console.error('Configure as variáveis de ambiente no Vercel');
    process.exit(1);
  }

  serviceAccount = {
    type: 'service_account',
    project_id: process.env.FIREBASE_PROJECT_ID,
    private_key_id: process.env.FIREBASE_PRIVATE_KEY_ID,
    private_key: (process.env.FIREBASE_PRIVATE_KEY || '').replace(/\\n/g, '\n'),
    client_email: process.env.FIREBASE_CLIENT_EMAIL,
    client_id: process.env.FIREBASE_CLIENT_ID,
    auth_uri: process.env.FIREBASE_AUTH_URI || 'https://accounts.google.com/o/oauth2/auth',
    token_uri: process.env.FIREBASE_TOKEN_URI || 'https://oauth2.googleapis.com/token',
    auth_provider_x509_cert_url: process.env.FIREBASE_AUTH_PROVIDER_CERT_URL || 'https://www.googleapis.com/oauth2/v1/certs',
    client_x509_cert_url: process.env.FIREBASE_CLIENT_CERT_URL
  };
} else {
  // Modo desenvolvimento (arquivo local)
  console.log('📁 Usando serviceAccountKey.json local');

  try {
    serviceAccount = require('./serviceAccountKey.json');
  } catch (error) {
    console.error('❌ Erro: serviceAccountKey.json não encontrado!');
    process.exit(1);
  }
}

try {
  admin.initializeApp({
    credential: admin.credential.cert(serviceAccount),
    databaseURL: process.env.FIREBASE_DATABASE_URL || "https://controle-de-acesso-tel-rfid2-default-rtdb.asia-southeast1.firebasedatabase.app"
  });
  console.log('✓ Firebase inicializado com sucesso!');
} catch (error) {
  console.error('❌ Erro ao inicializar Firebase:', error.message);
  process.exit(1);
}

const db = admin.database();

// ============================================================
// ROTAS PARA ESP32
// ============================================================

/**
 * POST /api/esp32/dados
 * Recebe dados do ESP32 e envia para Firebase
 *
 * Body esperado:
 * {
 *   "timestamp": 12345,
 *   "temperatura": 28,
 *   "umidade": 65,
 *   "rssi": -45,
 *   "mensagem": "Dados do ESP32"
 * }
 */
app.post('/api/esp32/dados', async (req, res) => {
  try {
    const { temperatura, umidade, rssi, mensagem } = req.body;

    // Validar dados
    if (temperatura === undefined || umidade === undefined || rssi === undefined) {
      return res.status(400).json({
        success: false,
        error: 'Faltam campos obrigatórios: temperatura, umidade, rssi'
      });
    }

    // Adicionar timestamp do servidor
    const dados = {
      timestamp: admin.database.ServerValue.TIMESTAMP,
      temperatura,
      umidade,
      rssi,
      mensagem: mensagem || "Dados do ESP32"
    };

    // Salvar no Firebase
    await db.ref('devices/esp32-test').set(dados);

    console.log('✓ Dados do ESP32 salvos:', dados);

    res.json({
      success: true,
      message: 'Dados salvos com sucesso',
      data: dados
    });

  } catch (error) {
    console.error('✗ Erro ao processar dados:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

/**
 * POST /api/esp32/acessos
 * Log de acessos (para RFID)
 *
 * Body esperado:
 * {
 *   "cardId": "ABC123",
 *   "acao": "acesso_concedido",
 *   "usuario": "João"
 * }
 */
app.post('/api/esp32/acessos', async (req, res) => {
  try {
    const { cardId, acao, usuario } = req.body;

    if (!cardId || !acao) {
      return res.status(400).json({
        success: false,
        error: 'Faltam campos: cardId, acao'
      });
    }

    const acesso = {
      cardId,
      acao,
      usuario: usuario || 'desconhecido',
      timestamp: admin.database.ServerValue.TIMESTAMP
    };

    // Usar push() para criar ID único automaticamente
    const ref = await db.ref('acessos').push(acesso);

    console.log('✓ Acesso registrado:', ref.key);

    res.json({
      success: true,
      message: 'Acesso registrado',
      id: ref.key
    });

  } catch (error) {
    console.error('✗ Erro ao registrar acesso:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

/**
 * GET /api/esp32/dados
 * Retorna os últimos dados do ESP32
 */
app.get('/api/esp32/dados', async (req, res) => {
  try {
    const snapshot = await db.ref('devices/esp32-test').once('value');
    const dados = snapshot.val();

    if (!dados) {
      return res.json({
        success: true,
        data: null,
        message: 'Nenhum dado ainda'
      });
    }

    res.json({
      success: true,
      data: dados
    });

  } catch (error) {
    console.error('✗ Erro ao ler dados:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

/**
 * GET /api/esp32/acessos
 * Retorna todos os acessos registrados
 */
app.get('/api/esp32/acessos', async (req, res) => {
  try {
    const snapshot = await db.ref('acessos').limitToLast(10).once('value');
    const acessos = snapshot.val();

    if (!acessos) {
      return res.json({
        success: true,
        data: [],
        message: 'Nenhum acesso registrado'
      });
    }

    res.json({
      success: true,
      data: acessos
    });

  } catch (error) {
    console.error('✗ Erro ao ler acessos:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ============================================================
// ROTAS DE SAÚDE E INFO
// ============================================================

/**
 * GET /health
 * Verifica se o servidor está rodando
 */
app.get('/health', (req, res) => {
  res.json({
    status: 'OK',
    timestamp: new Date().toISOString(),
    message: 'Servidor Node.js está rodando'
  });
});

/**
 * GET /
 * Info da API
 */
app.get('/', (req, res) => {
  res.json({
    name: 'Firebase Gateway - ESP32',
    version: '1.0.0',
    description: 'Servidor intermediário entre ESP32 e Firebase Realtime Database',
    status: 'online',
    timestamp: new Date().toISOString(),
    environment: process.env.NODE_ENV || 'development',
    endpoints: {
      'POST /api/esp32/dados': 'Enviar dados do ESP32',
      'GET /api/esp32/dados': 'Ler últimos dados',
      'POST /api/esp32/acessos': 'Registrar acesso (RFID)',
      'GET /api/esp32/acessos': 'Ler últimos 10 acessos',
      'GET /health': 'Status do servidor',
      'GET /': 'Informações da API'
    },
    documentation: 'https://github.com/SEU_USUARIO/firebase-auth#readme',
    firebase: {
      project: process.env.FIREBASE_PROJECT_ID || 'não configurado',
      database: process.env.FIREBASE_DATABASE_URL ? '✓ conectado' : '✗ não configurado'
    }
  });
});

// ============================================================
// ROTAS PARA TELEGRAM - CONFIRMAÇÃO DE CADASTRO
// ============================================================

/**
 * POST /api/esp32/telegram/ask
 * Recebe requisição do ESP32 para pedir confirmação no Telegram
 */
app.post('/api/esp32/telegram/ask', async (req, res) => {
  try {
    const { cardUID, action } = req.body;

    if (!cardUID) {
      return res.status(400).json({
        success: false,
        error: 'Faltam campos: cardUID'
      });
    }

    // Gerar ID único para esta requisição
    const registrationId = `${cardUID}_${Date.now()}`;

    // Armazenar no Firebase para rastreamento
    await db.ref(`registrations/${registrationId}`).set({
      cardUID: cardUID,
      status: 'pending',
      timestamp: admin.database.ServerValue.TIMESTAMP,
      expiresAt: admin.database.ServerValue.TIMESTAMP
    });

    console.log(`✓ Requisição de cadastro criada: ${registrationId}`);

    res.json({
      success: true,
      registrationId: registrationId,
      message: 'Confirmação enviada para Telegram'
    });

  } catch (error) {
    console.error('✗ Erro ao criar requisição:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

/**
 * POST /api/esp32/telegram/check
 * Verifica se o usuário confirmou ou cancelou no Telegram
 */
app.post('/api/esp32/telegram/check', async (req, res) => {
  try {
    const { registrationId } = req.body;

    if (!registrationId) {
      return res.status(400).json({
        success: false,
        error: 'Faltam campos: registrationId'
      });
    }

    // Buscar status da requisição
    const snapshot = await db.ref(`registrations/${registrationId}`).once('value');
    const registration = snapshot.val();

    if (!registration) {
      return res.json({
        status: 'not_found',
        message: 'Requisição não encontrada'
      });
    }

    // Verificar se já foi respondida
    if (registration.status === 'confirmed') {
      const confirmed = registration.response === '1';

      res.json({
        status: 'confirmed',
        confirmed: confirmed,
        message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado'
      });

      // Limpar registro após resposta
      setTimeout(() => {
        db.ref(`registrations/${registrationId}`).remove();
      }, 5000);
    } else {
      res.json({
        status: 'waiting',
        message: 'Aguardando resposta do Telegram'
      });
    }

  } catch (error) {
    console.error('✗ Erro ao verificar requisição:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

/**
 * POST /api/esp32/telegram/respond
 * Endpoint para o bot Telegram responder (1 para confirmar, 0 para cancelar)
 * Pode ser chamado via webhook do Telegram
 */
app.post('/api/esp32/telegram/respond', async (req, res) => {
  try {
    const { registrationId, response } = req.body;

    if (!registrationId || !response) {
      return res.status(400).json({
        success: false,
        error: 'Faltam campos: registrationId, response'
      });
    }

    // Validar resposta
    if (response !== '1' && response !== '0') {
      return res.status(400).json({
        success: false,
        error: 'Resposta deve ser "1" ou "0"'
      });
    }

    // Atualizar status da requisição
    await db.ref(`registrations/${registrationId}`).update({
      status: 'confirmed',
      response: response,
      respondedAt: admin.database.ServerValue.TIMESTAMP
    });

    console.log(`✓ Resposta Telegram recebida: ${registrationId} = ${response}`);

    res.json({
      success: true,
      message: 'Resposta registrada com sucesso'
    });

  } catch (error) {
    console.error('✗ Erro ao registrar resposta:', error);
    res.status(500).json({
      success: false,
      error: error.message
    });
  }
});

// ============================================================
// TRATAMENTO DE ERROS
// ============================================================

app.use((req, res) => {
  res.status(404).json({
    success: false,
    error: 'Rota não encontrada',
    path: req.path
  });
});

// ============================================================
// INICIAR SERVIDOR
// ============================================================

app.listen(PORT, () => {
  console.log(`
╔════════════════════════════════════════════╗
║   Firebase Gateway - ESP32                 ║
║   Servidor rodando em: http://localhost:${PORT}   ║
║   Pressione Ctrl+C para parar              ║
╚════════════════════════════════════════════╝
  `);
  console.log('Endpoints disponíveis:');
  console.log('  POST /api/esp32/dados');
  console.log('  GET /api/esp32/dados');
  console.log('  POST /api/esp32/acessos');
  console.log('  GET /api/esp32/acessos');
  console.log('  GET /health');
  console.log('');
});

module.exports = app;
