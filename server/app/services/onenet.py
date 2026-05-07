"""
OneNET 云平台 HTTP API 客户端。

参考 App 端 oneNetApi.ts 的接口定义，复用相同端点和认证方式。
"""

import logging
from dataclasses import dataclass
from datetime import datetime
from typing import Any

import httpx

from ..config import settings

logger = logging.getLogger(__name__)

THINGMODEL_BASE = "https://iot-api.heclouds.com/thingmodel"
DEVICE_BASE = "https://iot-api.heclouds.com/device"


@dataclass
class PropertyItem:
    """OneNET 物模型属性条目。"""

    identifier: str
    value: Any
    time: datetime | None = None


def _headers() -> dict[str, str]:
    return {
        "authorization": settings.onenet_token,
        "Content-Type": "application/json",
    }


async def query_device_properties() -> list[PropertyItem]:
    """查询设备最新全部属性。

    对应 App 端 queryDeviceProperty()。
    返回 PropertyItem 列表；出错时返回空列表。
    """
    params = {
        "product_id": settings.onenet_product_id,
        "device_name": settings.onenet_device_name,
    }
    try:
        async with httpx.AsyncClient(timeout=10) as client:
            resp = await client.get(
                f"{THINGMODEL_BASE}/query-device-property",
                params=params,
                headers=_headers(),
            )
            resp.raise_for_status()
            body = resp.json()

        if body.get("code", 0) != 0:
            logger.warning("[OneNET] query-device-property code=%s: %s", body.get("code"), body.get("msg"))
            return []

        raw_items: list[dict] = body.get("data", [])
        items: list[PropertyItem] = []
        for item in raw_items:
            value = item.get("value")
            # 某些属性 value 可能是字符串 "true"/"false"，转为 bool
            if isinstance(value, str) and value.lower() in ("true", "false"):
                value = value.lower() == "true"
            ts = item.get("time")
            dt = None
            if ts is not None:
                try:
                    # OneNET 返回毫秒时间戳
                    dt = datetime.fromtimestamp(int(ts) / 1000)
                except (ValueError, TypeError, OSError):
                    pass
            items.append(PropertyItem(identifier=item.get("identifier", ""), value=value, time=dt))

        return items

    except httpx.HTTPError as exc:
        logger.error("[OneNET] HTTP error querying properties: %s", exc)
        return []
    except Exception as exc:
        logger.error("[OneNET] Unexpected error querying properties: %s", exc)
        return []


async def query_device_status() -> dict[str, Any]:
    """查询设备在线状态。

    对应 App 端 queryDeviceStatus()。
    返回 {"online": bool, "last_time": str}。
    """
    params = {
        "product_id": settings.onenet_product_id,
        "device_name": settings.onenet_device_name,
    }
    try:
        async with httpx.AsyncClient(timeout=10) as client:
            resp = await client.get(
                f"{DEVICE_BASE}/detail",
                params=params,
                headers=_headers(),
            )
            resp.raise_for_status()
            body = resp.json()

        if body.get("code", 0) != 0:
            logger.warning("[OneNET] device/detail code=%s: %s", body.get("code"), body.get("msg"))
            return {"online": False, "last_time": None}

        data = body.get("data", {})
        status = data.get("status", 0)
        last_time = data.get("last_time")
        return {"online": status == 1, "last_time": last_time}

    except httpx.HTTPError as exc:
        logger.error("[OneNET] HTTP error querying device status: %s", exc)
        return {"online": False, "last_time": None}
    except Exception as exc:
        logger.error("[OneNET] Unexpected error querying device status: %s", exc)
        return {"online": False, "last_time": None}
