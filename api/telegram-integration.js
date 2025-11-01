/**
 * Integração com Telegram Bot para confirmação de cadastro RFID
 *
 * Configuração necessária:
 * 1. Criar um bot no Telegram: @BotFather
 * 2. Obter o token do bot
 * 3. Obter seu Chat ID: enviar mensagem para o bot e chamar getUpdates
 * 4. Definir as variáveis de ambiente:
 *    - TELEGRAM_BOT_TOKEN
 *    - TELEGRAM_CHAT_ID
 *
 * IMPORTANTE: Este sistema trabalha com Firebase Realtime Database para
 * manter sincronização em ambiente serverless (Vercel). Cada requisição
 * é uma instância diferente, então não podemos guardar em memória!
 */

const axios = require('axios');

class TelegramBot {
  constructor(botToken, chatId, firebaseDb = null) {
    this.botToken = botToken;
    this.chatId = chatId;
    this.baseUrl = `https://api.telegram.org/bot${botToken}`;
    this.db = firebaseDb; // Firebase database instance
  }

  /**
   * Envia mensagem ao Telegram solicitando confirmação de cadastro
   * Guarda o timestamp para validar que a resposta veio DEPOIS do pedido
   */
  async askConfirmation(registrationId, cardUID) {
    try {
      // 1. Registrar o momento exato que estamos enviando a mensagem
      const messageTimestamp = Date.now();

      // 2. Enviar mensagem de confirmação
      const message = `
🔐 *Novo Cartão RFID Detectado*

*Card ID:* \`${cardUID}\`
*Timestamp:* ${new Date().toLocaleString('pt-BR')}

Deseja cadastrar este cartão?

Responda:
• *1* para confirmar e cadastrar
• *0* para cancelar
      `.trim();

      await axios.post(`${this.baseUrl}/sendMessage`, {
        chat_id: this.chatId,
        text: message,
        parse_mode: 'Markdown'
      });

      // 3. Armazenar no Firebase: timestamp quando a mensagem foi enviada
      if (this.db) {
        await this.db.ref(`registrations/${registrationId}`).update({
          messageTimestamp: messageTimestamp,
          messageStatus: 'awaiting_response'
        });
        console.log(`✓ Mensagem enviada às ${new Date(messageTimestamp).toLocaleString('pt-BR')} para ${cardUID}`);
      }

      console.log(`✓ Mensagem Telegram enviada para ${cardUID} (ID: ${registrationId})`);
      return registrationId;

    } catch (error) {
      console.error('✗ Erro ao enviar mensagem Telegram:', error.message);
      throw error;
    }
  }

  /**
   * Verifica a resposta do Telegram
   * Procura por respostas que chegaram DEPOIS da mensagem de pedido de confirmação
   */
  async checkResponse(registrationId) {
    try {
      // Se Firebase estiver disponível, usar como fonte de verdade
      if (this.db) {
        const snapshot = await this.db.ref(`registrations/${registrationId}`).once('value');
        const registration = snapshot.val();

        if (!registration) {
          return { status: 'not_found', message: 'Registro não encontrado' };
        }

        // Verificar timeout (5 minutos)
        const timeout = 5 * 60 * 1000;
        if (registration.timestamp && Date.now() - registration.timestamp > timeout) {
          return { status: 'timeout', message: 'Tempo de resposta expirado' };
        }

        // Se já foi respondido no Firebase, retornar
        if (registration.status === 'confirmed' && registration.response) {
          const confirmed = registration.response === '1';
          return {
            status: 'confirmed',
            confirmed: confirmed,
            cardUID: registration.cardUID,
            message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado'
          };
        }

        // Se ainda aguarda, procurar mensagens no Telegram
        const messageTimestamp = registration.messageTimestamp || Date.now();

        try {
          // Buscar TODAS as mensagens recentes
          const updates = await axios.get(`${this.baseUrl}/getUpdates`, {
            params: {
              limit: 100 // Pegar até 100 mensagens mais recentes
            }
          });

          const messages = updates.data.result || [];

          console.log(`🔍 Procurando resposta para ${registrationId}: ${messages.length} mensagens encontradas`);
          console.log(`📅 Validando mensagens depois de: ${new Date(messageTimestamp).toLocaleString('pt-BR')}`);

          // Procurar por respostas com "1" ou "0" nas mensagens
          // MAS SÓ as que chegaram DEPOIS de messageTimestamp
          for (const update of messages) {
            if (update.message && update.message.text && update.message.date) {
              // update.message.date vem em formato UNIX timestamp (segundos)
              const messageDate = update.message.date * 1000; // Converter para milissegundos
              const text = update.message.text.trim();

              // Só processar mensagens que chegaram DEPOIS da requisição
              if (messageDate > messageTimestamp) {
                if (text === '1' || text === '0') {
                  console.log(`✓ Resposta válida encontrada: "${text}" às ${new Date(messageDate).toLocaleString('pt-BR')}`);

                  const confirmed = text === '1';

                  return {
                    status: 'confirmed',
                    confirmed: confirmed,
                    cardUID: registration.cardUID,
                    message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado',
                    responseTime: messageDate
                  };
                }
              }
            }
          }

          return {
            status: 'waiting',
            message: 'Aguardando resposta',
            messagesChecked: messages.length,
            validAfter: new Date(messageTimestamp).toLocaleString('pt-BR')
          };

        } catch (telegramError) {
          console.error('⚠️ Erro ao consultar Telegram:', telegramError.message);
          return { status: 'waiting', message: 'Aguardando resposta (erro ao consultar Telegram)' };
        }
      }

      // Fallback sem Firebase: não implementar lógica ambígua
      return {
        status: 'waiting',
        message: 'Sistema de validação não disponível (Firebase não configurado)'
      };

    } catch (error) {
      console.error('✗ Erro ao verificar resposta:', error.message);
      throw error;
    }
  }

  /**
   * Envia notificação de resultado do cadastro
   */
  async notifyResult(cardUID, success, message = '') {
    try {
      const emoji = success ? '✅' : '❌';
      const text = `
${emoji} *Cadastro ${success ? 'Confirmado' : 'Cancelado'}*

*Card:* \`${cardUID}\`
${message ? `*Info:* ${message}` : ''}
      `.trim();

      await axios.post(`${this.baseUrl}/sendMessage`, {
        chat_id: this.chatId,
        text: text,
        parse_mode: 'Markdown'
      });

      console.log(`✓ Notificação enviada: ${cardUID} - ${success ? 'Confirmado' : 'Cancelado'}`);

    } catch (error) {
      console.error('✗ Erro ao enviar notificação:', error.message);
    }
  }
}

module.exports = TelegramBot;
