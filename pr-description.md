# Native Temporal Entity API & Kafka Consumer Subsystem

## Addressed Maintainer Feedback

### 1. Array format for temporal attributes
**Issue:** Temporal attributes were returned as a single object instead of an array of instances per ETSI GS CIM 009 section 4.5.8.
**Fix:** `pgTemporalEntityBuild` now returns each attribute as an **array** of temporal instances with `instanceId`, `observedAt`, and optional `datasetId`.

### 2. Remove pgBufAlloc wrapper — use kaAlloc directly
**Issue:** Unnecessary `pgBufAlloc()` wrapper (kaAlloc + malloc fallback) in the append functions.
**Fix:** `pgBufAlloc()` removed entirely. All calls in `pgAttributeAppend`, `pgEntityAppend`, and `pgSubAttributeAppend` now use `kaAlloc(&orionldState.kalloc, ...)` directly.

### 3. timeproperty support in ORDER BY
**Issue:** Sort order was hardcoded to `ts` instead of respecting the `timeproperty` parameter.
**Fix:** All 6 ORDER BY clauses in `pgTemporalEntityQuery` now use `timeCol` (derived from `timeproperty`). New test `troe_get_temporal_entity_sort_timeproperty` verifies the behavior.

### 4. Missing ORIONLD_URIPARAM_LASTN registration
**Issue:** `lastN` was parsed but never registered in the URI param mask — no protection against unknown parameters.
**Fix:** Bit 26 defined as `ORIONLD_URIPARAM_LASTN`, mask set in `mhdConnectionInit`, allowed on the temporal endpoint in `orionldServiceInit`.

### 5. Bounds checks and comment block cleanup
**Issue:** Missing bounds checks and inconsistent code comment formatting.
**Fix:** Bounds check added for `attrFilter` buffer, comment blocks cleaned up.

---

## Summary

### Native Temporal Entity Query (TRoE/PostgreSQL)
Full implementation of `GET /temporal/entities/{entityId}` without Mintaka dependency:

- **`pgTemporalEntityQuery`** — Parameterized SQL supporting `timerel=before/after/between`, `timeproperty`, `attrs` filter, `lastN` (window function with `ROW_NUMBER() OVER PARTITION BY`)
- **`pgTemporalEntityBuild`** — Constructs NGSI-LD Temporal Entity response from 3 PG result sets (entity, attributes, sub-attributes)
- **`orionldGetTemporalEntity`** — Service routine with validation, format transformations (`temporalValues`, simplified/concise/normalized), `@context` compaction

### Kafka Consumer Subsystem
High-throughput ingestion pipeline bypassing the HTTP API:

- **`kafkaInit`** — librdkafka consumer setup with configurable thread count
- **`kafkaConsumerLoop`** — Polling + micro-batch accumulation (size/linger-based)
- **`kafkaBatchProcess`** — 3-round validation (same pipeline as `orionldPostBatchUpsert`), MongoDB upsert, TRoE write, notification dispatch
- **`kafkaMessageParse`** — JSON parsing + structural validation (object/array, id+type check)
- **`kafkaRelease`** — Graceful shutdown with thread join

### TRoE Append/Replace opMode Tracking
Correct distinction between new attributes (Append) and overwrites (Replace) across `troePatchEntity`, `troePostEntity`, and `troePatchEntity2`.

### New CLI Options
| Option | Default | Description |
|--------|---------|-------------|
| `-kafka` | false | Enable Kafka consumer |
| `-kafkaBrokerList` | localhost:9092 | Broker addresses |
| `-kafkaTopic` | orionld-entities | Consumer topic |
| `-kafkaGroupId` | orionld-consumer | Consumer group ID |
| `-kafkaBatchSize` | 100 | Max entities per micro-batch |
| `-kafkaBatchLingerMs` | 50 | Max batch wait time (ms) |
| `-kafkaConsumerThreads` | 2 | Number of consumer threads |

### Trace Levels
| Level | Name | Description |
|-------|------|-------------|
| 2100 | KtKafka | Consumer init, message receipt, batch processing |
| 2101 | KtKafkaDetail | Payload contents |

---

## New & Updated Tests

### New Tests
| Test | Coverage |
|------|----------|
| `troe_get_temporal_entity` | Extended: attrs filter, lastN, count header, temporalValues format |
| `troe_get_temporal_entity_sort_timeproperty` | Sorting by observedAt vs modifiedAt |

### Updated Tests
| Test | Change |
|------|--------|
| `troe_get_temporal_entity_all_types` | Array format expectations for all NGSI-LD types |
| `troe_get_temporal_entity_between` | Array format expectations for between queries |
| `troe_get_temporal_entity_errors` | Updated error messages |
