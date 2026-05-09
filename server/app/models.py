from datetime import datetime

from sqlalchemy import Boolean, DateTime, Float, String, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column

from .database import Base


class PostureRecord(Base):
    __tablename__ = "posture_records"
    __table_args__ = (
        UniqueConstraint("device_id", "onenet_time", name="uk_device_time"),
    )

    id: Mapped[int] = mapped_column(primary_key=True, autoincrement=True)
    device_id: Mapped[str] = mapped_column(String(64), nullable=False, default="main")
    posture_type: Mapped[str] = mapped_column(String(20), nullable=False)
    person_present: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    ambient_lux: Mapped[float | None] = mapped_column(Float, nullable=True)
    fill_light_on: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    onenet_time: Mapped[datetime] = mapped_column(DateTime, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime, nullable=False, default=datetime.now)
