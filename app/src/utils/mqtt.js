/*
 * legacy: 新主流程已迁移到 `src/utils/realtime/mqtt-client.ts`。
 * 该文件暂保留作参考，不再新增业务引用。
 *
 * 模块职责：MQTT 客户端单例封装，为 uni-app 提供发布/订阅能力。
 *
 * 数据流：
 * - 上行：页面或业务模块调用 publish() 向指定 topic 发送消息。
 * - 下行：底层 client 收到 message 后，经 handleMessage() 按 topic 分发到回调队列。
 *
 * 平台差异：
 * - APP/H5：依赖 mqtt.js（WebSocket 协议）建立真实连接。
 * - MP-WEIXIN：当前实现明确报错或仅日志占位，避免误判“已连接可用”。
 */

class MQTTClient {
  constructor() {
    this.client = null;
    this.isConnected = false;
    this.config = null;
    this.messageCallbacks = new Map();
    this.connectionCallback = null;
    this.errorCallback = null;
  }

  /**
   * 初始化MQTT配置
   */
  init(config) {
    this.config = {
      port: 1883,
      useSSL: false,
      ...config
    };
    console.log('[MQTT] 配置已初始化', this.config);
  }

  /**
   * 连接到MQTT服务器
   */
  connect() {
    return new Promise((resolve, reject) => {
      if (!this.config) {
        reject(new Error('MQTT未初始化，请先调用init()'));
        return;
      }

      if (this.isConnected) {
        console.log('[MQTT] 已经连接');
        resolve();
        return;
      }

      // APP/H5 分支：要求全局已注入 mqtt 对象。
      // #ifdef APP-PLUS || H5
      if (typeof mqtt !== 'undefined') {
        this.connectWithMQTTjs(resolve, reject);
      } else {
        reject(new Error('MQTT.js未加载，请先在HTML中引入'));
      }
      // #endif

      // 小程序分支：当前不提供可用 MQTT 通道，直接失败提示调用方兜底。
      // #ifdef MP-WEIXIN
      // 小程序端这里显式失败，避免“看似连接成功但实际不可用”的假状态。
      reject(new Error('微信小程序需要使用原生Socket实现MQTT'));
      // #endif
    });
  }

  /**
   * 使用MQTT.js连接
   */
  connectWithMQTTjs(resolve, reject) {
    try {
      // 按 useSSL 选择 ws/wss，统一追加 /mqtt 路径。
      const protocol = this.config.useSSL ? 'wss' : 'ws';
      const url = `${protocol}://${this.config.host}:${this.config.port}/mqtt`;
      
      this.client = mqtt.connect(url, {
        clientId: this.config.clientId,
        username: this.config.username,
        password: this.config.password,
        reconnectPeriod: 5000,
        connectTimeout: 30000
      });

      this.client.on('connect', () => {
        console.log('[MQTT] 连接成功');
        this.isConnected = true;
        if (this.connectionCallback) {
          this.connectionCallback(true);
        }
        resolve();
      });

      this.client.on('message', (topic, message) => {
        // 所有下行消息统一进入分发入口，便于后续扩展通配符匹配。
        this.handleMessage(topic, message);
      });

      this.client.on('error', (error) => {
        console.error('[MQTT] 错误:', error);
        this.isConnected = false;
        if (this.errorCallback) {
          this.errorCallback(error);
        }
        reject(error);
      });

      this.client.on('close', () => {
        console.log('[MQTT] 连接关闭');
        this.isConnected = false;
        if (this.connectionCallback) {
          this.connectionCallback(false);
        }
      });

    } catch (error) {
      reject(error);
    }
  }

  /**
   * 处理接收到的消息
   */
  handleMessage(topic, message) {
    const messageStr = message.toString();
    console.log(`[MQTT] 收到消息 [${topic}]:`, messageStr);

    const callbacks = this.messageCallbacks.get(topic);
    // 当前实现按“精确 topic”派发；若后续要支持通配符，需要改这里。
    if (callbacks) {
      callbacks.forEach(callback => {
        try {
          callback(topic, messageStr);
        } catch (error) {
          console.error('[MQTT] 回调函数执行错误:', error);
        }
      });
    }
  }

  /**
   * 订阅主题
   */
  subscribe(topic, callback, options = {}) {
    return new Promise((resolve, reject) => {
      if (!this.isConnected) {
        reject(new Error('MQTT未连接'));
        return;
      }

      // 先记录本地回调，再向 broker 发送订阅请求。
      if (!this.messageCallbacks.has(topic)) {
        this.messageCallbacks.set(topic, []);
      }
      this.messageCallbacks.get(topic).push(callback);

      // #ifdef APP-PLUS || H5
      if (this.client) {
        this.client.subscribe(topic, options, (error) => {
          if (error) {
            console.error('[MQTT] 订阅失败:', error);
            reject(error);
          } else {
            console.log(`[MQTT] 订阅成功: ${topic}`);
            resolve();
          }
        });
      }
      // #endif

      // #ifdef MP-WEIXIN
      console.log(`[MQTT] 订阅主题: ${topic}`);
      resolve();
      // #endif
    });
  }

  /**
   * 取消订阅
   */
  unsubscribe(topic) {
    return new Promise((resolve, reject) => {
      if (!this.isConnected) {
        reject(new Error('MQTT未连接'));
        return;
      }

      this.messageCallbacks.delete(topic);

      // #ifdef APP-PLUS || H5
      if (this.client) {
        this.client.unsubscribe(topic, (error) => {
          if (error) {
            console.error('[MQTT] 取消订阅失败:', error);
            reject(error);
          } else {
            console.log(`[MQTT] 取消订阅成功: ${topic}`);
            resolve();
          }
        });
      }
      // #endif

      // #ifdef MP-WEIXIN
      console.log(`[MQTT] 取消订阅主题: ${topic}`);
      resolve();
      // #endif
    });
  }

  /**
   * 发布消息
   */
  publish(topic, message, options = {}) {
    return new Promise((resolve, reject) => {
      if (!this.isConnected) {
        reject(new Error('MQTT未连接'));
        return;
      }

      // 统一将对象消息序列化，保证传输层 payload 为字符串。
      let messageStr = message;
      if (typeof message === 'object') {
        messageStr = JSON.stringify(message);
      }

      // #ifdef APP-PLUS || H5
      if (this.client) {
        this.client.publish(topic, messageStr, options, (error) => {
          if (error) {
            console.error('[MQTT] 发布失败:', error);
            reject(error);
          } else {
            console.log(`[MQTT] 发布成功 [${topic}]:`, messageStr);
            resolve();
          }
        });
      }
      // #endif

      // #ifdef MP-WEIXIN
      console.log(`[MQTT] 发布消息 [${topic}]:`, messageStr);
      resolve();
      // #endif
    });
  }

  /**
   * 断开连接
   */
  disconnect() {
    return new Promise((resolve) => {
      if (!this.isConnected) {
        resolve();
        return;
      }

      // #ifdef APP-PLUS || H5
      if (this.client) {
        this.client.end(false, () => {
          console.log('[MQTT] 连接已断开');
          this.isConnected = false;
          this.client = null;
          resolve();
        });
      }
      // #endif

      // #ifdef MP-WEIXIN
      this.isConnected = false;
      console.log('[MQTT] 连接已断开');
      resolve();
      // #endif
    });
  }

  /**
   * 设置连接状态回调
   */
  onConnectionChange(callback) {
    this.connectionCallback = callback;
  }

  /**
   * 设置错误回调
   */
  onError(callback) {
    this.errorCallback = callback;
  }

  /**
   * 获取连接状态
   */
  getConnectionStatus() {
    return this.isConnected;
  }
}

// 导出单例实例
const mqttClient = new MQTTClient();

export default mqttClient;
