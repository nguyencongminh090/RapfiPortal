#!/bin/bash

# start.sh - Script khởi động nhanh server Gomoku-VN (Play3CR)
# Chạy lệnh này bằng: bash start.sh (hoặc ./start.sh)

echo "🔄 Đang dọn dẹp port 3000..."
# Tìm và tắt tiến trình đang chiếm port 3000 để tránh lỗi "Address already in use"
fuser -k 3000/tcp 2>/dev/null

echo "🚀 Bắt đầu khởi động Node.js Server..."
# Chạy server
npm start
