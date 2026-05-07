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


class HistoryRecordOut(BaseModel):
    """历史记录（和 PostureRecordOut 相同结构，语义区分）。"""

    model_config = ConfigDict(from_attributes=True)

    id: int
    device_id: str
    posture_type: str
    person_present: bool
    ambient_lux: float | None
    fill_light_on: bool
    onenet_time: datetime
    created_at: datetime


class DailyStatsOut(BaseModel):
    """当天统计数据。"""

    good_posture_minutes: int
    abnormal_count: int
    health_score: int


class WeekTrendDayOut(BaseModel):
    """7 天趋势中某一天的数据。"""

    date: str
    score: int | None


class DeviceStatusOut(BaseModel):
    """设备在线状态。"""

    online: bool
    last_time: str | None = None
