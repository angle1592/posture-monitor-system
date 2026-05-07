from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy import text

from .config import settings
from .database import engine
from .routers import posture
from .services.sync import start_sync, stop_sync


@asynccontextmanager
async def lifespan(app: FastAPI):
    """FastAPI lifespan: 启动时开启后台同步，关闭时停止。"""
    start_sync()
    yield
    stop_sync()


app = FastAPI(title=settings.app_name, lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(posture.router)


@app.get("/health")
def health() -> dict[str, str]:
    database_status = "ok"
    try:
        with engine.connect() as conn:
            conn.execute(text("SELECT 1"))
    except Exception as exc:  # pragma: no cover - depends on deployment DB.
        database_status = f"error: {exc.__class__.__name__}"

    return {
        "status": "ok",
        "database": database_status,
        "env": settings.app_env,
    }
