#!/bin/bash
set -e  # 遇到错误时退出脚本

# 清理临时文件并进入工作目录
cd ~
rm -rf tmp_*
cd polardb_competition_2025/test/
source pg-venv/bin/activate  # 激活虚拟环境
cd ../polardb/

# 编译安装PolarDB
bash build.sh --port=5432 --prefix="$HOME"

# 定义基础路径和数据库目录
BASE_DIR="$HOME"
PGDATA="$BASE_DIR/tmp_polardb_pg_15_primary"
CONFIG_FILE="$PGDATA/postgresql.conf"

# 配置数据库参数
cat >> "$CONFIG_FILE" << EOF

# 性能优化参数
shared_buffers = 4GB
polar_xlog_queue_buffers = 2GB
maintenance_work_mem = 4GB
max_parallel_maintenance_workers = 16
max_parallel_workers = 16

EOF

# 重启数据库使配置生效
export PATH="$BASE_DIR/tmp_polardb_pg_15_base/bin:$PATH"
pg_ctl -D "$PGDATA" restart

# 数据库用户和库配置
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"

# 创建用户和数据库
psql -h 127.0.0.1 -p 5432 -U postgres << EOF
-- 创建超级用户
CREATE USER $USER WITH LOGIN SUPERUSER PASSWORD '$PASSWORD';
-- 创建测试数据库
DROP DATABASE IF EXISTS $DBNAME;
CREATE DATABASE $DBNAME OWNER $USER;
EOF

# 创建向量表结构
VECTOR_DIM=784
psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME" << EOF
-- 启用向量插件
CREATE EXTENSION IF NOT EXISTS vector;
-- 创建向量存储表
DROP TABLE IF EXISTS vector_table;
CREATE TABLE vector_table (id bigserial PRIMARY KEY, embedding vector($VECTOR_DIM));
ALTER TABLE vector_table ALTER COLUMN embedding SET STORAGE PLAIN;
EOF

# 导入测试数据
cd ../test
DATASET_NAME="fashion-mnist-784-euclidean"

python3 load.py \
  --host 127.0.0.1 \
  --port 5432 \
  --database "$DBNAME" \
  --user "$USER" \
  --password "$PASSWORD" \
  --filename "${DATASET_NAME}.hdf5" \
  --tablename vector_table \
  --batch_size 1000 \
  --num_workers 8

# 创建向量索引
psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME" << EOF
CREATE INDEX ON vector_table
USING hnsw (embedding vector_l2_ops)
WITH (m = 4, ef_construction = 8);
EOF

# 执行查询测试
python3 query.py \
  --host 127.0.0.1 \
  --port 5432 \
  --database "$DBNAME" \
  --user "$USER" \
  --password "$PASSWORD" \
  --table_name vector_table \
  --hdf5_file "${DATASET_NAME}.hdf5" \
  --k 10 \
  --metric l2 \
  --max_vid 1000000 \
  --workers 8 \
  --pgvector_hnsw_ef_search 120