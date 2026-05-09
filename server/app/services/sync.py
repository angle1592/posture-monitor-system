"""
后台定时同步任务：每 10 秒从 OneNET 拉取最新属性写入 MySQL。

使用 APScheduler 调度，FastAPI lifespan 中启停。
"""

import logging
from datetime import datetime

from apscheduler.schedulers.asyncio import AsyncIOScheduler
from sqlalchemy import desc
from sqlalchemy.orm import Session

from ..config import settings
from ..database import SessionLocal
from ..models import PostureRecord
from .onenet import query_device_properties

logger = logging.getLogger(__name__)

# OneNET 驼峰属性名 → 数据库字段名
PROPERTY_MAP = {
    "postureType": "posture_type",
    "personPresent": "person_present",
    "ambientLux": "ambient_lux",
    "fillLightOn": "fill_light_on",
}

# 姿态类型数字 → 字符串（对应 shared/protocol 常量）
POSTURE_TYPE_MAP = {
    0: "normal",
    1: "head_down",
    2: "hunchback",
}

# 默认值（当 OneNET 返回中缺少某属性时使用）
DEFAULTS = {
    "posture_type": "unknown",
    "person_present": False,
    "ambient_lux": None,
    "fill_light_on": False,
}

scheduler = AsyncIOScheduler()


async def sync_once() -> None:
    """执行一次同步：从 OneNET 拉取最新属性，写入 MySQL。"""
    items = await query_device_properties()
    if not items:
        return

    # 把 PropertyItem 列表转成 {identifier: value} 字典
    prop_dict = {item.identifier: item for item in items}

    # 提取 onenet_time（取所有属性中最新的时间戳）
    onenet_time: datetime | None = None
    for item in items:
        if item.time is not None:
            if onenet_time is None or item.time > onenet_time:
                onenet_time = item.time

    if onenet_time is None:
        onenet_time = datetime.now()

    # 构建记录字段
    record_kwargs: dict = {
        "device_id": settings.onenet_device_name or "main",
        "onenet_time": onenet_time,
    }
    for onenet_id, db_field in PROPERTY_MAP.items():
        item = prop_dict.get(onenet_id)
        if item is not None:
            value = item.value
            # postureType: OneNET 返回字符串数字，转为对应姿态名
            if db_field == "posture_type":
                try:
                    value = POSTURE_TYPE_MAP.get(int(value), "unknown")
                except (ValueError, TypeError):
                    pass
            record_kwargs[db_field] = value
        else:
            record_kwargs[db_field] = DEFAULTS[db_field]

    # 检查是否已存在相同 device_id + onenet_time 的记录（去重）
    db: Session = SessionLocal()
    try:
        existing = (
            db.query(PostureRecord)
            .filter(
                PostureRecord.device_id == record_kwargs["device_id"],
                PostureRecord.onenet_time == record_kwargs["onenet_time"],
            )
            .first()
        )
        if existing is not None:
            logger.debug("[Sync] Duplicate record at %s, skipping", record_kwargs["onenet_time"])
            return

        record = PostureRecord(**record_kwargs)
        db.add(record)
        db.commit()
        logger.info(
            "[Sync] Saved: posture=%s person=%s lux=%s light=%s time=%s",
            record_kwargs.get("posture_type"),
            record_kwargs.get("person_present"),
            record_kwargs.get("ambient_lux"),
            record_kwargs.get("fill_light_on"),
            onenet_time,
        )
    except Exception as exc:
        db.rollback()
        logger.error("[Sync] Failed to save record: %s", exc)
    finally:
        db.close()


def start_sync() -> None:
    """启动后台同步调度器（每 10 秒执行一次）。"""
    scheduler.add_job(sync_once, "interval", seconds=10, id="onenet_sync", replace_existing=True)
    scheduler.start()
    logger.info("[Sync] Background sync started (interval=10s)")


def stop_sync() -> None:
    """停止后台同步调度器。"""
    scheduler.shutdown(wait=False)
    logger.info("[Sync] Background sync stopped")
