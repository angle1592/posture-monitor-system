# Posture Monitor FastAPI Backend

毕业设计后端服务：从 OneNET 同步坐姿数据到 MySQL，并给 App 提供查询接口。

## Local Server Setup

```bash
cd server
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
```

创建数据库和表：

```bash
mysql -u root -p < schema.sql
```

启动接口：

```bash
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

快速验证：

```bash
curl http://127.0.0.1:8000/health
curl -X POST http://127.0.0.1:8000/api/test/insert
curl http://127.0.0.1:8000/api/posture/latest
```

## Notes

- MySQL 建议只监听 `127.0.0.1`，App 只访问 FastAPI。
- `server/.env` 存放真实数据库密码和 OneNET token，不要提交。
- 当前为 M1/M2 最小骨架，OneNET 定时同步和统计接口在后续里程碑实现。
