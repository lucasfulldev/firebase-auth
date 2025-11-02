const express = require('express');
const admin = require('firebase-admin');
const cors = require('cors');
const axios = require('axios');
const TelegramBot = require('./telegram-integration');

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

// Inicializar Telegram Bot se credenciais estiverem configuradas
let telegramBot = null;
if (process.env.TELEGRAM_BOT_TOKEN && process.env.TELEGRAM_CHAT_ID) {
  telegramBot = new TelegramBot(process.env.TELEGRAM_BOT_TOKEN, process.env.TELEGRAM_CHAT_ID, db);
  console.log('✓ Telegram Bot inicializado com Firebase');
} else {
  console.warn('⚠️ Telegram Bot não configurado (faltam credenciais)');
}

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
 * POST /api/esp32/check-authorization
 * Valida se um cartão está autorizado (busca em registrations confirmados)
 */
app.post('/api/esp32/check-authorization', async (req, res) => {
  try {
    const { cardId } = req.body;

    if (!cardId) {
      return res.status(400).json({
        success: false,
        authorized: false,
        error: 'Faltam campos: cardId'
      });
    }

    // Buscar todas as registrations no Firebase
    const snapshot = await db.ref('registrations').once('value');
    const registrations = snapshot.val();

    if (!registrations) {
      console.log(`✗ Cartão ${cardId} - nenhuma registration no banco`);
      return res.json({
        success: true,
        authorized: false,
        message: 'Cartão não autorizado'
      });
    }

    // Procurar cartão que foi confirmado (status: 'confirmed' e response: '1')
    const isAuthorized = Object.values(registrations).some(registration =>
      registration.cardUID === cardId &&
      registration.status === 'confirmed' &&
      registration.response === '1'
    );

    if (isAuthorized) {
      console.log(`✓ Cartão ${cardId} - AUTORIZADO (confirmado via Telegram)`);
      res.json({
        success: true,
        authorized: true,
        message: 'Cartão autorizado'
      });
    } else {
      console.log(`✗ Cartão ${cardId} - NÃO AUTORIZADO`);
      res.json({
        success: true,
        authorized: false,
        message: 'Cartão não autorizado'
      });
    }

  } catch (error) {
    console.error('✗ Erro ao verificar autorização:', error);
    res.status(500).json({
      success: false,
      authorized: false,
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
      'POST /api/esp32/check-authorization': 'Validar se cartão está autorizado',
      'POST /api/esp32/telegram/ask': 'Pedir confirmação Telegram',
      'POST /api/esp32/telegram/check': 'Verificar resposta Telegram',
      'POST /api/esp32/telegram/respond': 'Registrar resposta Telegram',
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

    // Verificar se Telegram Bot está configurado
    if (!telegramBot) {
      console.error('❌ Telegram não configurado');
      return res.status(500).json({
        success: false,
        error: 'Telegram não configurado no servidor'
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

    // Usar classe TelegramBot para pedir confirmação
    try {
      await telegramBot.askConfirmation(registrationId, cardUID);

      console.log(`✓ Requisição de cadastro criada: ${registrationId}`);

      res.json({
        success: true,
        registrationId: registrationId,
        message: 'Confirmação enviada para Telegram'
      });

    } catch (telegramError) {
      console.error('❌ Erro ao enviar mensagem Telegram:', telegramError.message);

      // Mesmo com erro, retorna sucesso para ESP32 continuar aguardando
      res.json({
        success: true,
        registrationId: registrationId,
        message: 'Confirmação registrada (erro ao enviar Telegram)',
        telegramError: telegramError.message
      });
    }

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
 * Usa a classe TelegramBot para verificar respostas
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

    // Verificar se Telegram Bot está configurado
    if (!telegramBot) {
      // Fallback: tentar carregar do Firebase
      const snapshot = await db.ref(`registrations/${registrationId}`).once('value');
      const registration = snapshot.val();

      if (!registration) {
        return res.json({
          status: 'not_found',
          message: 'Requisição não encontrada'
        });
      }

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
        return;
      }

      return res.json({
        status: 'waiting',
        message: 'Aguardando resposta do Telegram'
      });
    }

    // Usar classe TelegramBot para verificar resposta
    try {
      const result = await telegramBot.checkResponse(registrationId);

      if (result.status === 'confirmed') {
        // Armazenar resposta no Firebase também
        await db.ref(`registrations/${registrationId}`).update({
          status: 'confirmed',
          response: result.confirmed ? '1' : '0',
          respondedAt: admin.database.ServerValue.TIMESTAMP
        });

        res.json({
          status: 'confirmed',
          confirmed: result.confirmed,
          message: result.message
        });
      } else if (result.status === 'timeout') {
        // Armazenar timeout no Firebase
        await db.ref(`registrations/${registrationId}`).update({
          status: 'timeout',
          respondedAt: admin.database.ServerValue.TIMESTAMP
        });

        res.json({
          status: 'timeout',
          message: 'Tempo de resposta expirado'
        });
      } else {
        // status: 'waiting' ou 'not_found'
        res.json(result);
      }

    } catch (telegramError) {
      console.error('⚠️ Erro ao consultar Telegram:', telegramError.message);
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
 * Armazena a resposta no Firebase para que o TelegramBot possa recuperá-la
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

    // Armazenar resposta no Firebase
    await db.ref(`registrations/${registrationId}`).update({
      status: 'confirmed',
      response: response,
      respondedAt: admin.database.ServerValue.TIMESTAMP
    });

    console.log(`✓ Resposta Telegram recebida: ${registrationId} = ${response}`);

    // Se TelegramBot estiver configurado, atualizar o registro pendente também
    if (telegramBot) {
      // A classe TelegramBot guarda registros na memória (pendingRegistrations)
      // Aqui apenas alertamos que a resposta foi recebida
      console.log(`✓ Resposta armazenada em Firebase para TelegramBot processar`);
    }

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

module.exports = app;
