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
    WeekTrendDayOut,
)
from ..services.onenet import query_device_status

router = APIRouter(prefix="/api", tags=["posture"])

CN = ZoneInfo("Asia/Shanghai")

# 姿态分类
ABNORMAL_TYPES = {"head_down", "hunchback"}
HEALTHY_TYPES = {"normal"}
# 无人、未知不计入评分


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

    end = datetime.combine(target, datetime.min.time()) + timedelta(days=1)
    start = end - timedelta(days=days)

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

    时长按实际使用时段计算：记录间隔超过 5 分钟视为不同会话，
    只累加各会话首尾之间的时长，再按良好记录占比分配。
    """
    SESSION_GAP_MINUTES = 5  # 间隔超过此值视为不同会话

    if date:
        try:
            target = datetime.strptime(date, "%Y-%m-%d").date()
        except ValueError:
            raise HTTPException(400, "date 格式应为 YYYY-MM-DD")
    else:
        target = datetime.now(CN).date()

    start = datetime.combine(target, datetime.min.time())
    end = start + timedelta(days=1)

    # 只取有人在座的评分记录（排除 no_person / unknown）
    records = (
        db.query(PostureRecord)
        .filter(
            PostureRecord.onenet_time >= start,
            PostureRecord.onenet_time < end,
            PostureRecord.person_present == True,
            PostureRecord.posture_type.in_(list(HEALTHY_TYPES | ABNORMAL_TYPES)),
        )
        .order_by(PostureRecord.onenet_time.asc())
        .all()
    )

    total = len(records)
    if total == 0:
        return {"good_posture_minutes": 0, "abnormal_count": 0, "health_score": 100}

    # 按间隔拆分会话
    sessions: list[list] = [[records[0]]]
    for r in records[1:]:
        gap = (r.onenet_time - sessions[-1][-1].onenet_time).total_seconds()
        if gap > SESSION_GAP_MINUTES * 60:
            sessions.append([])
        sessions[-1].append(r)

    # 计算各会话时长和良好时长
    total_session_seconds = 0
    good_seconds = 0
    abnormal = 0

    for session in sessions:
        if len(session) < 2:
            # 单条记录按 10 秒估算
            session_seconds = 10
        else:
            session_seconds = (
                session[-1].onenet_time - session[0].onenet_time
            ).total_seconds()
            # 至少按记录数 * 10 秒算（防止时间差为 0）
            session_seconds = max(session_seconds, len(session) * 10)

        s_healthy = sum(1 for r in session if r.posture_type in HEALTHY_TYPES)
        s_abnormal = sum(1 for r in session if r.posture_type in ABNORMAL_TYPES)
        s_scored = s_healthy + s_abnormal

        abnormal += s_abnormal
        total_session_seconds += session_seconds

        if s_scored > 0:
            good_seconds += session_seconds * (s_healthy / s_scored)

    good_minutes = round(good_seconds / 60)
    total_scored = sum(
        1 for r in records if r.posture_type in HEALTHY_TYPES | ABNORMAL_TYPES
    )
    healthy = sum(1 for r in records if r.posture_type in HEALTHY_TYPES)
    score = round(healthy / total_scored * 100) if total_scored > 0 else 100

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
