# SNII 与 V3 Clucene 倒排索引读性能测试计划

## 0. 推荐部署模式

本次 benchmark 建议直接使用单机 `cloud mode`，不建议继续在当前 `local mode` 集群上做主测试。

原因很直接：

- 本次要对比的是“远端对象存储上的倒排索引读 + file cache 回写/命中 + searcher cache 命中”
- `cloud mode` 原生具备 `storage vault`、remote read、file cache、meta service 这一整套路径
- 当前 `local mode` 环境里，`SHOW STORAGE VAULT` 不可用，且 file cache 相关开关默认未打开，不适合作为 `pure cold / writeback cold / warm / hot` 四态 benchmark 基线

因此，后续执行建议固定为：

- 使用当前 worktree 编译产物下的 `output/ms`、`output/fe`、`output/be`
- 以 `regression-test/pipeline/cloud_p0/` 的单机 cloud 配置为参考
- FE/BE 端口、网络、日志目录等，可吸收 `~/doris_conf/fe.conf` 与 `~/doris_conf/be1.conf` 里的现有配置
- 文件缓存、对象存储、warehouse、storage vault 等 cloud 专属配置，以 cloud mode 要求为准

## 1. 目标

目标是在同一套 Doris 环境、同一批数据、同一套表结构和索引定义下，对比 `SNII` 与 `V3` Clucene 倒排索引的读性能差异，重点覆盖以下 4 种读状态：

1. 纯冷读
2. 回写 file cache 冷读
3. warm file cache 读
4. hot file cache 读

本计划以 `~/datasets/agent_trace_xm/w1.0001.jsonl` 为示例数据源，以 `events_full` 风格表为基底，输出一套可复现的 benchmark 流程、查询集和观测指标。

## 2. 可比边界

- 主结论只比较字符串、文本、`ARRAY<STRING>` 上的倒排读性能。
- `SNII` 当前不支持 BKD / range query，因此数值列和时间列的范围查询不能纳入 `SNII vs V3` 主结论。
- 范围查询可以单独列为 `V3-only` 兼容性/能力项，单独展示。
- `SNII` 当前不适合走“先建表后 BUILD INDEX”的流程，benchmark 表必须在 `CREATE TABLE` 时一次性声明全部倒排索引，然后再导入数据。

## 3. 测试对象与建表策略

## 3.1 数据集

- 数据源：`~/datasets/agent_trace_xm/w1.0001.jsonl`
- 导入原则：
  - `SNII` 和 `V3` 两张表导入完全相同的数据文件
  - 使用相同的 batch 切分、相同的导入并发、相同的副本数
  - 导入期间关闭自动 compaction，避免 rowset/segment 形态漂移
  - 导入完成后执行 `SYNC`

建议在导入前后确认两张表的以下信息一致：

- 分桶数
- 分区数
- tablet 数
- rowset/segment 数量
- 总行数

如果这些条件不一致，读性能结果很容易被 segment layout 污染。

## 3.2 对比表

建议基于 `events_full` 风格 DDL 创建两张 benchmark 表：

- `events_full_bench_v3`
- `events_full_bench_snii`

两张表必须满足：

- 列定义完全一致
- 分区与分桶完全一致
- 倒排索引列完全一致
- 倒排索引 `parser` / `analyzer` 配置完全一致
- 唯一差异仅为表属性 `"inverted_index_storage_format"`

建议流程不是 `CREATE TABLE LIKE` 后再补索引，而是直接用同一份 DDL 生成两张表，仅替换最后的 format：

```sql
PROPERTIES (
    ...,
    "disable_auto_compaction" = "true",
    "inverted_index_storage_format" = "V3"
);
```

```sql
PROPERTIES (
    ...,
    "disable_auto_compaction" = "true",
    "inverted_index_storage_format" = "SNII"
);
```

## 3.3 建议倒排索引字段

建议把字段分成 3 类。

### A. 精确过滤字段

这些字段用于点查和多条件过滤，索引建议使用非分词倒排，即 `parser="none"`：

- `project_id`
- `trace_id`
- `span_id`
- `parent_span_id`
- `user_id`
- `session_id`
- `type`
- `environment`
- `prompt_name`
- `provided_model_name`
- `source`

建议索引形式：

```sql
INDEX idx_trace_id(trace_id) USING INVERTED PROPERTIES("parser" = "none")
```

### B. 文本检索字段

这些字段用于 `MATCH_ANY`、`MATCH_ALL`、`MATCH_PHRASE`、`MATCH_PHRASE_PREFIX`、`MATCH_REGEXP`，索引建议使用分词 analyzer，例如 `parser="standard"`：

- `name`
- `trace_name`
- `status_message`

建议索引形式：

```sql
INDEX idx_name(name) USING INVERTED PROPERTIES("parser" = "standard")
```

### C. 标签数组字段

- `tags`

如果 `tags` 是 `ARRAY<STRING>`，建议保留倒排索引，并通过 `array_contains(tags, '...')` 作为数组命中场景。

## 3.4 暂不纳入主对比的字段

以下字段不建议放入第一轮主结论：

- 数值列、时间列上的倒排范围查询
- `VARIANT` / JSON 大字段直接做全文检索

如果需要覆盖 `input` / `output` / `metadata` 一类大文本，建议在第二阶段先抽取成物化字符串列，例如：

- `input_text`
- `output_text`

然后再对这些字符串列建全文倒排索引，否则 tokenization、JSON 路径提取和大字段裁剪会把索引格式差异与数据清洗差异混在一起。

## 4. 查询 workload

## 4.1 字面量选择原则

不要只测一条 SQL。每一类查询都要准备固定的字面量集合，并冻结下来，保证 `SNII` 与 `V3` 使用完全相同的 predicate。

建议按选择率准备 3 档字面量：

- 高选择率：命中 1 到 10 行
- 中选择率：命中 100 到 1000 行
- 低选择率：命中 0.1% 到 5% 数据

建议先在任意一张基准表上抽样得到固定字面量，再把这组值复用于另一张表。

## 4.2 主对比 SQL

下列 SQL 都应在 `events_full_bench_v3` 和 `events_full_bench_snii` 上各跑一套。

### Q1. span_id 点查

```sql
SELECT * 
FROM events_full_bench_${fmt}
WHERE span_id = '${span_id}'
LIMIT 1;
```

### Q2. trace_id 精确过滤

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE trace_id = '${trace_id}';
```

### Q3. 多条件精确过滤

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE project_id = '${project_id}'
  AND environment = '${environment}'
  AND session_id = '${session_id}';
```

### Q4. tags 数组命中

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE array_contains(tags, '${tag}');
```

### Q5. 普通全文匹配

`Doris` 里建议统一用 `MATCH_ANY` 作为普通 match 基线。

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE name MATCH_ANY '${token}';
```

### Q6. 多词全文匹配

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE status_message MATCH_ALL '${terms}';
```

### Q7. 短语匹配

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE trace_name MATCH_PHRASE '${phrase}';
```

### Q8. 前缀短语匹配

用户提到的“match prefix”在 Doris 中建议映射为 `MATCH_PHRASE_PREFIX`。

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE name MATCH_PHRASE_PREFIX '${phrase_prefix}';
```

### Q9. 正则匹配

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE status_message MATCH_REGEXP '${regexp}';
```

### Q10. 组合过滤

```sql
SELECT count(*)
FROM events_full_bench_${fmt}
WHERE environment = '${environment}'
  AND name MATCH_ANY '${token}';
```

## 4.3 V3-only 范围查询

这部分不计入 `SNII vs V3` 主结论，但建议单独保留结果，说明 `V3` 的能力边界。

示例：

```sql
SELECT count(*)
FROM events_full_bench_v3
WHERE start_time >= '${ts_begin}'
  AND start_time < '${ts_end}';
```

```sql
SELECT count(*)
FROM events_full_bench_v3
WHERE duration_ms >= ${lower}
  AND duration_ms < ${upper};
```

## 5. 四种 cache 状态的定义与执行流程

## 5.1 通用 session 设置

每次 benchmark 前建议固定以下变量：

```sql
SET enable_profile = true;
SET profile_level = 2;
SET enable_sql_cache = false;
SET enable_inverted_index_query_cache = false;
SET enable_segment_limit_pushdown = true;
SET enable_fallback_on_missing_inverted_index = true;
```

如果要确保 `MATCH_*` 查询没有回退到无索引路径，可额外设置：

```sql
SET enable_match_without_inverted_index = false;
```

## 5.2 file cache 清理

每次进入冷态前，先清空所有 BE 的 file cache：

```bash
curl "http://<be_http_host>:<be_http_port>/api/file_cache?op=clear&sync=true"
```

建议对所有 BE 执行，并等待几秒后再发起查询。

## 5.3 状态定义

| 状态 | file cache | searcher cache | 执行方式 | 预期 profile 特征 |
| --- | --- | --- | --- | --- |
| 纯冷读 | 关闭/旁路 | 关闭 | 清 file cache 后首读 | `InvertedIndexBytesScannedFromRemote > 0`，`InvertedIndexBytesWriteIntoCache = 0` |
| 回写 file cache 冷读 | 开启但为空 | 关闭 | 清 file cache 后首读 | 远端读大于 0，同时 `InvertedIndexBytesWriteIntoCache > 0` |
| warm file cache 读 | 开启且已命中 | 关闭 | 在上一轮回写冷读后直接复跑同一 SQL | `InvertedIndexBytesScannedFromCache > 0`，`InvertedIndexSearcherCacheHit = 0` |
| hot file cache 读 | 开启且已命中 | 开启且已预热 | file cache warm 后，先跑一轮预热 searcher，再测第二轮 | `InvertedIndexSearcherCacheHit > 0` |

## 5.4 每种状态对应设置

### 纯冷读

```sql
SET enable_file_cache = false;
SET disable_file_cache = true;
SET enable_inverted_index_searcher_cache = false;
```

流程：

1. 清 file cache
2. 跑目标 SQL
3. 采集 profile 与 audit

### 回写 file cache 冷读

```sql
SET enable_file_cache = true;
SET disable_file_cache = false;
SET enable_inverted_index_searcher_cache = false;
```

流程：

1. 清 file cache
2. 跑目标 SQL
3. 采集 profile 与 audit
4. 验证 `InvertedIndexBytesWriteIntoCache > 0`

### warm file cache 读

```sql
SET enable_file_cache = true;
SET disable_file_cache = false;
SET enable_inverted_index_searcher_cache = false;
```

流程：

1. 先完成“回写 file cache 冷读”
2. 不清 file cache，直接再次运行同一条 SQL
3. 采集 profile 与 audit
4. 验证 `InvertedIndexBytesScannedFromCache > 0`

### hot file cache 读

```sql
SET enable_file_cache = true;
SET disable_file_cache = false;
SET enable_inverted_index_searcher_cache = true;
```

流程：

1. 确保 file cache 已 warm
2. 先跑一轮相同 SQL，填充 searcher cache
3. 第二轮作为正式测量
4. 验证 `InvertedIndexSearcherCacheHit > 0`

这里的 `hot` 定义为“磁盘 file cache 已热，且 inverted index searcher 内存态也已热”。如果 profile 中没有出现 `InvertedIndexSearcherCacheHit > 0`，该轮结果应作废。

## 5.5 重复次数

建议最少重复次数：

- 纯冷读：每条 SQL 每种格式至少 5 轮，每轮都清 file cache
- 回写 file cache 冷读：每条 SQL 每种格式至少 5 轮，每轮都清 file cache
- warm file cache：每条 SQL 每种格式至少 10 轮
- hot file cache：每条 SQL 每种格式至少 10 轮

建议在轮次间交替执行顺序，避免总是先跑 `SNII` 再跑 `V3`：

- 第 1 轮：`SNII -> V3`
- 第 2 轮：`V3 -> SNII`

## 6. 观测指标

主观测来源分三层：

1. 单查询级：`query profile`
2. 单查询级：`__internal_schema.audit_log`
3. 集群级补充：BE `/metrics` 或 `/brpc_metrics`

最终结论应以 1 和 2 为主。

## 6.1 audit log

建议按 `query_id` 回查：

```sql
SELECT
    query_id,
    time,
    state,
    query_time,
    cpu_time_ms,
    peak_memory_bytes,
    scan_bytes,
    scan_rows,
    return_rows,
    scan_bytes_from_local_storage,
    scan_bytes_from_remote_storage,
    inverted_index_bytes_from_remote_storage,
    segment_footer_index_bytes_from_remote_storage
FROM __internal_schema.audit_log
WHERE query_id = '${query_id}';
```

重点字段：

- `query_time`
- `cpu_time_ms`
- `peak_memory_bytes`
- `scan_bytes`
- `scan_rows`
- `return_rows`
- `scan_bytes_from_local_storage`
- `scan_bytes_from_remote_storage`
- `inverted_index_bytes_from_remote_storage`
- `segment_footer_index_bytes_from_remote_storage`

## 6.2 query profile

执行查询后获取 `query_id`：

```sql
SELECT last_query_id();
```

再通过 FE HTTP API 拉取文本版 profile：

```bash
curl "http://<fe_http_host>:<fe_http_port>/rest/v1/query_profile/text/<query_id>"
```

建议重点采集以下 counter：

| 逻辑指标 | profile 中的实际名字 |
| --- | --- |
| 倒排过滤行数 | `RowsInvertedIndexFiltered` |
| 倒排总查询时间 | `InvertedIndexQueryTime` |
| 倒排 null bitmap 时间 | `InvertedIndexQueryNullBitmapTime` |
| 倒排 bitmap copy 时间 | `InvertedIndexQueryBitmapCopyTime` |
| searcher 打开时间 | `InvertedIndexSearcherOpenTime` |
| searcher 查询时间 | `InvertedIndexSearcherSearchTime` |
| searcher 初始化时间 | `InvertedIndexSearcherSearchInitTime` |
| searcher 执行时间 | `InvertedIndexSearcherSearchExecTime` |
| analyzer 时间 | `InvertedIndexAnalyzerTime` |
| lookup 时间 | `InvertedIndexLookupTimer` |
| searcher cache hit | `InvertedIndexSearcherCacheHit` |
| searcher cache miss | `InvertedIndexSearcherCacheMiss` |
| query cache hit | `InvertedIndexQueryCacheHit` |
| query cache miss | `InvertedIndexQueryCacheMiss` |
| 远端读取的倒排字节 | `InvertedIndexBytesScannedFromRemote` |
| 从 cache 读取的倒排字节 | `InvertedIndexBytesScannedFromCache` |
| 倒排远端物理读字节 | `InvertedIndexRemotePhysicalReadBytes` |
| 倒排写入 cache 的字节 | `InvertedIndexBytesWriteIntoCache` |
| 倒排 range read 次数 | `InvertedIndexRangeReadCount` |
| 倒排 serial read 轮数 | `InvertedIndexSerialReadRounds` |
| footer 远端 IO 次数 | `SegmentFooterIndexNumRemoteIOTotal` |
| footer 远端读取字节 | `SegmentFooterIndexBytesScannedFromRemote` |
| footer 远端 IO 时间 | `SegmentFooterIndexRemoteIOUseTimer` |

用户给出的统计名与 Doris 现有字段并不完全同名，建议按下面映射采集：

| 用户关心项 | 建议采集名 |
| --- | --- |
| `segment_footer_index_bytes_read_from_remote` | `segment_footer_index_bytes_from_remote_storage` 或 `SegmentFooterIndexBytesScannedFromRemote` |
| `segment_footer_index_remote_io_timer` | `SegmentFooterIndexRemoteIOUseTimer` |
| `segment_footer_index_num_remote_io_total` | `SegmentFooterIndexNumRemoteIOTotal` |
| `inverted_index_remote_physical_read_bytes` | `InvertedIndexRemotePhysicalReadBytes` |
| `inverted_index_bytes_write_into_cache` | `InvertedIndexBytesWriteIntoCache` |
| `inverted_index_range_read_count` | `InvertedIndexRangeReadCount` |
| `inverted_index_serial_read_rounds` | `InvertedIndexSerialReadRounds` |

## 6.3 BE 全局 metrics

BE `/metrics` 或 `/brpc_metrics` 可以作为补充趋势数据，但不是单查询统计。

建议额外记录：

- `num_io_bytes_read_from_cache`
- `num_io_bytes_read_from_remote`
- `num_io_bytes_read_from_peer`
- `num_io_bytes_read_total`
- `doris_inverted_index_reader_mem_bytes`
- `doris_inverted_index_reader_num`

用途：

- 验证整轮 benchmark 期间 cache 是否稳定增长
- 观察 `hot` 阶段是否带来 searcher 内存常驻

## 7. 倒排索引文件大小与 meta/dict/posting 观测

## 7.1 V3 当前可直接观测的数据

先找到 tablet 与目标 BE：

```sql
SHOW TABLETS FROM events_full_bench_v3;
SHOW BACKENDS;
```

再调用 BE HTTP API：

```bash
curl "http://<be_http_host>:<be_http_port>/api/show_nested_index_file?tablet_id=<tablet_id>"
```

对于 `V3`，可以拿到 nested index file 列表与大小。建议至少归类统计：

- term dict 相关文件大小
- posting 相关文件大小
- norms / 其他辅助文件大小
- 单 segment 的总 index size

实际 suffix 以接口返回的 nested file 名字为准。通常可以按“dict 相关文件”和“posting 相关文件”两大类聚合，不必在第一版报告里细拆到所有 Lucene 后缀。

## 7.2 SNII 当前可直接观测的数据

`SNII` 当前在线接口更适合观察整个 `.idx` 文件大小，而不是内部 section 大小。也就是说，现阶段可以直接拿到：

- 单 segment `.idx` 总大小

但不能直接在线拿到：

- `per_index_meta` 字节数
- `dict_region` 字节数
- `posting_region` 字节数
- `norms` 字节数
- `null_bitmap` 字节数
- `bsbf` 字节数

## 7.3 建议的补充手段

如果本次 benchmark 需要把 `meta / dict / posting` 尺寸也纳入结果，建议单独做一层离线解析：

- 对 `V3`：基于 `/api/show_nested_index_file` 的 nested file 名字与大小做归类聚合
- 对 `SNII`：离线调用 SNII reader 解析 `SectionRefs`，提取：
  - `dict_region`
  - `posting_region`
  - `norms`
  - `null_bitmap`
  - `bsbf`

需要注意：

- 这些尺寸更接近“文件结构占比”而不是“单查询真实读取字节”
- 当前 Doris profile 并没有现成 counter 直接给出“本次查询实际读取了多少 meta/dict/posting 字节”

因此，报告中应把“文件结构尺寸”和“查询时实际 remote/cache 读字节”明确分开。

## 8. 结果表建议

建议最终按如下粒度出表：

| query_family | predicate | selectivity | state | format | p50_ms | p90_ms | cpu_ms | rows_filtered | remote_idx_bytes | cache_idx_bytes | footer_remote_bytes | searcher_cache_hit | searcher_cache_miss |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

建议至少比较：

- 延迟：`p50` / `p90`
- CPU：`cpu_time_ms`
- 倒排过滤效率：`RowsInvertedIndexFiltered`
- 远端索引读取：`InvertedIndexBytesScannedFromRemote`
- cache 读取：`InvertedIndexBytesScannedFromCache`
- footer 额外 remote 成本：`SegmentFooterIndexBytesScannedFromRemote`
- searcher cache 命中情况：`InvertedIndexSearcherCacheHit/Miss`

## 9. 执行前检查清单

- 两张表的 DDL 只有 `inverted_index_storage_format` 不同
- 所有倒排索引都在建表时声明完成
- 两张表导入相同数据，且 segment layout 尽量一致
- `enable_sql_cache = false`
- `enable_inverted_index_query_cache = false`
- `pure cold` 阶段显式旁路 file cache
- `warm` 阶段确认 `InvertedIndexBytesScannedFromCache > 0`
- `hot` 阶段确认 `InvertedIndexSearcherCacheHit > 0`
- `V3-only` 范围查询不混入主结论

## 10. 预期产出

本 benchmark 完成后，至少应输出以下 3 类结论：

1. 在相同字符串/全文检索 workload 下，`SNII` 与 `V3` 在 4 种 cache 状态下的延迟和 IO 差异
2. 这些差异主要来自哪里：footer remote IO、倒排 remote read、file cache 命中，还是 searcher cache 命中
3. `V3` 独有的 range query 能力及其单独性能表现

如果要继续深挖结构原因，再追加一轮“索引文件内部尺寸分析”，把 `V3` 的 nested files 与 `SNII` 的 `dict/posting/meta` section 尺寸并列展示。
