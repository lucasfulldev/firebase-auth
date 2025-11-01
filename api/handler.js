const express = require('express');
const admin = require('firebase-admin');
const cors = require('cors');

// Verificar se Firebase já foi inicializado
if (!admin.apps.length) {
  // Validar variáveis de ambiente
  const requiredEnvVars = [
    'FIREBASE_PROJECT_ID',
    'FIREBASE_PRIVATE_KEY_ID',
    'FIREBASE_PRIVATE_KEY',
    'FIREBASE_CLIENT_EMAIL',
    'FIREBASE_CLIENT_ID',
    'FIREBASE_DATABASE_URL'
  ];

  const missingVars = requiredEnvVars.filter(v => !process.env[v]);

  if (missingVars.length > 0) {
    console.error('❌ Variáveis de ambiente faltando:', missingVars.join(', '));
    console.error('Configure as seguintes variáveis no Vercel:');
    console.error(requiredEnvVars.join('\n'));
    throw new Error('Variáveis de ambiente não configuradas');
  }

  // Usar variáveis de ambiente
  const serviceAccount = {
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

  console.log('📦 Inicializando Firebase com variáveis de ambiente...');
  console.log('Projeto:', serviceAccount.project_id);

  admin.initializeApp({
    credential: admin.credential.cert(serviceAccount),
    databaseURL: process.env.FIREBASE_DATABASE_URL
  });

  console.log('✓ Firebase inicializado com sucesso!');
}

const app = express();
const db = admin.database();

// Middlewares
app.use(cors());
app.use(express.json());

// ============================================================
// ROTAS PARA ESP32
// ============================================================

/**
 * POST /api/esp32/dados
 */
app.post('/api/esp32/dados', async (req, res) => {
  try {
    const { temperatura, umidade, rssi, mensagem } = req.body;

    if (temperatura === undefined || umidade === undefined || rssi === undefined) {
      return res.status(400).json({
        success: false,
        error: 'Faltam campos obrigatórios: temperatura, umidade, rssi'
      });
    }

    const dados = {
      timestamp: admin.database.ServerValue.TIMESTAMP,
      temperatura,
      umidade,
      rssi,
      mensagem: mensagem || "Dados do ESP32"
    };

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

/**
 * GET /health
 */
app.get('/health', (req, res) => {
  res.json({
    status: 'OK',
    timestamp: new Date().toISOString(),
    message: 'Servidor Node.js está rodando no Vercel'
  });
});

/**
 * GET /
 */
app.get('/', (req, res) => {
  res.json({
    name: 'Firebase Gateway - ESP32',
    version: '1.0.0',
    description: 'Servidor intermediário entre ESP32 e Firebase Realtime Database (Vercel)',
    status: 'online',
    timestamp: new Date().toISOString(),
    platform: 'Vercel Serverless',
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

module.exports = app;
