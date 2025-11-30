# pgvector 性能优化建议

基于源代码深度分析，以下是按优先级排序的优化方案。

## 🔥 高优先级优化（强烈推荐）

### 1. IVFFlat 中心选择 SIMD 并行化 ⭐⭐⭐⭐⭐

**位置**: `polardb/external/pgvector/src/ivfbuild.c:166-175`

**问题**:
- 索引构建时，每个向量都要遍历所有聚类中心计算距离（O(n×k)复杂度）
- 当lists=100时，每个向量要计算100次距离
- **这是索引构建的最大瓶颈**

**当前代码**:
```c
/* Find the list that minimizes the distance */
for (int i = 0; i < centers->length; i++)
{
    distance = DatumGetFloat8(FunctionCall2Coll(
        buildstate->procinfo,
        buildstate->collation,
        value,
        PointerGetDatum(VectorArrayGet(centers, i))
    ));

    if (distance < minDistance)
    {
        minDistance = distance;
        closestCenter = i;
    }
}
```

**优化方案**:
使用 SIMD 批量计算所有中心的距离，一次性找到最近的中心。

**预期收益**: 索引构建速度提升 **40-60%**

---

### 2. IVFFlat 查询中心选择 SIMD 优化 ⭐⭐⭐⭐⭐

**位置**: `polardb/external/pgvector/src/ivfscan.c:36-107`

**问题**:
- 每次查询都要遍历所有list页，计算与所有中心的距离
- lists=100时，每次查询计算100次距离
- **这是查询的最大瓶颈**

**当前代码**:
```c
static void GetScanLists(IndexScanDesc scan, Datum value)
{
    /* Search all list pages */
    while (BlockNumberIsValid(nextblkno))
    {
        for (OffsetNumber offno = FirstOffsetNumber; offno <= maxoffno; offno++)
        {
            IvfflatList list = (IvfflatList) PageGetItem(...);

            // 单个距离计算
            distance = DatumGetFloat8(
                so->distfunc(so->procinfo, so->collation,
                    PointerGetDatum(&list->center), value)
            );

            // 维护top-k heap
            if (listCount < so->maxProbes) { ... }
        }
    }
}
```

**优化方案**:
1. 收集所有中心到一个连续数组
2. 使用 SIMD 批量计算所有距离
3. 使用 SIMD 指令找到top-k个最近的

**预期收益**: 查询速度提升 **50-70%**（QPS提升50-70%）

---

### 3. 元数据页缓存 ⭐⭐⭐⭐

**位置**:
- HNSW: `polardb/external/pgvector/src/hnswutils.c:296-326`
- HNSW Scan: `polardb/external/pgvector/src/hnswscan.c:28`
- IVFFlat: `polardb/external/pgvector/src/ivfflat.c`

**问题**:
- 每次查询/插入都要读取元数据页
- 元数据很少变化，但每次都从磁盘读取并加锁

**当前代码**:
```c
void HnswGetMetaPageInfo(Relation index, int *m, HnswElement * entryPoint)
{
    // 每次都读取磁盘
    buf = ReadBuffer(index, HNSW_METAPAGE_BLKNO);
    LockBuffer(buf, BUFFER_LOCK_SHARE);
    page = BufferGetPage(buf);
    metap = HnswPageGetMeta(page);

    // 读取数据...

    UnlockReleaseBuffer(buf);
}
```

**优化方案**:
使用 PostgreSQL 的 relation cache (`rd_amcache`) 缓存元数据。

**预期收益**:
- 每次查询节省1次磁盘I/O和锁操作
- QPS提升 **10-20%**

---

### 4. IVFFlat Top-K 堆优化 ⭐⭐⭐⭐

**位置**: `polardb/external/pgvector/src/ivfscan.c:120`

**问题**:
- 使用 tuplesort 对所有候选向量完整排序
- 实际只需要 top-k 个结果
- 排序开销: O(n log n)，实际只需 O(n log k)

**当前代码**:
```c
static void GetScanItems(IndexScanDesc scan, Datum value)
{
    tuplesort_reset(so->sortstate);

    // 扫描多个lists，添加所有候选...
    while (so->listIndex < so->maxProbes && ...) {
        // 添加所有候选到sort
        tuplesort_puttupleslot(so->sortstate, slot);
    }

    // 完整排序
    tuplesort_performsort(so->sortstate);
}
```

**优化方案**:
1. 使用固定大小的优先队列（pairing heap）替代完整排序
2. 只保留top-k个最佳候选
3. 当堆满时，新候选只有比堆顶更好才加入

**预期收益**:
- 查询速度提升 **20-30%**
- 内存使用减少 **50-80%**

---

## 🚀 中优先级优化

### 5. HNSW 访问过节点的布隆过滤器 ⭐⭐⭐

**位置**: `polardb/external/pgvector/src/hnswutils.c:817-980`

**问题**:
- 使用哈希表跟踪已访问节点
- 每次检查都要哈希计算和查找

**优化方案**:
1. 先用布隆过滤器快速过滤
2. 只有通过布隆过滤器的才查哈希表

**预期收益**: HNSW查询提升 **10-15%**

---

### 6. 批量距离计算缓存 ⭐⭐⭐

**位置**:
- `polardb/external/pgvector/src/hnswutils.c:1057-1158`
- HNSW 邻居选择

**问题**:
- 同样的向量对可能多次计算距离
- 没有缓存机制

**优化方案**:
使用临时哈希表缓存距离计算结果，键为向量对ID。

**预期收益**: HNSW构建提升 **10-20%**

---

### 7. IVFFlat K-means 提前终止 ⭐⭐⭐

**位置**: `polardb/external/pgvector/src/ivfkmeans.c:404-464`

**问题**:
- 固定迭代500次
- 很多情况下10-20次就收敛了

**当前代码**:
```c
for (int iteration = 0; iteration < 500; iteration++)
{
    // 聚类更新...
    // 没有收敛检测
}
```

**优化方案**:
```c
double prevInertia = DBL_MAX;
for (int iteration = 0; iteration < 500; iteration++)
{
    // 聚类更新...

    // 检查收敛
    double inertiaChange = fabs(prevInertia - currentInertia);
    if (inertiaChange / prevInertia < 0.0001)
        break;  // 收敛了

    prevInertia = currentInertia;
}
```

**预期收益**: 索引构建时间减少 **20-40%**

---

### 8. 预取优化 ⭐⭐⭐

**位置**:
- `polardb/external/pgvector/src/hnswscan.c:38-42`
- HNSW层级搜索

**问题**:
- 逐层搜索时，下一层的页面还在磁盘上
- 没有预取下一层候选节点

**优化方案**:
在处理当前层时，使用 `PrefetchBuffer` 预取下一层的页面。

**预期收益**: 磁盘I/O密集场景提升 **20-30%**

---

## 💡 低优先级优化（长期考虑）

### 9. 自定义WAL记录
- 减少WAL体积
- 提升写入性能
- **实现复杂度高**

### 10. 内存分配器优化
- 使用slab allocator
- 减少内存碎片
- **收益较小**

---

## 📊 优化实施优先级建议

根据你的目标（高QPS，Recall ≥ 0.85），建议按以下顺序实施：

### 第一阶段（最大收益）：
1. ✅ **SIMD距离计算** （已完成）
2. 🔥 **IVFFlat查询中心选择SIMD优化** (预计QPS +50-70%)
3. 🔥 **元数据页缓存** (预计QPS +10-20%)

**预期综合收益**: QPS提升 **60-90%**

### 第二阶段（进一步优化）：
4. **IVFFlat Top-K堆优化** (预计QPS +20-30%)
5. **IVFFlat构建中心选择SIMD优化** (索引构建 +40-60%)
6. **K-means提前终止** (索引构建 +20-40%)

**预期综合收益**: QPS再提升 **20-30%**，索引构建快 **60-100%**

### 第三阶段（锦上添花）：
7. HNSW布隆过滤器
8. 预取优化
9. 距离缓存

**预期综合收益**: QPS再提升 **10-20%**

---

## 🎯 针对比赛的特别建议

### 索引选择策略：
1. **对于高维向量（>128维）**: 使用 **IVFFlat**
   - 构建更快
   - 查询性能稳定
   - 易于优化

2. **对于低维向量（≤128维）**: 使用 **HNSW**
   - 召回率更高
   - 查询速度更快
   - 但构建较慢

### 参数调优建议（IVFFlat）：
```sql
-- 索引构建
CREATE INDEX ON vectors USING ivfflat (embedding vector_l2_ops)
WITH (lists = 100);

-- 查询参数
SET ivfflat.probes = 10;  -- 根据recall要求调整
```

**Lists数量选择**:
- 数据量 < 100K: lists = 100
- 数据量 100K-1M: lists = 200-500
- 数据量 > 1M: lists = 1000+

**Probes数量**（影响recall）:
- Recall ≥ 0.85: 建议 probes ≥ 10
- Recall ≥ 0.90: 建议 probes ≥ 20
- Recall ≥ 0.95: 建议 probes ≥ 50

### 参数调优建议（HNSW）：
```sql
-- 索引构建
CREATE INDEX ON vectors USING hnsw (embedding vector_l2_ops)
WITH (m = 16, ef_construction = 64);

-- 查询参数
SET hnsw.ef_search = 100;  -- 根据recall要求调整
```

**M参数**（连接数）:
- 较小数据集: m = 16
- 较大数据集: m = 32
- 高召回要求: m = 64

**ef_search参数**（影响recall）:
- Recall ≥ 0.85: ef_search ≥ 80
- Recall ≥ 0.90: ef_search ≥ 120
- Recall ≥ 0.95: ef_search ≥ 200

---

## 📝 下一步行动

建议立即实施优化2和3：

1. **IVFFlat查询中心选择SIMD优化** - 最大QPS提升
2. **元数据页缓存** - 简单且有效

这两个优化预计可以让你的QPS提升 **60-90%**，而且实现相对简单。

是否需要我帮你实现这些优化？
