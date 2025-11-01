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
 */

const axios = require('axios');

class TelegramBot {
  constructor(botToken, chatId, firebaseDb = null) {
    this.botToken = botToken;
    this.chatId = chatId;
    this.baseUrl = `https://api.telegram.org/bot${botToken}`;
    this.db = firebaseDb; // Firebase database instance (opcional)
  }

  /**
   * Envia mensagem ao Telegram solicitando confirmação de cadastro
   * O registro é armazenado no Firebase para persistência entre requisições
   */
  async askConfirmation(registrationId, cardUID) {
    try {
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

      console.log(`✓ Mensagem Telegram enviada para ${cardUID} (ID: ${registrationId})`);
      return registrationId;

    } catch (error) {
      console.error('✗ Erro ao enviar mensagem Telegram:', error.message);
      throw error;
    }
  }

  /**
   * Verifica a resposta do Telegram
   * Consulta o Firebase para obter status da resposta
   */
  async checkResponse(registrationId) {
    try {
      // Se Firebase estiver disponível, consultar status lá
      if (this.db) {
        const snapshot = await this.db.ref(`registrations/${registrationId}`).once('value');
        const registration = snapshot.val();

        if (!registration) {
          return { status: 'not_found', message: 'Registro não encontrado' };
        }

        // Verificar timeout (5 minutos)
        const timeout = 5 * 60 * 1000;
        if (Date.now() - registration.timestamp > timeout) {
          return { status: 'timeout', message: 'Tempo de resposta expirado' };
        }

        // Se já foi respondido
        if (registration.status === 'confirmed' && registration.response) {
          const confirmed = registration.response === '1';
          return {
            status: 'confirmed',
            confirmed: confirmed,
            cardUID: registration.cardUID,
            message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado'
          };
        }

        // Ainda aguardando
        return { status: 'waiting', message: 'Aguardando resposta' };
      }

      // Fallback: buscar diretamente do Telegram (menos confiável em serverless)
      const updates = await axios.get(`${this.baseUrl}/getUpdates`);
      const messages = updates.data.result || [];

      // Procurar por respostas com "1" ou "0"
      for (const update of messages) {
        if (update.message && update.message.text) {
          const text = update.message.text.trim();

          if (text === '1' || text === '0') {
            const confirmed = text === '1';
            return {
              status: 'confirmed',
              confirmed: confirmed,
              message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado'
            };
          }
        }
      }

      return { status: 'waiting', message: 'Aguardando resposta' };

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
