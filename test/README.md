# 本地测试流程

## 初始化

1.  环境配置与初始化
    
    ```bash
    # $BASE_DIR为项目根目录
    cd $BASE_DIR/test
    
    # install python
    sudo apt-get install -y python3-pip python3-requests python3-venv
    
    # create and activate venv
    python3 -m venv pg-venv
    source pg-venv/bin/activate
    
    # install python dep
    pip3 install psycopg2 numpy h5py -i https://mirrors.aliyun.com/pypi/simple/
    ```
    
2.  下载数据集
    
    ```bash
    # 以下为 500 MB 以下的数据集，适合于本地运行，注意在测试时选择正确的距离操作符
    # 可以使用 wget http://ann-benchmarks.com/{dataset_name}.hdf5 的方式拉取数据
    "fashion-mnist-784-euclidean": 217 MB, 784 x 60,000, Euclidean (L2)
    "mnist-784-euclidean": 217 MB, 784 x 60,000, Euclidean (L2)
    "glove-25-angular": 121 MB, 25 x 1,183,514, Angular (Cosine)
    "glove-50-angular": 235 MB, 50 x 1,183,514, Angular (Cosine)
    "glove-100-angular": 463 MB, 100 x 1,183,514, Angular (Cosine)
    "sift-128-euclidean": 501 MB, 128 x 1,000,000, Euclidean (L2)
    "nytimes-256-angular": 301 MB, 256 x 290,000, Angular (Cosine)
    "nytimes-16-angular": 26 MB, 16 x 290,000, Angular (Cosine)
    "lastfm-64-dot": 135 MB, 65 x 292,385, Angular (Cosine)
    wget http://ann-benchmarks.com/mnist-784-euclidean.hdf5
    
    # 使用 wget https://github.com/fabiocarrara/str-encoders/releases/download/v0.1.3/{dataset_name}.hdf5 的方式拉取数据
    "coco-i2i-512-angular": 136 MB, 512 x 113,287, Angular (Cosine)
    "coco-t2i-512-angular": 136 MB, 512 x 113,287, Angular (Cosine)
    wget https://github.com/fabiocarrara/str-encoders/releases/download/v0.1.3/coco-t2i-512-angular.hdf5
    ```
    *   推荐使用 [ann-benchmarks](https://github.com/erikbern/ann-benchmarks) 中提供的数据数据集
        

## 拉取镜像并运行容器

1.  推荐基于 Docker 容器运行测试：当前已经将开发环境打包为镜像，可以直接拉取镜像进行开发 
    
    ```bash
    docker pull registry.cn-hangzhou.aliyuncs.com/polardb_pg/polardb_pg_devel:ubuntu24.04
    ```
    *   流程参考：[基于 Docker 容器开发 | PolarDB for PostgreSQL](https://apsaradb.github.io/PolarDB-for-PostgreSQL/zh/development/dev-on-docker.html)
        
    *   开发环境：如果想要添加更多依赖，定制开发环境，可以在当前 Dockerfile 基础上进行开发
        
        *   [定制开发环境 | PolarDB for PostgreSQL](https://apsaradb.github.io/PolarDB-for-PostgreSQL/zh/development/customize-dev-env.html)
            
        *   Dockerfile：[ApsaraDB/polardb-pg-docker-images](https://github.com/ApsaraDB/polardb-pg-docker-images)

    *   想直接在 Linux 中开发可以参考 Dockerfile：[ApsaraDB/polardb-pg-docker-images](https://github.com/ApsaraDB/polardb-pg-docker-images) 中的依赖
        

1.  创建并运行容器

    ```bash
    # 设置共享内存大小, 需要大于maintenance_work_mem
    SHM_SIZE=8g
    # 进入项目根目录后运行容器
    docker run -it -p 5432:5432 \
        -v $PWD:/home/postgres/polardb_competition_2025 \
        --shm-size=$SHM_SIZE \
        --name polardb_competition_2025_devel \
        --cap-add=SYS_PTRACE --privileged=true \
        registry.cn-hangzhou.aliyuncs.com/polardb_pg/polardb_pg_devel:ubuntu24.04 \
        bash
    
    # 设置目录权限, 避免无法运行
    cd polardb_competition_2025
    sudo chmod -R a+wr ./
    sudo chown -R postgres:postgres ./
    ```

## 编译并配置数据库

1.  参考 开发部署流程，启动容器进行测试
    
2.  数据库编译
    
    ```bash
    cd $BASE_DIR/polardb
    # 可以使用 bash build.sh --help 查看可以配置的选项
    bash build.sh --port=5432 --prefix="$HOME"
    ```
    
3.  设置数据库参数
    
    *   shared\_buffers 应当小于当前最大物理内存的 40%，适合设置为 25%。需要注意的是，如果该值小于向量数据与索引的大小，可能因无法完全缓存向量数据与索引，导致索引构建与向量查询性能严重下降。
        
    *   polar\_xlog\_queue\_buffers 应该设置为 shared\_buffers 的 1/8，如果不设置可能无法启动
        
    *   maintenance\_work\_mem 为索引构建使用的缓存大小，该值应该大于索引大小，以加快索引构建
        
    *   max\_parallel\_workers 控制最大并发度
        
    *   max\_parallel\_maintenance\_workers 控制索引构建的最大并发度，该值不能大于 max\_parallel\_workers
        
    ```bash
    # PolarDB 安装目录
    BASE_DIR="$HOME"
    
    # PolarDB DATA/BIN 目录
    PGDATA="$BASE_DIR/tmp_polardb_pg_15_primary"
    CONFIG_FILE="$PGDATA/postgresql.conf"
    
    # 设置参数
    cat >> "$CONFIG_FILE" << EOF
    
    shared_buffers = 2GB
    polar_xlog_queue_buffers = 256MB
    maintenance_work_mem = 1GB
    max_parallel_maintenance_workers = 2
    max_parallel_workers = 2
    
    EOF
    
    # => 重启数据库
    export PATH="$BASE_DIR/tmp_polardb_pg_15_base/bin:$PATH"
    pg_ctl -D "$PGDATA" restart
    ```

## 插入数据并建立索引

1.  初始化数据库表结构
    
    ```bash
    USER="testuser"
    PASSWORD="testPawword"
    DBNAME="testdb"
    
    # 使用超级用户登录数据库
    psql -h 127.0.0.1 -p 5432 -U postgres << EOF
    -- 创建数据库用户 
    CREATE USER $USER WITH LOGIN SUPERUSER PASSWORD '$PASSWORD';
    -- 创建测试数据库
    DROP DATABASE IF EXISTS $DBNAME;
    CREATE DATABASE $DBNAME OWNER $USER;
    EOF
    
    # 根据向量维度创建表结构
    VECTOR_DIM=784
    # 创建插件与测试表
    psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME" << EOF
    -- 创建插件
    CREATE EXTENSION IF NOT EXISTS vector;
    -- 创建表
    DROP TABLE IF EXISTS vector_table;
    CREATE TABLE vector_table (id bigserial PRIMARY KEY, embedding vector($VECTOR_DIM));
    ALTER TABLE vector_table ALTER COLUMN embedding SET STORAGE PLAIN;
    EOF
    ```
    
2.  插入数据

    ```bash
    USER="testuser"
    PASSWORD="testPawword"
    DBNAME="testdb"
    DATASET_NAME="mnist-784-euclidean"

    # cd $BASE_DIR/test && source pg-venv/bin/activate
    python3 load.py \
    --host 127.0.0.1 \
    --port 5432 \
    --database $DBNAME \
    --user $USER \
    --password $PASSWORD \
    --filename "${DATASET_NAME}.hdf5" \
    --tablename vector_table \
    --batch_size 1000 \
    --num_workers 8
    ```

3.  建立索引

    *   当前支持两种类型的索引，分别是 IVFFLAT 和 HNSW

    *   需要注意创建向量索引使用的操作符类型，如果使用的向量索引操作符类型与查询搜索时使用的类型不匹配，查找时将无法利用索引加速，当前主要使用 vector_l2_ops(<->)/vector_cosine_ops(<=>)/vector_ip_ops(<#>) 这三种操作符
    
    ```sql
    psql -h 127.0.0.1 -p 5432 -U testuser -d testdb -W  # 输入密码 testPawword
    
    -- 创建 HNSW 索引
    CREATE INDEX ON vector_table 
    USING hnsw (embedding vector_l2_ops) 
    WITH (m = 16, ef_construction = 64);
    
    -- 创建 IVFFLAT 索引
    CREATE INDEX ON vector_table
    USING ivfflat (embedding vector_l2_ops)
    WITH (lists = 2000);
    ```

## 运行基准测试

```bash
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"
DATASET_NAME="mnist-784-euclidean"

# 基本使用方式
usage: query.py 
  [-h] [--explain EXPLAIN] --hdf5_file HDF5_FILE 
  --table_name TABLE_NAME [--host HOST] [--port PORT] 
  [--database DATABASE] [--user USER] [--password PASSWORD] 
  [--k K] [--metric {l2,cosine,inner}]
  [--max_vid MAX_VID] [--workers WORKERS] 
  [--pgvector_hnsw_ef_search PGVECTOR_HNSW_EF_SEARCH] 
  [--pgvector_ivf_probes PGVECTOR_IVF_PROBES]
  [--query_num QUERY_NUM] [--offset OFFSET]
  
 python3 query.py \
    --hdf5_file mnist-784-euclidean.hdf5 \
    --table_name vector_table \
    --host 127.0.0.1 \
    --port 5432 \
    --database testdb \
    --user testuser \
    --password testPassword
```

*   `--hdf5_file`: 指定 HDF5 格式的输入文件路径，包含测试向量和真实近邻数据
    
### 数据库连接参数

*   `--host`: PostgreSQL 数据库主机地址，默认为 127.0.0.1
    
*   `--port`: PostgreSQL 数据库端口号，默认为 5432
    
*   `--database`: 数据库名称，默认为 postgres
    
*   `--user`: 数据库用户名，默认为 postgres
    
*   `--password`: 数据库密码
    
*   `--table_name`: 指定要查询的数据库表名
    
*   `--explain`: 是否启用 SQL 解释模式，开启后会输出查询执行计划而非实际执行查询
    

### 查询配置参数

*   `--k`: 指定返回最近邻的数量，默认为 10
    
*   `--metric`: 指定距离度量方式，可选'l2'(欧氏距离 <->)、'cosine'(余弦距离 <=>)、'inner'(内积 <#>)，默认为'l2'，**在进行测试时，应该按照数据集要求选择距离度量方式，避免 Recall 计算异常**
    
*   `--max_vid`: 最大向量 ID 限制，用于计算召回率时过滤超出范围的向量 ID（通常应该 > 向量总数）
    
*   `--workers`: 指定并行查询的进程数量，默认为 8
    

### 索引搜索参数

*   `--pgvector_hnsw_ef_search`: HNSW 索引搜索参数，控制搜索过程中候选节点的数量
    
*   `--pgvector_ivf_probes`: IVF 索引搜索参数，控制探测的聚类中心数量
    

### 查询控制参数

*   `--query_num`: 指定要执行的查询数量，0 表示执行所有查询
    
*   `--offset`: 指定查询起始偏移量，用于分段执行查询
    

### HNSW 索引查询

```bash
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"
DATASET_NAME=

python3 query.py \
  --host 127.0.0.1 \
  --port 5432 \
  --database $DBNAME \
  --user $USER \
  --password $PASSWORD \
  --table_name vector_table \
  --hdf5_file "${DATASET_NAME}.hdf5" \
  --k 10 \
  --metric l2 \
  --max_vid 1000000 \
  --workers 8 \
  --pgvector_hnsw_ef_search 120
```

### IVF\_FLAT 索引查询

```bash
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"
DATASET_NAME=

python3 query.py \
  --host 127.0.0.1 \
  --port 5432 \
  --database $DBNAME \
  --user $USER \
  --password $PASSWORD \
  --table_name vector_table \
  --hdf5_file "${DATASET_NAME}.hdf5" \
  --k 10 \
  --metric l2 \
  --max_vid 1000000 \
  --workers 8 \
  --pgvector_ivf_probes 30
```

### EXPLAIN 打印执行计划

```bash
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"
DATASET_NAME=

python3 query.py \
  --host 127.0.0.1 \
  --port 5432 \
  --database $DBNAME \
  --user $USER \
  --password $PASSWORD \
  --table_name vector_table \
  --hdf5_file "${DATASET_NAME}.hdf5" \
  --k 10 \
  --metric l2 \
  --max_vid 1000000 \
  --workers 8 \
  --explain true \
  --query_num 1 \
  --pgvector_hnsw_ef_search 120
```
