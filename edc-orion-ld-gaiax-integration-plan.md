# Plan: Native Anbindung des Eclipse Dataspace Connector (EDC) an Orion‑LD für GAIA‑X‑Kompatibilität

> **Status:** Architektur-/Konzeptentwurf · **Stand:** 2026-06-08
> **Scope:** Wie lässt sich ein EDC-Connector als souveräne Vertrags- und Identitätsschicht *nativ* vor den Orion‑LD NGSI‑LD Context Broker setzen, sodass das Ergebnis GAIA‑X‑konform ist.
>
> Die mit „🔴/🟠" und „Korrektur" markierten Punkte sind Befunde aus einer Quellcode-Prüfung des Orion‑LD-Repositorys und korrigieren mehrere naheliegende, aber falsche Annahmen. Versions- und GAIA‑X-Release-Angaben sind zum jeweiligen Ziel-Release zu verifizieren (siehe Abschnitt 6).

---

## 1. Kurzfazit & Empfehlung

**Zielbild:** Ein EDC-Connector setzt sich als souveräne Vertrags- und Identitätsschicht **vor** Orion‑LD. Orion‑LD bleibt eine *reine, credential-agnostische Data Plane* (genau das Muster, das auch FIWARE im eigenen „Data Space Connector" fährt). Die gesamte GAIA‑X-/Trust-Logik liegt im EDC-Control-Plane + IdentityHub; die Datenpfade laufen erst **nach** einem `FINALIZED`-Vertrag.

**Hybrider Ansatz mit zwei Datenpfaden, einer Identität:**

- **Pull/Temporal** → native EDC-Data-Plane-Extension `:data-plane-ngsi-ld` (DataAddress-Typ `NgsiLd`). Ein EDC-*Asset* = eine policy-begrenzte NGSI‑LD-*Query / Entity-Set* statt eines opaken Byte-Blobs.
- **Push/Live** → Subscription-Bridge `:data-plane-ngsi-ld-subscription`, die den EDC-Transfer-State-Machine 1:1 auf den Orion‑LD-Subscription-Lebenszyklus abbildet.
- **Bootstrap & Fallback** → der **Standard-`data-plane-http`** von EDC als reiner Proxy. Damit ist in **1–3 Tagen** ein erster DSP+DCP-End-to-End-Durchstich machbar, ganz ohne Java.

**Wichtigste strategische Warnung vorab:** Bevor irgendetwas gebaut wird, muss eine **Vorentscheidung** fallen (Abschnitt 2). FIWARE liefert mit dem **FDSC‑EDC** bereits eine EDC-DSP-↔-NGSI‑LD-Kopplung. Ein kompletter Eigenbau (~16–30 Personenwochen) riskiert, das zu duplizieren.

---

## 2. Strategische Vorentscheidung (Phase 0 — *blockierend*)

Es gibt **drei** Wege zum Ziel „NGSI‑LD + GAIA‑X". Die Wahl ändert die gesamte Roadmap:

| Option | Was es ist | Aufwand | Wann sinnvoll |
|---|---|---|---|
| **A — FIWARE Data Space Connector (DSC) nativ** | FIWARE-eigener Stack (Keycloak/VCVerifier, APISIX-PEP, OPA-PDP, TMForum-APIs, Trusted-Issuers-List). Orion‑LD/Scorpio steckt schon als Data Plane dahinter; enthält **FDSC‑EDC** für DSP. | Niedrig (Konfiguration/Helm) | Wenn das Ziel „GAIA‑X/DSBA-konform" lautet und nicht zwingend „Eclipse EDC". Schnellster Weg. |
| **B — FDSC‑EDC erweitern** | Die bereits existierende EDC-↔-NGSI‑LD-Brücke im DSC adoptieren/erweitern statt neu bauen. | Mittel | Wenn Eclipse-EDC gefordert ist, man aber Wartungslast teilen will. |
| **C — Eigenbau: Eclipse EDC vor Orion‑LD** (dieser Plan) | Native `:data-plane-ngsi-ld`-Extensions, volle Kontrolle, NGSI‑LD-semantische ODRL-Policies. | Hoch | Wenn man maximale Kontrolle/Nativität braucht *und* der Ecosystem-Pfad (z. B. Catena‑X/Tractus‑X) Eclipse-EDC erzwingt. |

> ⚠️ **Kritischer Befund:** „Eclipse EDC vor Orion‑LD" als *eigenständige, ausgereifte* Brücke existiert öffentlich **nicht** — die einzige reale EDC-↔-NGSI‑LD-Kopplung ist FDSC‑EDC im FIWARE-DSC. Hinweis am Rande: **Catena‑X/Tractus‑X nutzt kein NGSI‑LD** (dort: AAS-Submodels/SAMM), d. h. „GAIA‑X über EDC" ≠ „NGSI‑LD über EDC". Deshalb ist die erste Aufgabe ein **zeitlich begrenzter FDSC‑EDC-Spike**: Kann FDSC‑EDC Pull + Temporal + Subscription, Orion‑LD (statt Scorpio-Default), DCP+GXDCH und das benötigte ODRL-Modell? Erst danach das Eigenbau-Commitment.

Der Rest dieses Plans beschreibt **Option C** (Eigenbau) im Detail — als Referenz-Zielbild und als das, was nach einem negativen FDSC-Spike zu bauen wäre. Der HttpData-PoC (Phase 1) entwertet diese Entscheidung nicht und ist in allen Optionen nützlich.

---

## 3. Zielarchitektur (Überblick)

```
        ┌──────────────────────── PROVIDER ────────────────────────┐
Consumer│  EDC Control Plane (DSP/Catalog/Negotiation/Transfer)    │
EDC ───►│   + PolicyEngine + Management API v3                     │
 (DSP)  │  IdentityHub (DCP, did:web, VC-Wallet, STS, GXDCH-VCs)   │
        │            │  signal START/SUSPEND/RESUME/TERMINATE       │
        │            ▼                                              │
        │  ┌─ Data Plane: :data-plane-ngsi-ld   (PULL/Temporal) ─┐ │
        │  ├─ Data Plane: :data-plane-ngsi-ld-subscription (PUSH)┤ │── X-Auth-Token (Vault)
        │  │   gemeinsam: OrionLdClient · NgsiLdCatalog(DCAT)    │ │── NGSILD-Tenant (immutabel)
        │  └──────────────────────┬───────────────────────────────┘ │
        └─────────── NUR hier erreichbar (Netz-Isolation) ──────────┘
                                  ▼
                       Orion‑LD  (NGSI‑LD REST · MongoDB · PostgreSQL/TRoE)
                       = reine Data Plane, KEIN Code-Change für Pull/C1
```

**Wiederverwendet (unverändert):** Control Plane (`control-plane-core/-api`, Management API v3), DSP-Module (`dsp-catalog/negotiation/transfer-http`, EDR), Data-Plane-Framework (`PipelineService`, Signaling API, `DataPlaneSelector`), `data-plane-http`, **IdentityHub + DCP + STS**, HashiCorp Vault, Produktiv-Basis (Tractus‑X-EDC- oder Sovity-CE-Images, PostgreSQL).

**Neu zu bauen:** die zwei NGSI‑LD-Data-Planes, ein gemeinsamer `OrionLdClient` (zentrale, nicht-fälschbare Header-Injektion), `NgsiLdParamsDecorator` (serverseitige Scope-Erzwingung), NGSI‑LD-ODRL-Constraint-Funktionen, der konsumentenseitige `NgsiLdSubscriptionIngress`, ein `NgsiLdCatalog`/DCAT-Mapper, sowie ein **GAIA‑X-VC-Verifier** (s. Abschnitt 6) und die Security-Guards (Allowlists/Caps).

---

## 4. Die zwei Datenpfade

### 4a. Pull / Temporal (native, Approach A)

1. Consumer ruft Katalog ab (`POST /management/v3/catalog/request`) → DCAT-Datasets, vom `NgsiLdCatalog`-Mapper mit `entityType`, `attrs`, `@context`, Geo/Zeit-Coverage befüllt.
2. DSP-Contract-Negotiation; `PolicyEngine` prüft `ngsild:*`-Access-Constraints; Identität via DCP. → `FINALIZED` + `contractAgreementId`.
3. `POST /management/v3/transferprocesses` (`NgsiLd-PULL`) → Provider-Data-Plane liefert eine **EDR** `{endpoint, token}`, die auf die **EDC-Public-URL** zeigt (nicht auf Orion).
4. Consumer GET auf EDR-Endpoint → `NgsiLdParamsDecorator` prüft Sub-Selektoren gegen den Vertrags-Scope → `OrionLdClient` ruft `GET /ngsi-ld/v1/entities` bzw. `POST /ngsi-ld/v1/entityOperations/query` mit injiziertem `NGSILD-Tenant` + `X-Auth-Token` (Vault) + `Ngsild-Attribute-Format`.
5. Quelle folgt der **RFC-5988-`Link rel=next`-Paginierung** (Orion emittiert diese selbst) und streamt JSON-LD zurück, inkl. korrektem `json-ld#context`-Link-Header.
6. **Temporal:** identisch, Ziel `GET /ngsi-ld/v1/temporal/entities` (braucht TRoE/PostgreSQL), Zeitfenster per `ngsild:temporalWindow` **fail-closed** begrenzt.

### 4b. Push / Live-Streaming (Subscription-Bridge, Approach C1)

- `START` → Bridge baut den Subscription-Body (`entities`, `watchedAttributes`, `q`, `geoQ`) **vollständig aus dem Vertrag** und `POST /ngsi-ld/v1/subscriptions` mit `notification.endpoint.uri` = Consumer-Ingress (Allowlist!) und dem EDR/STS-Token in `notification.endpoint.receiverInfo`. `subId ↔ transferProcessId` wird persistiert.
- **Steady State:** Orion pusht bei jeder Änderung `{subscriptionId, notifiedAt, data[]}` an den Ingress. Der `NgsiLdSubscriptionIngress` validiert den Token gegen das *lebende* Agreement **fail-closed**.
- `SUSPEND/RESUME` → `PATCH {"status":"paused"|"active"}` (**Standard-Feld**, nicht das Orion-proprietäre `isActive`), `TERMINATE`/Revoke/Expiry → `DELETE`.

### 4c. Bootstrap & Fallback (Approach B)

Stock-`data-plane-http` proxyt eine fix verdrahtete NGSI‑LD-Query als `HttpData`-Asset. Ein winziger `HttpParamsDecorator` stempelt den immutablen `NGSILD-Tenant`. → Tag-1-Durchstich und Dauer-Fallback für „ein Angebot = ein Datensatz".

---

## 5. NGSI‑LD ↔ EDC Mapping

| NGSI‑LD | EDC-Konzept | Anmerkung |
|---|---|---|
| Entity-Type + gespeicherte Query (`q`/`geoQ`/`attrs`) | **Asset** (`DataAddress.type = NgsiLd`) | 1 Asset = 1 policy-begrenztes Entity-Set |
| `@context` + Type + Attr-Liste + Coverage | `Asset.properties` → DCAT-Metadaten | speist die GAIA‑X **Data-Resource-VC** |
| Attr/Geo/Zeit-Scoping | ODRL-Constraints `ngsild:entityType/attrs/geoBoundary/temporalWindow` | Control-Plane **und** `NgsiLdParamsDecorator` (s. u.) |
| `GET /entities`, `POST /entityOperations/query` | `NgsiLdDataSource` (`NgsiLd-PULL`) | `rel=next`-Paging in der Source |
| `GET /temporal/entities` | `NgsiLdDataSource` Temporal-Mode | braucht TRoE; `ngsild:temporalWindow` **pflichtig** |
| `POST /entityOperations/upsert` | `NgsiLdDataSink` | **207 partial-success** (s. Korrektur unten) |
| Subscription `POST/PATCH/DELETE` | PUSH-Transfer-Lebenszyklus | `START→POST`, `SUSPEND→status:paused`, `TERMINATE→DELETE` |
| `notification…receiverInfo` | EDR/STS-Kurzzeit-Token | **einziger** Bindungs-Mechanismus (s. Korrektur) |
| `NGSILD-Tenant` / `Fiware-Service` | nicht-fälschbar aus Vertrag injiziert | Orion vertraut dem Header blind |
| `X-Auth-Token` (Orion/PEP) | Vault-Secret (`authKeyAlias`) | nie an den Consumer |
| `Fiware-Correlator` | `contractAgreementId` | End-to-End-Audit-Spur |
| `csourceRegistrations` (Föderation) | **v1: deaktivieren** | sonst SSRF/DoS (s. Sicherheit) |

> **Korrektur 1 (gegen Quellcode geprüft):** `entityOperations/upsert` ist **nicht** „all-or-nothing", sondern liefert **207 Multi-Status** (partieller Erfolg). Nur `entityOperations/create` ist atomar. Der `NgsiLdDataSink` muss den 207-Fall explizit auf einen DPF-`StreamResult` abbilden (Teil-Fehler ≠ `COMPLETED`).
>
> **Korrektur 2 (gegen Quellcode geprüft):** Die Token-Bindung beim Push funktioniert **ausschließlich** über `receiverInfo`. Der „Auto-Forward von `X-Auth-Token`" in `notificationSend.cpp` weiterzuleiten ist der Token **des Schreibenden**, der die Entity-Änderung ausgelöst hat — nicht der des Consumers. Das ist sogar ein **Leck** (s. Abschnitt 7, Befund 2).

---

## 6. GAIA‑X-Compliance — *richtig* aufgesetzt

> ⚠️ **Die wichtigste konzeptionelle Korrektur:** Eine VP im DSP/DCP-Handshake zu präsentieren ist **nicht** dasselbe wie GAIA‑X-Compliance. Es sind **zwei getrennte Trust-Schichten**. Im DSP/DCP-Laufzeitpfad gibt es **keinen** GXDCH-Aufruf. Stock-IdentityHub prüft VC-Signaturen und DCP-Envelopes — aber **nicht** GAIA‑X-`gx:`-Shapes oder die GXDCH-Trust-Anchor-Kette.

### (A) Out-of-band-Onboarding (einmalig/periodisch, vor dem Betrieb)

1. **DID + Schlüssel:** `did:web` unter `https://<domain>/.well-known/did.json`. **Aber:** GAIA‑X vertraut einem `did:web` *nicht* allein wegen DNS-Kontrolle. Die GAIA‑X-VCs müssen mit einem Schlüssel signiert sein, dessen **X.509-Zertifikatskette zu einem Trust-Anchor der GXDCH-Registry** (eIDAS-verwurzelte/akzeptierte CA) führt; der x5u/x509-Chain muss in der Verification-Method liegen. → **Achtung:** Die `did:web`-Identität für den **DSP/DCP-Transport** und die **GAIA‑X-Signatur-Identität** können verschiedene Schlüssel sein.
2. **Participant-VCs:** `gx:LegalParticipant` (selbst-signiert) + `gx:legalRegistrationNumber` (**vom GXDCH-Notary** ausgestellt) + `gx:GaiaXTermsAndConditions`.
3. **Offering-VCs** (pro Asset, auto-generiert aus `Asset.properties`): `gx:ServiceOffering` + `gx:DataResource` (ggf. `gx:InstantiatedVirtualResource`/`gx:PhysicalResource`).
4. **GXDCH-Compliance-Call:** Alle VCs als VP an den **GXDCH-Compliance-Service** → dieser gibt eine **`gx:compliance`-VC** zurück. **Diese Credential ist der Dreh- und Angelpunkt** — sie landet im Wallet und muss zur Laufzeit präsentiert werden.
5. **Federated Catalogue:** Der GAIA‑X-Catalogue ingestiert **keinen** DSP/DCAT-Endpoint, sondern **signierte Self-Description-VCs** über seine eigene Upload-API. → Es braucht einen **VC-Self-Description-Uploader** (zusätzlich zum DCAT-Mapper).

### (B) Laufzeit (pro DSP-Transaktion)

6. Der Counterparty präsentiert via DCP eine VP, die die **bereits ausgestellte `gx:compliance`-VC** enthält. Der Provider-Verifier muss **selbst** prüfen: Signatur kettet zu einem **GXDCH-Key auf der Registry-Trust-List** (Issuer-Pinning gegen self-signed Look-alikes) **und** `gx:`-Shapes erfüllt. → Das ist ein **zu bauender GAIA‑X-VC-Verifier**, kein Stock-Feature.
7. ODRL-Access-Policies fordern die **`gx:compliance`-VC** (nicht eine selbst-ausstellbare Legal-Person-VC), bevor Katalog-Sichtbarkeit/Agreement entsteht.

> **Versions-Hinweis:** Die Aussage „Loire 24.06 = VC-JWT" ist **unsicher** und sollte nicht hartkodiert werden. „22.10" ist eine *Trust-Framework-Dokumentversion*, „Tagus/Loire/Aquitaine" sind *GXDCH-Release-Namen* — nicht vermischen. Zum Stichtag (Mitte 2026) ist das aktuelle GXDCH-Release vermutlich **jenseits von Loire**. Proof-Format (JSON-LD-LD-Proofs vs. JOSE/VC-JWT) **pro Ziel-Release** verifizieren. Außerdem zu beachten: **Ablauf/Revocation** der `gx:compliance`-VC (StatusList) und periodische Re-Compliance.

---

## 7. Kritische Sicherheits-Befunde (gegen Orion‑LD-Quellcode verifiziert)

Diese sind real und müssen **vor** Produktivbetrieb adressiert werden — der EDC kann sie teils **nicht** „von außen" schließen, weil sie in Orions eigenem Outbound-Code sitzen. Line-Angaben können über Versionen driften; relevant ist das *Verhalten*.

1. **🔴 TLS-Verifikation in Notifications/Föderation hart abgeschaltet.** `httpsNotify.cpp` (HTTPS-Notification) und `distOpSend.cpp` (verteilte Queries) setzen `CURLOPT_SSL_VERIFYPEER=0` und `VERIFYHOST=0` **bedingungslos** (nicht hinter `-insecureNotif`). D. h. der **Broker→Consumer-Ingress-Hop** (genau der Pfad, den die Push-Bridge einführt) akzeptiert *jedes* Zertifikat → MITM kann den ganzen Stream **und** den EDR-Token aus `receiverInfo` abgreifen. → **mTLS an einem Sidecar/Proxy vor dem Ingress erzwingen** (der Ingress, nicht Orion, validiert den Peer) **oder** Orion patchen, eine CA-Bundle zu honorieren. **Harte Phase-2-Vorbedingung.**

2. **🔴 Cross-Principal-Credential-Leak.** `notificationSend.cpp` injiziert das `X-Auth-Token`/`Authorization` **des auslösenden Schreibers** (`orionldState.in`) in die ausgehende Notification, *sofern `receiverInfo` diesen Header nicht schon setzt*. → Der Write-Back-/Ingest-Pfad (oder ein anderer interner Writer) kann sein Upstream-Orion-/PEP-Token an den Consumer-Ingress verlieren. **Gegenmaßnahme:** in jeder Subscription `Authorization` **und** `X-Auth-Token` in `receiverInfo` explizit setzen (auch leer), damit der Auto-Forward unterdrückt wird; + Test, der sicherstellt, dass **kein** anderer Token als der EDR-Token je in einer Notification erscheint.

3. **🔴 Netz-Isolation ist eine *unausgesprochene* tragende Annahme.** Orion validiert weder `X-Auth-Token` noch den `Fiware-Service`/Tenant-Header — es vertraut beidem blind. Die gesamte „Orion = reine Data Plane, EDC schließt die Lücken"-These bricht zusammen, wenn **irgendein** Pfad zu Orions REST-Surface (insb. `POST /subscriptions`, `csourceRegistrations`, `jsonldContexts`) **nicht** über die EDC-Data-Plane läuft. → **Invariante per NetworkPolicy/mTLS:** Orions HTTP-Listener ist *ausschließlich* von den EDC-Data-Plane-Prozessen (+ PEP) erreichbar. App-Layer-Allowlists allein sind umgehbar.

4. **🟠 ODRL-Scope „nur narrow, nie widen" nicht durch Diffing, sondern durch Konstruktion.** Eine konsumentenseitige `q`/`geoQ` als Teilmenge des Vertrags-Scopes zu *beweisen* hieße, NGSI‑LD-Q-Sprache/Geo-Containment in Java nachzubauen (fehleranfällig, „subtile Divergenz"). **Stattdessen:** Die Query wird **provider-seitig aus dem Vertrag konstruiert**; der Consumer darf nur aus einem **geschlossenen, validierten Parameter-Set** wählen (`attrs ⊆ erlaubt`, Zeit im Fenster, bbox im Polygon). Beim **Push** ist der Subscription-Body **vollständig provider-abgeleitet** — der konsumentenseitige Ingress-Filter ist nur Defense-in-Depth, **nicht** der Enforcement-Punkt. `notifiedAttributes` zwingend begrenzen (sonst Over-Fetch via `previousValue`).

5. **🟠 Voll-Extraktion trotz „Result-Cap".** Orion begrenzt `limit` nur **pro Seite** (1000), nicht total. Da die Source dem `rel=next`-Chain folgt, kann jeder Lese-Vertrag das **gesamte** Entity-Set paginieren. → **Total-Zeilen/-Bytes/-Seiten-Cap + Wall-Clock-Budget pro Transfer** in der `NgsiLdDataSource`; `ngsild:temporalWindow` mit Max-Spanne **fail-closed**.

6. **🟠 Verwaiste Subscriptions = unkontrahierter Datenfluss.** Orion kennt das EDC-Agreement nicht; nur ein explizites `DELETE` stoppt den Stream. Fällt der Bridge-Prozess oder das `subId↔transferProcessId`-Mapping aus, pusht Orion weiter. → Subscription `expiresAt` **≤ Agreement/Token-Expiry** (fail-closed) **und** ein Reconciliation-Sweeper, der Subscriptions ohne lebenden Transfer löscht. Für Revoke **immer `DELETE`, nie pausieren**.

7. **🟠 `@context`-SSRF & Context-Remap-Policy-Bypass.** Orion lädt `@context` server-seitig (inkl. Fallback `http://localhost:80`). Kann der Consumer die `@context`-URL beeinflussen, ist das ein attacker-gesteuerter Fetch aus dem Provider-Netz; ein manipulierter Context kann Terme so umbenennen, dass eine `attrs`-Policy stillschweigend mehr freigibt. → `@context` **pro Asset pinnen** (immutabel), Orions Egress per Firewall auf einen internen Context-Cache beschränken, und **ODRL gegen voll-expandierte IRIs** (nicht Kurznamen) evaluieren.

8. **🟠 Föderation „out of scope" ≠ deaktiviert.** Bleibt `csourceRegistrations` erreichbar, macht eine einzige bösartige Registration den Broker zum Query-Forwarder über die (s. Befund 1) kaputte TLS-Verifikation. → Endpoint **netz-/PEP-seitig blocken**.

---

## 8. Roadmap

| Phase | Ziel | Kernaufgaben | Aufwand |
|---|---|---|---|
| **0 — Gate** | Build-vs-Adopt entscheiden | **FDSC‑EDC-Spike** (Pull/Temporal/Subscription, Orion‑LD, DCP+GXDCH, ODRL?); Ziel-GXDCH-Release + Federated Catalogue + Label-Level fixieren | 1–2 Wochen |
| **1 — PoC + native Pull** | DSP+DCP+GAIA‑X durchstechen, dann produktiver Lese-Pfad | (a) HttpData-PoC mit `did:web` + Participant-VC + erstem GXDCH-Compliance-Call, gegen `dsp-tck`; (b) `:data-plane-ngsi-ld` Source + `OrionLdClient` + Temporal + ODRL-Constraints + `NgsiLdParamsDecorator` + Caps/Anti-SSRF | PoC 1–3 Tage; native ~4–7 Wochen |
| **2 — Push-Bridge** | Souveräne Live-Feeds, ohne Broker-Code-Change | `:data-plane-ngsi-ld-subscription` (Signaling↔Subscription), Token in `receiverInfo` + Rotation (Overlap-Fenster!), `NgsiLdSubscriptionIngress` (fail-closed), **mTLS-Vorbedingung (Befund 1)**, Callback-Allowlist, `expiresAt`-Bindung + Reconciliation, Notification-Health-Feedback in den Transfer-State, `NgsiLdDataSink` (207-Handling) | 4–7 Wochen (+3–4 für Kafka/at-least-once C2 falls gefordert) |
| **3 — Labels, Föderation, Härtung, Betrieb** | Höhere GAIA‑X-Labels, Auffindbarkeit, Produktionsreife | `gx:ServiceOffering`/`gx:DataResource`-VCs auto-generieren → **GXDCH-Compliance-VCs** + Self-Description-**VC-Upload** in den Federated Catalogue; GAIA‑X-VC-Verifier (Shapes + Issuer-Pinning); Multi-Tenant-Header-Integritäts-Tests, Vault-per-Tenant-Rotation; **Observability** (Korrelations-Map `agreementId↔transferProcessId↔subId↔Fiware-Correlator`, Alerts auf Token-Rotation/Allowlist/Cap); Versions-Pinning + CI gegen `dsp-tck` **und** ein **Orion‑LD-Kompatibilitäts-Testkit** | 4–8 Wochen |

---

## 9. Offene Entscheidungen

1. **Ist *Eclipse* EDC zwingend** — oder erfüllt der FIWARE-DSC/FDSC‑EDC die Anforderung (das ändert alles)?
2. **Welcher Ziel-Dataspace / GXDCH-Release / Federated Catalogue** und welches **GAIA‑X-Label** (Standard vs. Label 1–3)? Bestimmt Proof-Format, VC-Set und Ingestion-API.
3. **EDC-Distribution & gepinnter Release:** Tractus‑X-EDC, Sovity CE oder eigener Launcher?
4. **Push nötig — und wenn ja, at-least-once?** (zieht die Kafka-Variante C2 + extra Failure-Domain nach sich)
5. **Multi-Tenancy ab Tag 1?** (verschiebt Header-Integritäts-/Vault-Arbeit nach vorn)
6. **NGSI‑LD-Föderation (`csourceRegistrations`) in Scope?** Wenn nein: deaktivieren (Sicherheit). Wenn ja: eigener Security-Workstream.
7. **Temporal/TRoE benötigt** (PostgreSQL) und mit den Result-Caps verträglich?

---

## Anhang A — Relevante Orion‑LD-Integrationsoberfläche (aus dem Quellcode)

- **Core-API:** `GET/POST/PATCH/PUT/DELETE /ngsi-ld/v1/entities`, Batch unter `/ngsi-ld/v1/entityOperations/{create,upsert,update,delete,query}`.
- **Query-Parameter:** `q` (NGSI‑LD-Q-Sprache), `geoQ`, `type`, `id`, `attrs`, `limit`, `offset`, `options` (`concise`/`simplified`/`temporalValues`).
- **Temporal:** `GET /ngsi-ld/v1/temporal/entities?timerel=…&timeAt=…&timeproperty=observedAt|modifiedAt|createdAt` (benötigt PostgreSQL/TRoE + PostGIS).
- **Subscriptions:** `POST/GET/PATCH/DELETE /ngsi-ld/v1/subscriptions`; Notification-Ziele HTTP/HTTPS/MQTT/MQTTS/WS/WSS; `notification.endpoint.receiverInfo`/`headers` (Custom-Header inkl. Token); `status:active|paused` (Standard) bzw. `isActive` (Orion-spezifisch).
- **Federation:** `POST/GET/PATCH/DELETE /ngsi-ld/v1/csourceRegistrations` — leitet passende Queries an entfernte Broker weiter (`distOp`).
- **Context-Server:** `POST/GET/DELETE /ngsi-ld/v1/jsonldContexts`; Antworten referenzieren `@context` via `Link: <url>; rel="http://www.w3.org/ns/json-ld#context"`.
- **Multi-Tenancy:** `NGSILD-Tenant` (Alias `Fiware-Service`) + `Fiware-Servicepath`.
- **Auth:** `X-Auth-Token` wird nur durchgereicht/weitergeleitet, **nicht** validiert (RBAC ist an einen vorgelagerten PEP/Reverse-Proxy delegiert).
- **Server:** libmicrohttpd (TLS via `-https -key -cert`, CORS via `-corsOrigin`, `-maxConnections`).
- **Relevante Pfade:** `src/lib/orionld/serviceRoutines/`, `src/lib/orionld/notifications/` (`notificationSend.cpp`, `httpsNotify.cpp`), `src/lib/orionld/distOp/` (`distOpSend.cpp`), `src/lib/orionld/mhd/`, `src/app/orionld/orionldRestServices.cpp`.

## Anhang B — Relevante EDC-Bausteine (Stand v0.x, ~v0.17.0)

- **Control Plane:** `control-plane-core/-aggregate-services/-api`, Management API v3 (`/management/v3/assets|policydefinitions|contractdefinitions|catalog/request|contractnegotiations|transferprocesses`).
- **Offering-Modell:** Asset (+ `dataAddress`) · PolicyDefinition (ODRL) · ContractDefinition (`accessPolicyId` + `contractPolicyId` + `assetsSelector`).
- **DSP:** `dsp-catalog-http`, `dsp-negotiation-http`, `dsp-transfer-process-http`, `dsp-http-core` (DSP 2024-1/2025-1); Konformität via `dsp-tck` (140+ Tests).
- **Data Plane Framework:** `DataSource`/`DataSink`/`DataSourceFactory`/`DataSinkFactory`, `PipelineService`, Data Plane Signaling API (`START/SUSPEND/RESUME/TERMINATE`), `DataPlaneSelector`, EDR (consumer-pull) und `data-plane-http` (`proxyPath/proxyQueryParams/proxyMethod/proxyBody`).
- **Extension-SPI:** `org.eclipse.edc.spi.system.ServiceExtension` + `META-INF/services`-Registrierung, DI via `@Inject`/`@Provider`/`@Requires`/`@Setting`.
- **Identität:** IdentityHub (Presentation API ~:10001, Identity API ~:8182, STS), **DCP** (formerly IATP) mit `did:web` + W3C VCs/VPs; DAPS/OAuth2 nur noch als Legacy.
- **Distributionen:** Eclipse EDC (Upstream) → Tractus‑X-EDC (Catena‑X, Docker/Helm, Postgres + Vault) → Sovity CE (UI + vereinfachte API).

---

*Dieses Dokument ist ein Entwurf zur Entscheidungsfindung. Die nächste konkrete Aktion ist der FDSC‑EDC-Spike (Abschnitt 2) parallel zum HttpData-PoC (Phase 1).*
