# Pull Request: Native Temporal Entity API & Kafka Consumer Subsystem

## Adressierte Maintainer-Kritiken

### 1. Array-Format statt einzelnes Objekt pro Attribut
**Kritik:** Temporale Attribute wurden als einzelnes Objekt zurückgegeben statt als Array von Instanzen (ETSI GS CIM 009 §4.5.8).
**Fix:** `pgTemporalEntityBuild.cpp` liefert jetzt jedes Attribut als **Array** von Temporal-Instanzen mit `instanceId`, `observedAt`, und optionalem `datasetId`.

### 2. kaAlloc statt pgBufAlloc-Wrapper
**Kritik:** Überflüssiger `pgBufAlloc()`-Wrapper (kaAlloc + malloc Fallback) in den Append-Funktionen.
**Fix:** `pgBufAlloc()` komplett entfernt. Alle Aufrufe in `pgAttributeAppend.cpp`, `pgEntityAppend.cpp`, `pgSubAttributeAppend.cpp` nutzen jetzt direkt `kaAlloc(&orionldState.kalloc, ...)`.

### 3. timeproperty-Support in ORDER BY
**Kritik:** Sortierung war hartcodiert auf `ts` statt den `timeproperty`-Parameter zu berücksichtigen.
**Fix:** Alle 6 ORDER BY-Klauseln in `pgTemporalEntityQuery.cpp` verwenden jetzt `timeCol` (abgeleitet aus `timeproperty`). Neuer Test `troe_get_temporal_entity_sort_timeproperty.test` verifiziert das Verhalten.

### 4. ORIONLD_URIPARAM_LASTN fehlte
**Kritik:** `lastN` wurde zwar geparst, aber nie im URI-Param-Mask registriert — kein Schutz gegen unbekannte Parameter.
**Fix:** Bit 26 definiert als `ORIONLD_URIPARAM_LASTN`, Mask in `mhdConnectionInit.cpp` gesetzt, am temporal Endpoint in `orionldServiceInit.cpp` erlaubt.

### 5. Bounds-Checks und Comment-Block-Cleanup
**Kritik:** Fehlende Bounds-Checks und Code-Kommentar-Formatierung.
**Fix:** Bounds-Check für `attrFilter`-Buffer hinzugefügt, Kommentarblöcke bereinigt.

---

## Summary

### Native Temporal Entity Query (TRoE/PostgreSQL)
Vollständige Implementierung von `GET /temporal/entities/{entityId}` ohne Mintaka-Abhängigkeit:

- **`pgTemporalEntityQuery`** — Parameterized SQL mit `timerel=before/after/between`, `timeproperty`, `attrs`-Filter, `lastN` (Window-Funktion mit `ROW_NUMBER() OVER PARTITION BY`)
- **`pgTemporalEntityBuild`** — Baut NGSI-LD Temporal Entity Response aus 3 PG-Resultsets (Entity, Attributes, Sub-Attributes)
- **`orionldGetTemporalEntity`** — Service-Routine mit Validierung, Format-Transformationen (`temporalValues`, simplified/concise/normalized), `@context`-Compaction

### Kafka Consumer Subsystem
High-throughput Ingestion-Pipeline, die den HTTP-API-Pfad umgeht:

- **`kafkaInit`** — librdkafka Consumer-Setup, konfigurierbare Thread-Anzahl
- **`kafkaConsumerLoop`** — Polling + Micro-Batch-Akkumulation (size/linger-basiert)
- **`kafkaBatchProcess`** — 3-Runden-Validierung (gleiche Pipeline wie `orionldPostBatchUpsert`), MongoDB Upsert, TRoE-Write, Notification-Dispatch
- **`kafkaMessageParse`** — JSON-Parsing + Struktur-Validierung (Object/Array, id+type Check)
- **`kafkaRelease`** — Graceful Shutdown mit Thread-Join

### TRoE Append/Replace opMode
Korrekte Unterscheidung zwischen neuen Attributen (Append) und Überschreibungen (Replace) in `troePatchEntity.cpp`, `troePostEntity.cpp`, `troePatchEntity2.cpp`.

### Neue CLI-Optionen
| Option | Default | Beschreibung |
|--------|---------|-------------|
| `-kafka` | false | Kafka Consumer aktivieren |
| `-kafkaBrokerList` | localhost:9092 | Broker-Adressen |
| `-kafkaTopic` | orionld-entities | Consumer-Topic |
| `-kafkaGroupId` | orionld-consumer | Consumer Group ID |
| `-kafkaBatchSize` | 100 | Max Entities pro Micro-Batch |
| `-kafkaBatchLingerMs` | 50 | Max Wartezeit pro Batch (ms) |
| `-kafkaConsumerThreads` | 2 | Anzahl Consumer-Threads |

### Trace Levels
| Level | Name | Beschreibung |
|-------|------|-------------|
| 2100 | KtKafka | Consumer Init, Message Receipt, Batch Processing |
| 2101 | KtKafkaDetail | Payload-Inhalte |

---

## Geänderte Dateien

### Source (14 Dateien)
| Datei | Änderung |
|-------|----------|
| `src/app/orionld/orionld.cpp` | Kafka CLI-Optionen, Init/Release |
| `src/lib/orionld/types/OrionLdRestService.h` | `ORIONLD_URIPARAM_LASTN` (Bit 26) |
| `src/lib/orionld/common/orionldState.h` | `lastN` im uriParams-Struct |
| `src/lib/orionld/mhd/mhdConnectionInit.cpp` | `lastN` Parsing + Mask |
| `src/lib/orionld/mhd/mhdConnectionTreat.cpp` | `lastN` String-Mapping |
| `src/lib/orionld/service/orionldServiceInit.cpp` | URI-Params für temporal Endpoint |
| `src/lib/orionld/serviceRoutines/orionldGetTemporalEntity.cpp` | attrs, lastN, count, temporalValues, Format-Support |
| `src/lib/orionld/troe/pgTemporalEntityQuery.cpp` | SQL-Builder: timerel, timeproperty, attrs, lastN |
| `src/lib/orionld/troe/pgTemporalEntityQuery.h` | Erweiterte Signatur |
| `src/lib/orionld/troe/pgTemporalEntityBuild.cpp` | Array-Format, alle Typen, Sub-Attribute |
| `src/lib/orionld/troe/pgAttributeAppend.cpp` | kaAlloc statt pgBufAlloc |
| `src/lib/orionld/troe/pgEntityAppend.cpp` | kaAlloc statt pgBufAlloc |
| `src/lib/orionld/troe/pgSubAttributeAppend.cpp` | kaAlloc statt pgBufAlloc |
| `src/lib/orionld/apiModel/ntonAttribute.cpp` | instanceId in Skip-Liste |
| `src/lib/orionld/context/orionldEntityCompact.cpp` | Array-Attribut-Compaction |
| `doc/temporal/basic-architecture.md` | Architektur-Dokumentation |

### Tests (5 Dateien)

#### Neue Tests
| Test | Beschreibung |
|------|-------------|
| `troe_get_temporal_entity.test` | Erweitert: attrs-Filter, lastN, count-Header, temporalValues-Format |
| `troe_get_temporal_entity_sort_timeproperty.test` | **Neu:** Sortierung nach observedAt vs modifiedAt |

#### Aktualisierte Tests
| Test | Änderung |
|------|----------|
| `troe_get_temporal_entity_all_types.test` | Array-Format-Erwartungen für alle NGSI-LD Typen |
| `troe_get_temporal_entity_between.test` | Array-Format-Erwartungen für between-Queries |
| `troe_get_temporal_entity_errors.test` | Aktualisierte Fehlermeldungen |

---

## Verifikation

1. `make` — fehlerfrei kompilieren
2. Alle temporalen Tests:
   - `troe_get_temporal_entity.test`
   - `troe_get_temporal_entity_all_types.test`
   - `troe_get_temporal_entity_between.test`
   - `troe_get_temporal_entity_errors.test`
   - `troe_get_temporal_entity_no_troe.test`
   - `troe_get_temporal_entity_sort_timeproperty.test`
3. Kafka-Test (benötigt laufenden Kafka-Broker):
   - `troe_kafka_consumer_ingestion.test`
