CREATE DATABASE IF NOT EXISTS posture_monitor
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'posture_user'@'127.0.0.1' IDENTIFIED BY 'CHANGE_ME';
GRANT SELECT, INSERT, UPDATE, DELETE ON posture_monitor.* TO 'posture_user'@'127.0.0.1';
FLUSH PRIVILEGES;

USE posture_monitor;

CREATE TABLE IF NOT EXISTS posture_records (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    device_id VARCHAR(64) NOT NULL DEFAULT 'main',
    posture_type VARCHAR(20) NOT NULL,
    person_present BOOLEAN NOT NULL DEFAULT FALSE,
    ambient_lux FLOAT NULL,
    fill_light_on BOOLEAN NOT NULL DEFAULT FALSE,
    onenet_time DATETIME(3) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_device_time (device_id, onenet_time),
    INDEX idx_device_time (device_id, onenet_time),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
