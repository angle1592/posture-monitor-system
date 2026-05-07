import os
from dataclasses import dataclass
from pathlib import Path

from dotenv import load_dotenv


BASE_DIR = Path(__file__).resolve().parent.parent
load_dotenv(BASE_DIR / ".env")


@dataclass(frozen=True)
class Settings:
    app_name: str = os.getenv("APP_NAME", "posture-monitor-api")
    app_env: str = os.getenv("APP_ENV", "development")
    api_host: str = os.getenv("API_HOST", "0.0.0.0")
    api_port: int = int(os.getenv("API_PORT", "8000"))
    database_url: str = os.getenv(
        "DATABASE_URL",
        "mysql+pymysql://posture_user:CHANGE_ME@127.0.0.1:3306/posture_monitor?charset=utf8mb4",
    )
    onenet_product_id: str = os.getenv("ONENET_PRODUCT_ID", "")
    onenet_device_name: str = os.getenv("ONENET_DEVICE_NAME", "")
    onenet_token: str = os.getenv("ONENET_TOKEN", "")


settings = Settings()
