from datetime import datetime
from zoneinfo import ZoneInfo

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import desc
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import PostureRecord
from ..schemas import PostureRecordOut, TestInsertResponse


router = APIRouter(prefix="/api", tags=["posture"])


def china_now() -> datetime:
    return datetime.now(ZoneInfo("Asia/Shanghai")).replace(tzinfo=None)


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
    except IntegrityError:
        db.rollback()
        raise HTTPException(status_code=409, detail="duplicate test record")
    db.refresh(record)
    return TestInsertResponse(id=record.id, message="inserted")


@router.get("/posture/latest", response_model=PostureRecordOut)
def latest_posture(db: Session = Depends(get_db)) -> PostureRecord:
    record = (
        db.query(PostureRecord)
        .order_by(desc(PostureRecord.onenet_time), desc(PostureRecord.id))
        .first()
    )
    if record is None:
        raise HTTPException(status_code=404, detail="no posture records")
    return record
