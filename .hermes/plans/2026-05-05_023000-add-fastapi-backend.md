# 坐姿监测系统 - FastAPI + MySQL 后端接入计划

## 目标

在阿里云服务器上新增 FastAPI 后端服务，用 MySQL 存储历史坐姿数据，App 端数据源从 OneNET API 切换到新后端。ESP32 和 K230 代码不动。

## 当前架构

```
ESP32 -> MQTT -> OneNET -> App (直接调 OneNET HTTP API)
```

App 调用的 OneNET 接口（`app/src/utils/oneNetApi.ts`）：

- `GET /thingmodel/query-device-property` - 最新属性
- `GET /thingmodel/query-device-property-history` - 历史数据（最多 200 条，7 天）
- `GET /device/detail` - 设备在线状态
- `POST /thingmodel/set-device-property` - 下发控制指令

## 目标架构

```
ESP32 -> MQTT -> OneNET（保留设备通信）
                  ^
            FastAPI 定时拉取（每 10 秒）
                  |
              MySQL 存储
                  |
           App 查询 / 控制转发
```

---

## 里程碑计划

### M1: 数据库搭建 + 连接测试

**做什么：**

1. 阿里云服务器安装 MySQL（yum 或 Docker）
2. 创建数据库 `posture_monitor`
3. 建表 `posture_records`
4. 安全组开放 3306（仅开发阶段，生产用 SSH 隧道或 localhost）

**表结构：**

```sql
CREATE DATABASE posture_monitor CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE TABLE posture_records (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    device_id VARCHAR(64) NOT NULL DEFAULT 'default',
    posture_type VARCHAR(20) NOT NULL,       -- normal/head_down/hunchback/no_person/unknown
    person_present BOOLEAN DEFAULT FALSE,
    is_posture BOOLEAN DEFAULT TRUE,
    onenet_time DATETIME(3) NOT NULL,        -- OneNET 原始时间戳
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_device_time (device_id, onenet_time),
    INDEX idx_created (created_at)
);
```

**验证：**

- [ ] 服务器本地 `mysql -u root -p` 能登录
- [ ] 手动 INSERT 一条数据，SELECT 能查到
- [ ] （可选）远程 MySQL 客户端能连上

---

### M2: FastAPI 骨架 + 数据库连接

**做什么：**

1. 服务器安装 Python 依赖（`pip install fastapi uvicorn sqlalchemy pymysql`）
2. 创建 `server/` 目录结构
3. 实现 FastAPI 入口 + 数据库连接
4. 实现一个健康检查接口和一个手动插入测试数据的接口

**目录结构：**

```
server/
├── app/
│   ├── __init__.py
│   ├── main.py              # FastAPI 入口
│   ├── config.py            # 配置
│   ├── database.py          # SQLAlchemy 连接
│   ├── models.py            # ORM 模型
│   ├── schemas.py           # Pydantic 响应模型
│   └── routers/
│       ├── __init__.py
│       └── posture.py       # 坐姿接口
├── requirements.txt
└── .env.example
```

**验证：**

- [ ] `uvicorn app.main:app --host 0.0.0.0 --port 8000` 启动成功
- [ ] `curl localhost:8000/health` 返回 200
- [ ] `curl -X POST localhost:8000/api/test/insert` 插入一条测试数据
- [ ] `curl localhost:8000/api/posture/latest` 能查到刚插入的数据

---

### M3: OneNET 数据同步

**做什么：**

1. 实现 `services/onenet.py` - 封装 OneNET HTTP API 调用
2. 实现 `services/sync.py` - 后台定时任务，每 10 秒从 OneNET 拉取最新属性写入 MySQL
3. 配置 OneNET 凭据到 `.env`

**OneNET API 调用（复用现有逻辑）：**

```
GET https://iot-api.heclouds.com/thingmodel/query-device-property
    ?product_id=xxx&device_name=xxx
Header: authorization: xxx
```

**验证：**

- [ ] `curl localhost:8000/api/posture/latest` 返回 OneNET 的实时数据
- [ ] MySQL 里有新数据持续写入（`SELECT COUNT(*) FROM posture_records`）
- [ ] 等 1 分钟后查数据量在增长（约 6 条，10 秒/条）

---

### M4: 查询接口完善

**做什么：**

1. `GET /api/posture/history?date=2026-05-05` - 按天查历史
2. `GET /api/posture/stats/daily?date=2026-05-05` - 当天统计（良好时长/异常次数/健康评分）
3. `GET /api/posture/stats/weekly` - 7 天趋势
4. `GET /api/device/status` - 设备在线状态（转发 OneNET）
5. 加 CORS 中间件

**验证：**

- [ ] `curl localhost:8000/api/posture/stats/daily` 返回统计数据
- [ ] `curl localhost:8000/api/posture/history?date=2026-05-05` 返回当天记录列表
- [ ] 从本地浏览器/Postman 能访问（跨域已处理）

---

### M5: App 端切换数据源

**做什么：**

1. `.env` 加 `VITE_API_BASE_URL=http://服务器IP:8000`
2. 修改 `app/src/utils/oneNetApi.ts`：数据获取函数改调 FastAPI
3. 适配响应格式（OneNET `{ code, data }` -> FastAPI 直接 JSON）
4. `history/index.vue` 统计改用后端接口

**验证：**

- [ ] `npm run dev:h5` 启动 App
- [ ] 首页显示最新坐姿状态（来自 FastAPI -> MySQL -> OneNET）
- [ ] 历史页显示数据（来自 MySQL，不受 200 条/7 天限制）
- [ ] 控制指令（切换模式等）能通过 FastAPI 透传到 OneNET 生效

---

### M6: 部署与收尾

**做什么：**

1. FastAPI 配 systemd 服务（开机自启）
2. Nginx 反代 8000 端口（可选）
3. 关闭 MySQL 远程端口，改为 localhost 连接
4. 删除 M1-M2 的测试接口
5. 更新 README 文档

**验证：**

- [ ] 服务器重启后 FastAPI 自动运行
- [ ] App 通过 Nginx/公网 IP 正常访问
- [ ] 全链路跑通 10 分钟无报错

---

## 文件变更清单

| 文件 | 操作 | 里程碑 |
|------|------|--------|
| `server/**` | 新建 | M2-M4 |
| `app/src/utils/oneNetApi.ts` | 修改 | M5 |
| `app/.env.example` | 修改 | M5 |
| `app/src/pages/history/index.vue` | 修改 | M5 |
| `app/src/utils/store.ts` | 小改 | M5（如需要） |

## 依赖

```
# server/requirements.txt
fastapi>=0.115
uvicorn[standard]>=0.34
sqlalchemy>=2.0
pymysql>=1.1
httpx>=0.28
apscheduler>=3.10
python-dotenv>=1.0
pydantic>=2.0
```
