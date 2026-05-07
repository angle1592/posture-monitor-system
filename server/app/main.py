from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy import text

from .config import settings
from .database import engine
from .routers import posture


app = FastAPI(title=settings.app_name)

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
