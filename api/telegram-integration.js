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
   * Guarda o update_id (offset) para saber quais são mensagens NOVAS depois disto
   */
  async askConfirmation(registrationId, cardUID) {
    try {
      // 1. Pegar o último update_id antes de enviar a mensagem
      const initialUpdates = await axios.get(`${this.baseUrl}/getUpdates`);
      const messages = initialUpdates.data.result || [];
      let lastUpdateId = 0;

      if (messages.length > 0) {
        // Pegar o update_id da última mensagem
        lastUpdateId = messages[messages.length - 1].update_id;
      }

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

      // 3. Armazenar no Firebase: offset da próxima mensagem esperada
      if (this.db) {
        await this.db.ref(`registrations/${registrationId}`).update({
          telegramOffset: lastUpdateId,
          messageTimestamp: Date.now()
        });
        console.log(`✓ Offset armazenado: ${lastUpdateId} para ${cardUID}`);
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
   * Busca TODAS as mensagens desde a requisição e procura pela resposta do usuário
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

        // Se ainda aguarda, procurar mensagens no Telegram desde aquele offset
        const telegramOffset = registration.telegramOffset || 0;

        try {
          // Buscar mensagens a partir do offset armazenado
          // offset: N significa "retorna updates com ID >= N"
          // Depois procuramos pela resposta entre TODAS as mensagens
          const updates = await axios.get(`${this.baseUrl}/getUpdates`, {
            params: {
              offset: telegramOffset + 1, // Começa após a última mensagem armazenada
              limit: 100 // Pegar até 100 mensagens (padrão Telegram)
            }
          });

          const messages = updates.data.result || [];

          console.log(`🔍 Procurando resposta para ${registrationId}: ${messages.length} mensagens encontradas`);

          // Procurar por respostas com "1" ou "0" nas mensagens retornadas
          for (const update of messages) {
            if (update.message && update.message.text) {
              const text = update.message.text.trim();

              if (text === '1' || text === '0') {
                console.log(`✓ Resposta encontrada: ${text} para ${registrationId}`);

                const confirmed = text === '1';

                // IMPORTANTE: Fazer acknowledge da mensagem no Telegram
                // Isso marca como "lida" e impede que seja retornada novamente
                try {
                  await axios.get(`${this.baseUrl}/getUpdates`, {
                    params: {
                      offset: update.update_id + 1 // Marca todas as mensagens como processadas
                    }
                  });
                  console.log(`✓ Mensagem reconhecida no Telegram: ${update.update_id}`);
                } catch (err) {
                  console.warn('⚠️ Erro ao reconhecer mensagem no Telegram:', err.message);
                }

                return {
                  status: 'confirmed',
                  confirmed: confirmed,
                  cardUID: registration.cardUID,
                  message: confirmed ? 'Cadastro confirmado' : 'Cadastro cancelado'
                };
              }
            }
          }

          // Nenhuma resposta ainda - atualizar offset para não processar mensagens antigas novamente
          if (messages.length > 0) {
            const lastUpdateId = messages[messages.length - 1].update_id;

            // Atualizar offset no Firebase para próximas consultas
            await this.db.ref(`registrations/${registrationId}`).update({
              telegramOffset: lastUpdateId
            });

            console.log(`📍 Offset atualizado para ${registrationId}: ${lastUpdateId}`);
          }

          return {
            status: 'waiting',
            message: 'Aguardando resposta',
            messagesChecked: messages.length
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
