# Posture Monitor FastAPI Backend

毕业设计后端服务：从 OneNET 同步坐姿数据到 MySQL，并给 App 提供查询接口。

## 架构

```
K230 → ESP32 → MQTT → OneNET (设备通信不动)
                          ↓
                    FastAPI 每10秒轮询 OneNET API → MySQL
                          ↓
                    App 调用 FastAPI 接口
```

## 接口一览

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/health` | 健康检查 |
| GET | `/api/posture/latest` | 最新坐姿状态 |
| GET | `/api/posture/history?date=2026-05-07&days=1` | 按日期查历史 |
| GET | `/api/posture/stats/daily?date=2026-05-07` | 当天统计 |
| GET | `/api/posture/stats/weekly` | 7天趋势 |
| GET | `/api/device/status` | 设备在线状态 |

## 本地开发

```bash
cd server
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
cp .env.example .env   # 填入真实数据库密码和 OneNET token
mysql -u root -p < schema.sql
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

验证：

```bash
curl http://127.0.0.1:8000/health
curl http://127.0.0.1:8000/api/posture/latest
curl http://127.0.0.1:8000/api/posture/stats/daily
```

## 部署（阿里云）

服务已配置 systemd 开机自启：

```bash
systemctl status posture-monitor   # 查看状态
systemctl restart posture-monitor  # 重启
journalctl -u posture-monitor -f   # 查看日志
```

Nginx 反代配置：`/etc/nginx/conf.d/posture-monitor.conf`（端口 8001 → 8000）

## 目录结构

```
server/
├── app/
│   ├── main.py              # FastAPI 入口 + lifespan
│   ├── config.py            # 配置（读 .env）
│   ├── database.py          # SQLAlchemy 连接
│   ├── models.py            # ORM 模型
│   ├── schemas.py           # Pydantic 响应模型
│   ├── routers/
│   │   └── posture.py       # 坐姿接口
│   └── services/
│       ├── onenet.py        # OneNET HTTP API 客户端
│       └── sync.py          # 后台定时同步
├── schema.sql               # 建库建表 SQL
├── requirements.txt         # Python 依赖
├── .env.example             # 环境变量模板
├── posture-monitor.service  # systemd 服务文件
└── nginx-posture.conf       # Nginx 配置
```

## 注意事项

- MySQL 只监听 `127.0.0.1`，不暴露远程端口
- `.env` 存真实凭据，不要提交到 Git
- OneNET token 有有效期，过期后需在 OneNET 控制台重新生成并更新 `.env`
- 同步间隔 10 秒，去重基于 `(device_id, onenet_time)` 唯一约束
