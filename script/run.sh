#!/bin/bash
set -euo pipefail

BASE_DIR=/workspaces/polardb_competition_2025
DATA_DIR="$BASE_DIR/test"

# DB config
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"

# Args: <dataset_name> <task_type: build|search>
DATASET_NAME=${1:-}
TASK_TYPE=${2:-}

usage() {
  echo "Usage: $0 <dataset_name> <task_type>" >&2
  echo "  <task_type>: build | search" >&2
  echo "  <dataset_name>: one of" >&2
  echo "    fashion-mnist-784-euclidean, mnist-784-euclidean, sift-128-euclidean" >&2
  echo "    glove-25-angular, glove-50-angular, glove-100-angular" >&2
  echo "    nytimes-256-angular, nytimes-16-angular, lastfm-64-dot" >&2
  echo "    coco-i2i-512-angular, coco-t2i-512-angular" >&2
}

if [[ -z "${DATASET_NAME}" || -z "${TASK_TYPE}" ]]; then
  usage; exit 1
fi

if [[ "${TASK_TYPE}" != "build" && "${TASK_TYPE}" != "search" ]]; then
  echo "Invalid task_type: ${TASK_TYPE}" >&2
  usage; exit 1
fi

# Dataset metadata
# dims
declare -A DIMS=(
  [fashion-mnist-784-euclidean]=784
  [mnist-784-euclidean]=784
  [sift-128-euclidean]=128
  [glove-25-angular]=25
  [glove-50-angular]=50
  [glove-100-angular]=100
  [nytimes-256-angular]=256
  [nytimes-16-angular]=16
  [lastfm-64-dot]=65
  [coco-i2i-512-angular]=512
  [coco-t2i-512-angular]=512
)
# counts (max_vid)
declare -A COUNTS=(
  [fashion-mnist-784-euclidean]=60000
  [mnist-784-euclidean]=60000
  [sift-128-euclidean]=1000000
  [glove-25-angular]=1183514
  [glove-50-angular]=1183514
  [glove-100-angular]=1183514
  [nytimes-256-angular]=290000
  [nytimes-16-angular]=290000
  [lastfm-64-dot]=292385
  [coco-i2i-512-angular]=113287
  [coco-t2i-512-angular]=113287
)
# metrics: l2 | cosine | inner
declare -A METRICS=(
  [fashion-mnist-784-euclidean]=l2
  [mnist-784-euclidean]=l2
  [sift-128-euclidean]=l2
  [glove-25-angular]=cosine
  [glove-50-angular]=cosine
  [glove-100-angular]=cosine
  [nytimes-256-angular]=cosine
  [nytimes-16-angular]=cosine
  [lastfm-64-dot]=inner
  [coco-i2i-512-angular]=cosine
  [coco-t2i-512-angular]=cosine
)

if [[ -z "${DIMS[${DATASET_NAME}]:-}" ]]; then
  echo "Unknown dataset: ${DATASET_NAME}" >&2
  usage; exit 1
fi

VECTOR_DIM=${DIMS[${DATASET_NAME}]}
MAX_VID=${COUNTS[${DATASET_NAME}]}
METRIC=${METRICS[${DATASET_NAME}]}

# Choose pgvector opclass by metric
case "$METRIC" in
  l2)    OPCLASS="vector_l2_ops" ;;
  cosine) OPCLASS="vector_cosine_ops" ;;
  inner)  OPCLASS="vector_ip_ops" ;;
  *) echo "Unsupported metric: $METRIC" >&2; exit 1 ;;
esac

# Build dataset-specific table name (sanitize to valid identifier)
RAW_TABLE_NAME="${DATASET_NAME}_vector_table"
TABLE_NAME=$(echo "$RAW_TABLE_NAME" | tr -c '[:alnum:]_' '_')

create_role_and_db() {
psql -h 127.0.0.1 -p 5432 -U postgres << EOF
-- 创建数据库用户 
CREATE USER $USER WITH LOGIN SUPERUSER PASSWORD '$PASSWORD';
-- 创建测试数据库
DROP DATABASE IF EXISTS $DBNAME;
CREATE DATABASE $DBNAME OWNER $USER;
EOF
}

create_schema() {
psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME" << EOF
-- 创建插件
CREATE EXTENSION IF NOT EXISTS vector;
-- 创建表
DROP TABLE IF EXISTS ${TABLE_NAME};
CREATE TABLE ${TABLE_NAME} (id bigserial PRIMARY KEY, embedding vector(${VECTOR_DIM}));
ALTER TABLE ${TABLE_NAME} ALTER COLUMN embedding SET STORAGE PLAIN;
EOF
}

load_data() {
  cd "$DATA_DIR"
  source pg-venv/bin/activate
  python3 load.py \
    --host 127.0.0.1 \
    --port 5432 \
    --database $DBNAME \
    --user "$USER" \
    --password "$PASSWORD" \
    --filename "${DATASET_NAME}.hdf5" \
    --tablename "${TABLE_NAME}" \
    --batch_size 1000 \
    --num_workers 8
}


create_index() {
psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME" << EOF
CREATE INDEX IF NOT EXISTS ${TABLE_NAME}_hnsw ON ${TABLE_NAME}
USING hnsw (embedding ${OPCLASS})
WITH (m = 16, ef_construction = 64);
EOF
}

run_query() {
  cd "$DATA_DIR"
  source pg-venv/bin/activate
  python3 query.py \
    --host 127.0.0.1 \
    --port 5432 \
    --database "$DBNAME" \
    --user "$USER" \
    --password "$PASSWORD" \
    --table_name "${TABLE_NAME}" \
    --hdf5_file "${DATASET_NAME}.hdf5" \
    --k 10 \
    --metric "$METRIC" \
    --max_vid "$MAX_VID" \
    --workers 8 \
    --pgvector_hnsw_ef_search 120
}

case "$TASK_TYPE" in
  build)
    echo "[Task] build: ${DATASET_NAME} -> table=${TABLE_NAME} (${VECTOR_DIM}d, metric=${METRIC})"
    create_role_and_db
    create_schema
    load_data
    create_index
    ;;
  search)
    echo "[Task] search: ${DATASET_NAME} -> table=${TABLE_NAME} (${VECTOR_DIM}d, metric=${METRIC})"
    run_query
    ;;
  *)
    echo "Unknown task type: $TASK_TYPE" >&2; exit 1;;
esac