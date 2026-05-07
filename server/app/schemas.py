from datetime import datetime

from pydantic import BaseModel, ConfigDict


class PostureRecordOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    posture_type: str
    person_present: bool
    ambient_lux: float | None
    fill_light_on: bool
    onenet_time: datetime
    created_at: datetime


class TestInsertResponse(BaseModel):
    id: int
    message: str
