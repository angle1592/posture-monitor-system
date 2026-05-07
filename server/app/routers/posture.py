from datetime import date, datetime, timedelta
from zoneinfo import ZoneInfo

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy import func
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import PostureRecord
from ..schemas import (
    DailyStatsOut,
    DeviceStatusOut,
    HistoryRecordOut,
    PostureRecordOut,
    TestInsertResponse,
    WeekTrendDayOut,
)
from ..services.onenet import query_device_status

router = APIRouter(prefix="/api", tags=["posture"])

CN = ZoneInfo("Asia/Shanghai")

# 姿态分类
ABNORMAL_TYPES = {"head_down", "hunchback"}
HEALTHY_TYPES = {"normal"}
# 无人、未知不计入评分


def china_now() -> datetime:
    return datetime.now(CN).replace(tzinfo=None)


@router.post("/test/insert", response_model=TestInsertResponse)
def insert_test_record(db: Session = Depends(get_db)) -> TestInsertResponse:
    record = PostureRecord(
        device_id="main",
        posture_type="normal",
        person_present=True,
        ambient_lux=120.0,
        fill_light_on=False,
        onenet_time=china_now(),
    )
    db.add(record)
    try:
        db.commit()
    except Exception:
        db.rollback()
        raise HTTPException(status_code=409, detail="duplicate test record")
    db.refresh(record)
    return TestInsertResponse(id=record.id, message="inserted")


@router.get("/posture/latest", response_model=PostureRecordOut)
def latest_posture(db: Session = Depends(get_db)) -> PostureRecord:
    record = (
        db.query(PostureRecord)
        .order_by(PostureRecord.onenet_time.desc(), PostureRecord.id.desc())
        .first()
    )
    if record is None:
        raise HTTPException(status_code=404, detail="no posture records")
    return record


@router.get("/posture/history", response_model=list[HistoryRecordOut])
def posture_history(
    date: str = Query(default=None, description="日期 YYYY-MM-DD，默认今天"),
    days: int = Query(default=1, ge=1, le=31, description="查询天数"),
    db: Session = Depends(get_db),
) -> list[PostureRecord]:
    """按日期查历史记录。

    返回指定日期（或最近 N 天）的全部记录，按时间升序。
    """
    if date:
        try:
            target = datetime.strptime(date, "%Y-%m-%d").date()
        except ValueError:
            raise HTTPException(400, "date 格式应为 YYYY-MM-DD")
    else:
        target = datetime.now(CN).date()

    start = datetime.combine(target, datetime.min.time())
    end = start + timedelta(days=days)

    records = (
        db.query(PostureRecord)
        .filter(PostureRecord.onenet_time >= start, PostureRecord.onenet_time < end)
        .order_by(PostureRecord.onenet_time.asc())
        .all()
    )
    return records


@router.get("/posture/stats/daily", response_model=DailyStatsOut)
def daily_stats(
    date: str = Query(default=None, description="日期 YYYY-MM-DD，默认今天"),
    db: Session = Depends(get_db),
) -> dict:
    """当天统计：良好时长、异常次数、健康评分。

    基于每条记录间隔估算时长（约 10 秒/条）。
    """
    if date:
        try:
            target = datetime.strptime(date, "%Y-%m-%d").date()
        except ValueError:
            raise HTTPException(400, "date 格式应为 YYYY-MM-DD")
    else:
        target = datetime.now(CN).date()

    start = datetime.combine(target, datetime.min.time())
    end = start + timedelta(days=1)

    records = (
        db.query(PostureRecord)
        .filter(PostureRecord.onenet_time >= start, PostureRecord.onenet_time < end)
        .all()
    )

    total = len(records)
    if total == 0:
        return {"good_posture_minutes": 0, "abnormal_count": 0, "health_score": 100}

    healthy = sum(1 for r in records if r.posture_type in HEALTHY_TYPES)
    abnormal = sum(1 for r in records if r.posture_type in ABNORMAL_TYPES)
    # 每条约 10 秒，转分钟
    good_minutes = round(healthy * 10 / 60)
    # 健康评分 = 正常占比 * 100（排除无人和未知）
    scored = healthy + abnormal
    score = round(healthy / scored * 100) if scored > 0 else 100

    return {
        "good_posture_minutes": good_minutes,
        "abnormal_count": abnormal,
        "health_score": score,
    }


@router.get("/posture/stats/weekly", response_model=list[WeekTrendDayOut])
def weekly_trend(db: Session = Depends(get_db)) -> list[dict]:
    """最近 7 天趋势，每天一个分数。无数据的天返回 null。"""
    today = datetime.now(CN).date()
    results = []

    for i in range(6, -1, -1):
        d = today - timedelta(days=i)
        start = datetime.combine(d, datetime.min.time())
        end = start + timedelta(days=1)

        rows = (
            db.query(PostureRecord.posture_type, func.count())
            .filter(PostureRecord.onenet_time >= start, PostureRecord.onenet_time < end)
            .group_by(PostureRecord.posture_type)
            .all()
        )

        counts = {pt: cnt for pt, cnt in rows}
        total = sum(counts.values())

        if total == 0:
            results.append({"date": d.isoformat(), "score": None})
        else:
            healthy = counts.get("normal", 0)
            abnormal = counts.get("head_down", 0) + counts.get("hunchback", 0)
            scored = healthy + abnormal
            score = round(healthy / scored * 100) if scored > 0 else 100
            results.append({"date": d.isoformat(), "score": score})

    return results


@router.get("/device/status", response_model=DeviceStatusOut)
async def device_status() -> dict:
    """设备在线状态（转发 OneNET）。"""
    return await query_device_status()
