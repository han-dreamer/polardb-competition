# 本地测试流程

注意：请在类 Unix 环境中进行运行测试，并提前配置好 docker 相关的环境

## 下载源代码

```bash
# 下载源代码，请各位选手根据自己私有仓库的地址修改拉取的库地址
git clone https://gitee.com/polardb-tianchi/polardb_competition_2025.git

# 如果已经配置 ssh 密钥，可使用 git@gitee.com 地址拉取代码
git clone git@gitee.com:polardb-tianchi/polardb_competition_2025.git
```
代码下载完成后，进入源码目录
```bash
cd polardb_competition_2025
```

## 拉取镜像并运行容器

1.  推荐基于 Docker 容器运行测试：当前已经将开发环境打包为镜像，可以直接拉取镜像进行开发 
    
    ```bash
    # 如果权限不足，请使用 sudo 或超级用户 root 执行
    # 如果拉取失败，请先检查网络问题与 docker 版本问题，若 docker 版本过低，请安装最新版本
    docker pull registry.cn-hangzhou.aliyuncs.com/polardb_pg/polardb_pg_devel:ubuntu24.04
    ```
    *   流程参考：[基于 Docker 容器开发 | PolarDB for PostgreSQL](https://apsaradb.github.io/PolarDB-for-PostgreSQL/zh/development/dev-on-docker.html)
        
    *   定制开发环境：如果想要添加更多依赖，定制开发环境，可以在当前 Dockerfile 基础上进行开发
        
        *   [定制开发环境 | PolarDB for PostgreSQL](https://apsaradb.github.io/PolarDB-for-PostgreSQL/zh/development/customize-dev-env.html)
            
        *   Dockerfile：[ApsaraDB/polardb-pg-docker-images](https://github.com/ApsaraDB/polardb-pg-docker-images)

    *   想直接在 Linux 中开发可以参考 Dockerfile：[ApsaraDB/polardb-pg-docker-images](https://github.com/ApsaraDB/polardb-pg-docker-images) 中的依赖
        

2.  创建并运行容器

    *   确保当前处于竞赛仓库根目录 polardb_competition_2025 下，我们需要基于开发镜像创建一个容器
        
        *   将当前目录 (polardb_competition_2025) 作为一个 volume 挂载到容器中，这样可以方便得同步容器内外的数据，便于在容器外编辑，容器内编译

        *   在创建容器时，需要设置共享内存大小，该值必须大于 maintenance_work_mem，该参数的作用详见「设置数据库参数」小节

        *   容器名称可以使用 --name 参数配置

    ```bash
    # 设置共享内存大小, 需要大于 maintenance_work_mem
    SHM_SIZE=4g
    # 创建容器, 注意需要在竞赛仓库根目录 polardb_competition_2025 下运行
    docker run -it \
        -v $PWD:/home/postgres/polardb_competition_2025 \
        --shm-size=$SHM_SIZE \
        --name polardb_competition_2025_devel \
        --cap-add=SYS_PTRACE --privileged=true \
        registry.cn-hangzhou.aliyuncs.com/polardb_pg/polardb_pg_devel:ubuntu24.04 \
        bash
    ```
3.  设置目录权限
    
    当容器成功启动后，会发现用户名已经变为 postgres，这表示当前已经进入容器中。为了避免后续编译时遇到问题，需要设置目录权限

    *   如果不想修改目录权限，可以在拉取镜像并运行容器后，在容器内部拉取代码

    ```bash
    # 设置目录权限, 避免无法运行
    cd polardb_competition_2025
    sudo chmod -R a+wr ./
    sudo chown -R postgres:postgres ./
    ```

## python 环境配置

为了运行本文中提到的测试脚本，需要构建一个 python 环境，推荐创建 venv 环境，也可以直接在全局环境中安装对应的包 

```bash
# 建议将虚拟环境创建在 polardb_competition_2025/test 目录下，也可以自行选择其他目录
cd test

# 安装 python 相关库
sudo apt-get update
sudo apt-get install -y python3-pip python3-requests python3-venv

# 创建并激活 venv 环境
python3 -m venv pg-venv
source pg-venv/bin/activate

# 安装 python 包
pip3 install psycopg2 numpy h5py -i https://mirrors.aliyun.com/pypi/simple/
```
    
## 下载数据集
    
```bash
# 以下为 500 MB 以下的数据集，适合于本地运行，参数含义如下: 
# "dataset_name": dataset_size, dimension x vector_nums, distance_operator
# 特别注意: 请在测试时选择正确的距离操作符

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

# 使用 wget https://github.com/fabiocarrara/str-encoders/releases/download/v0.1.3/{dataset_name}.hdf5 的方式拉取数据
"coco-i2i-512-angular": 136 MB, 512 x 113,287, Angular (Cosine)
"coco-t2i-512-angular": 136 MB, 512 x 113,287, Angular (Cosine)
```
*   推荐使用 [ann-benchmarks](https://github.com/erikbern/ann-benchmarks) 中提供的数据数据集

*   如果无法拉取数据集，可以到 [天池平台](https://tianchi.aliyun.com/competition/entrance/532409/information) 上下载数据集并解压使用
 
        
## 编译并配置数据库

1.  退出 test 目录并进入 polardb 目录

    ```bash
    cd ../polardb
    ```
    
2.  数据库编译
    
    ```bash
    # 可以使用 bash build.sh --help 查看可以配置的选项
    bash build.sh --port=5432 --prefix="$HOME"
    ```
    
3.  设置数据库参数
    
    *   shared\_buffers 应当小于当前最大物理内存的 40%，适合设置为 25%。需要注意的是，如果该值小于向量数据与索引的大小，可能因无法完全缓存向量数据与索引，导致索引构建与向量查询性能严重下降。
        
    *   polar\_xlog\_queue\_buffers 应该设置为 shared\_buffers 的 1/8，如果不设置可能无法启动
        
    *   maintenance\_work\_mem 为索引构建使用的缓存大小，该值应该大于索引大小，以加快索引构建，避免产生大量 IO
        
    *   max\_parallel\_workers 控制最大并发度
        
    *   max\_parallel\_maintenance\_workers 控制索引构建的最大并发度，该值不能大于 max\_parallel\_workers
        
    ```bash
    # 设置 PolarDB 安装目录
    BASE_DIR="$HOME"

    # 设置 PolarDB DATA/BIN 目录
    PGDATA="$BASE_DIR/tmp_polardb_pg_15_primary"
    CONFIG_FILE="$PGDATA/postgresql.conf"

    # 设置参数
    cat >> "$CONFIG_FILE" << EOF

    shared_buffers = 2GB
    polar_xlog_queue_buffers = 256MB
    maintenance_work_mem = 2GB
    max_parallel_maintenance_workers = 8
    max_parallel_workers = 8

    EOF

    # => 重启数据库
    export PATH="$BASE_DIR/tmp_polardb_pg_15_base/bin:$PATH"
    pg_ctl -D "$PGDATA" restart
    ```

## 插入数据并建立索引

1.  退出 polardb 目录，进入 test 目录

    ```bash
    cd ../test
    ```

2.  初始化数据库表结构

    请根据选择的数据集选择对应的向量维度，如使用 nytimes-16-angular 则需将 VECTOR_DIM 设置为 16
    
    ```bash
    # 根据向量维度创建表结构
    VECTOR_DIM=

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
    
3.  插入数据

    请根据选择数据集选择对应的数据集名称，如使用 nytimes-16-angular 则需将 DATASET_NAME 设置为 nytimes-16-angular

    ```bash
    # 填写数据集名称
    DATASET_NAME=

    USER="testuser"
    PASSWORD="testPawword"
    DBNAME="testdb"

    # 确保安装 python 依赖包或已激活虚拟 venv环境
    source pg-venv/bin/activate

    # 运行测试
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

4.  建立索引

    *   当前支持两种类型的索引，分别是 IVFFLAT 和 HNSW

    *   需要注意创建向量索引使用的操作符类型，如果使用的向量索引操作符类型与查询搜索时使用的类型不匹配，**查找时将无法利用索引加速**，当前主要使用 vector_l2_ops(<->)/vector_cosine_ops(<=>)/vector_ip_ops(<#>) 这三种操作符

    *   以 nytimes-16-angular 数据集为例，数据集操作符类型为 Angular (Cosine)，因此应该使用 vector_cosine_ops 操作符构建索引
    
    ```sql
    -- 创建 HNSW 索引
    CREATE INDEX ON vector_table 
    USING hnsw (embedding vector_cosine_ops) 
    WITH (m = 16, ef_construction = 64);

    -- 创建 IVFFLAT 索引
    CREATE INDEX ON vector_table
    USING ivfflat (embedding vector_cosine_ops)
    WITH (lists = 2000);
    ```

    *   可以使用 `psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME"` 连接数据库创建索引

    *   小贴士：可以执行 `\timing on` 显示构建索引的时间

    ```bash
    USER="testuser"
    PASSWORD="testPawword"
    DBNAME="testdb"
    
    psql -h 127.0.0.1 -p 5432 -U "$USER" -d "$DBNAME" << EOF
    CREATE INDEX ON vector_table 
    USING hnsw (embedding vector_cosine_ops) 
    WITH (m = 16, ef_construction = 64);
    EOF
    ```

## 运行基准测试

```bash
USER="testuser"
PASSWORD="testPawword"
DBNAME="testdb"
DATASET_NAME=

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
    

### HNSW 索引查询示例

*   请注意设置 `--metric` 参数的值，需要与数据集中指定的操作符保持一致，如使用 nytimes-16-angular，因为其操作符为 Angular (Cosine)，需要将 `--metric` 参数设置为 'cosine'

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
  --metric cosine \
  --workers 8 \
  --pgvector_hnsw_ef_search 120
```

### IVF\_FLAT 索引查询示例

*   请注意设置 `--metric` 参数的值，需要与数据集中指定的操作符保持一致，如使用 nytimes-16-angular，因为其操作符为 Angular (Cosine)，需要将 `--metric` 参数设置为 'cosine'

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
  --metric cosine \
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
  --metric cosine \
  --workers 8 \
  --explain true \
  --query_num 1 \
  --pgvector_hnsw_ef_search 120
```
