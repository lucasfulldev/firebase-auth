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
  constructor(botToken, chatId) {
    this.botToken = botToken;
    this.chatId = chatId;
    this.baseUrl = `https://api.telegram.org/bot${botToken}`;
    this.pendingRegistrations = new Map(); // Armazena cartões pendentes
  }

  /**
   * Envia mensagem ao Telegram solicitando confirmação de cadastro
   * Retorna um ID para rastrear a requisição
   */
  async askConfirmation(cardId, cardUID) {
    try {
      const registrationId = `${cardId}_${Date.now()}`;

      const message = `
🔐 *Novo Cartão RFID Detectado*

*Card ID:* \`${cardUID}\`
*Timestamp:* ${new Date().toLocaleString('pt-BR')}

Deseja cadastrar este cartão?

Responda:
• *1* para confirmar e cadastrar
• *0* para cancelar
      `.trim();

      const response = await axios.post(`${this.baseUrl}/sendMessage`, {
        chat_id: this.chatId,
        text: message,
        parse_mode: 'Markdown'
      });

      // Armazena a requisição pendente
      this.pendingRegistrations.set(registrationId, {
        cardId,
        cardUID,
        timestamp: Date.now(),
        messageId: response.data.result.message_id
      });

      console.log(`✓ Mensagem Telegram enviada para ${cardUID}`);
      return registrationId;

    } catch (error) {
      console.error('✗ Erro ao enviar mensagem Telegram:', error.message);
      throw error;
    }
  }

  /**
   * Verifica a resposta do Telegram
   */
  async checkResponse(registrationId) {
    try {
      const registration = this.pendingRegistrations.get(registrationId);

      if (!registration) {
        return { status: 'not_found', message: 'Registro não encontrado' };
      }

      // Verificar timeout (5 minutos)
      const timeout = 5 * 60 * 1000;
      if (Date.now() - registration.timestamp > timeout) {
        this.pendingRegistrations.delete(registrationId);
        return { status: 'timeout', message: 'Tempo de resposta expirado' };
      }

      // Buscar mensagens recentes
      const updates = await axios.get(`${this.baseUrl}/getUpdates`);
      const messages = updates.data.result;

      // Procurar por respostas com "1" ou "0"
      for (const update of messages) {
        if (update.message && update.message.text) {
          const text = update.message.text.trim();

          if (text === '1' || text === '0') {
            // Encontrou resposta
            const confirmed = text === '1';

            // Remover registro
            this.pendingRegistrations.delete(registrationId);

            return {
              status: 'confirmed',
              confirmed: confirmed,
              cardUID: registration.cardUID,
              message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado'
            };
          }
        }
      }

      console.log('updates', updates)

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

  /**
   * Limpar registros antigos (timeout)
   */
  cleanupOldRegistrations() {
    const timeout = 5 * 60 * 1000;
    const now = Date.now();

    for (const [key, value] of this.pendingRegistrations.entries()) {
      if (now - value.timestamp > timeout) {
        this.pendingRegistrations.delete(key);
        console.log(`🗑️ Limpeza: Registro ${key} removido por timeout`);
      }
    }
  }
}

module.exports = TelegramBot;
