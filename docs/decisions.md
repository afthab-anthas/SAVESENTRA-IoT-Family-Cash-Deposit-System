# Architectural Decisions

> **Rule:** Every ADR below is grounded in evidence found in actual source files, configuration, migration SQL, or project documentation. No decision is recorded without a citation.

---

## ADR-001: AI Provider — Groq Llama 3.3 70B (not Claude / Anthropic)

**Status:** Accepted (supersedes original Anthropic design in `notes/axis-pulse-archdoc.md`)

**Context:** The architecture document (`notes/axis-pulse-archdoc.md`) specifies `claude-sonnet-4-6` as the AI model for narration. However the actual implementation diverges from this.

**Decision:** Narration (`src/lib/narrate.ts`) and feed-summary generation (`src/lib/gemini.ts`) both use **Groq** (`@ai-sdk/groq`, model `llama-3.3-70b-versatile`) rather than Anthropic Claude. The `package.json` lists `@ai-sdk/groq` as a production dependency. `GROQ_API_KEY` is the operative API key.

**Rationale (inferred from code):** The file `src/lib/gemini.ts` — named for Gemini — also calls Groq, suggesting the provider was switched mid-build. The `.env.example` references `GEMINI_API_KEY` as a "free tier" option added in P20, while `GROQ_API_KEY` is used in the running code. Groq's free tier and low-latency inference are the likely practical drivers.

**Consequences:** The cost ceiling in `src/lib/cost-ceiling.ts` still references Sonnet 4.6 pricing (`$3/1M input, $15/1M output`) but the actual model is Groq's Llama. The `/admin/budget` display is therefore an approximation. `Intelligence.modelUsed` records `"llama-3.3-70b-versatile"` not `"claude-sonnet-4-6"`.

**Evidence:** [`src/lib/narrate.ts:1-15`](src/lib/narrate.ts:15) — `const MODEL = "llama-3.3-70b-versatile"`, `createGroq({ apiKey: process.env.GROQ_API_KEY })`. [`src/lib/gemini.ts:1-47`](src/lib/gemini.ts:1) — same pattern. [`package.json:17`](package.json:17) — `"@ai-sdk/groq": "^3.0.39"`.

---

## ADR-002: Delta Gating — SHA-256 Hash of Inputs Before Every AI Call

**Status:** Accepted

**Context:** AI narration is expensive and should not fire on identical inputs. A 90-second cooldown alone is insufficient because a project with frequent identical events would still accumulate calls after the cooldown expires.

**Decision:** Before every narration call, compute `inputHash = sha256(pNumberDetected | lastCommitSha | filesChangedBucket | sessionExcerpt | gitSummary | eventKind | repoContextRefreshedAt)`. If the latest `Intelligence.inputHash` for the project equals the new hash, skip the AI call entirely. A separate 90-second in-memory cooldown is applied regardless of delta.

**Rationale:** Deterministic deduplication at zero cost. The `@@unique([projectId, inputHash])` Prisma constraint on `Intelligence` provides a database-level guard against concurrent double-writes with the same hash.

**Consequences:** The cooldown map (`cooldowns: Map<string, number>`) is in-process memory — intentionally acceptable for single-process 0.5x/1x deployments. A comment in the source (`// Acceptable for single-process 0.5x deployment; promoted to Redis in P17`) marks this as known deferred work. Snapshot refreshes invalidate the gate because `repoContextRefreshedAt` is part of the hash.

**Evidence:** [`src/lib/narrate.ts:76-97`](src/lib/narrate.ts:86) — `computeInputHash()`. [`src/lib/narrate.ts:18-25`](src/lib/narrate.ts:18) — in-memory cooldowns map with comment. [`notes/axis-pulse-archdoc.md:93-99`](notes/axis-pulse-archdoc.md:93) — delta gating specification.

---

## ADR-003: Agent Token Auth — bcrypt + Pepper + Preview Index

**Status:** Accepted (evolved from single-hash to per-developer table in P-post-3)

**Context:** Claude Code hook scripts must authenticate to `/api/ingest/event` without storing a plaintext token anywhere. The token is shown once on creation/rotation and must be irrecoverable after that.

**Decision:** Agent tokens are 32-byte `crypto.randomBytes` encoded as `base64url`. They are stored as `bcrypt(pepper + raw, cost=12)`. A 4-character `tokenPreview` (last 4 chars) is stored in plaintext for fast pre-filter lookup. A dedicated `AgentToken` table (added in migration `20260607080000_per_developer_agent_tokens`) supports per-developer tokens. Legacy `Project.agentTokenHash` is retained as a fallback with a grace-window for old installs.

**Rationale:** bcrypt at cost 12 is standard for secret storage. Pepper (`AGENT_TOKEN_PEPPER` env) adds a server-side secret that makes offline dictionary attacks against a leaked DB useless. The preview-based pre-filter avoids full-table bcrypt scans on every ingest request. Grace-window support (`verifyTokenWithGrace`) prevents service interruption when tokens are rotated.

**Consequences:** Token rotation involves two hashes in the `previousTokenHash`/`previousTokenExpiresAt` fields during the grace window. The `AgentToken` table's legacy migration comment explicitly marks `Project.agentTokenHash` as intentional tech-debt: "should be retired once all projects have ≥1 AgentToken row."

**Evidence:** [`src/lib/token.ts:1-35`](src/lib/token.ts:1) — full implementation. [`src/lib/resolveAgentToken.ts:1-63`](src/lib/resolveAgentToken.ts:1) — lookup strategy with fallback. [`prisma/migrations/20260607080000_per_developer_agent_tokens/migration.sql:1-8`](prisma/migrations/20260607080000_per_developer_agent_tokens/migration.sql:1) — migration comment documenting the intentional tech-debt.

---

## ADR-004: Three-Tier RBAC — MANAGER / LINE_MANAGER / MEMBER

**Status:** Accepted

**Context:** A single admin/user binary is insufficient for a multi-team consulting org. Managers need org-wide visibility; line managers need team-scoped visibility; members need self-only time data.

**Decision:** Three roles enforced at the application layer via `withAuthScoped()`. MANAGER has `teamId IS NULL` and sees all teams. LINE_MANAGER has `teamId IS NOT NULL` and one team. MEMBER has `teamId IS NOT NULL`. Exactly one LINE_MANAGER per team is enforced by application code (409 on second promotion attempt). Scope derivation happens in `withAuthScoped()` and produces `{ allTeams, canSeeTimeData, viewedTeamIds }` consumed by every route handler.

**Rationale:** The archdoc's role visibility matrix (`notes/axis-pulse-archdoc.md:725-738`) drove the three-tier design. The invariant "one LM per team" is app-enforced rather than DB-enforced because Postgres partial unique indexes across nullable boolean columns are complex; the app-level 409 is simpler and equally effective for a single-process deployment.

**Consequences:** Every API route must call `withAuthScoped()` before any DB query — there is no automatic middleware injection. The response serialiser must strip the `time` block from member cards when `scope.canSeeTimeData === false`. Field omission happens server-side before the response leaves the process (defence in depth).

**Evidence:** [`src/lib/withAuthScoped.ts:1-76`](src/lib/withAuthScoped.ts:22) — full implementation with `orgWhere()`, `teamWhere()` helpers. [`notes/axis-pulse-archdoc.md:65-82`](notes/axis-pulse-archdoc.md:65) — role table definition. [`notes/user-story.md:429-459`](notes/user-story.md:429) — P9 RBAC phase spec.

---

## ADR-005: Session Strategy — NextAuth v5 JWT (No Database Sessions)

**Status:** Accepted

**Context:** The app needs stateless sessions that survive process restarts and do not require a sessions table. Role and teamId must be readable from the session without a DB round-trip on every request.

**Decision:** NextAuth v5 with `session: { strategy: "jwt" }`. No credentials provider — authentication is handled by the custom `/api/auth/login` route. Role, teamId, isLineManager, organisationId, and sessionVersion are embedded in the JWT payload via the `jwt` callback.

**Rationale:** JWT sessions are cheaper (no DB query per request) and survive horizontal scale-out. The `sessionVersion` field (an integer on the `User` row, checked in `withAuthScoped`) provides a server-side invalidation mechanism: bumping the DB version forces a re-login even though the JWT is still cryptographically valid.

**Consequences:** The `_sessionVersionCache` in `withAuthScoped.ts` caches the DB version for 30 seconds to avoid a DB hit on every request. If role or team assignments change, there is up to a 30-second propagation delay before the session reflects the change — acceptable for this use case.

**Evidence:** [`src/lib/auth.ts:1-30`](src/lib/auth.ts:5) — NextAuth config with `strategy: "jwt"`, no providers. [`src/lib/withAuthScoped.ts:4-16`](src/lib/withAuthScoped.ts:4) — session version cache with 30 s TTL. [`notes/axis-pulse-archdoc.md:803-809`](notes/axis-pulse-archdoc.md:803) — authentication design spec.

---

## ADR-006: Redaction Pipeline — Dual-Layer (Client + Server), Byte-Identical

**Status:** Accepted

**Context:** Claude Code hook scripts send session excerpts (prompts, tool outputs) that may contain API keys, JWTs, AWS credentials, PEM blocks, and URL-embedded passwords. These must never reach the database or logs.

**Decision:** A 7-rule regex redaction pipeline runs on **both** the client script (`lib/redact.mjs`) and the server ingest route (`src/lib/redact.ts`). The two files implement byte-identical logic (same regex patterns, same placeholder `[REDACTED]`, same 1500-char truncation). A shared test suite (`src/tests/redact-parity.test.ts`) asserts both produce identical output on every corpus case. Rules cover: env-style assignments, JWTs, Anthropic/OpenAI keys, AWS access keys, PEM blocks, URL credentials.

**Rationale:** Defence in depth. If the client script is compromised or buggy, the server catches what was missed. The archdoc explicitly states: "raw `sessionExcerpt` never logged on any path." Redaction count and kinds are emitted to `ActivityEvent.redactionCount` and an `AUDIT` row — but the content of removed values is never recorded.

**Consequences:** The `.mjs` and `.ts` files must be kept in sync manually — there is no automatic code-sharing mechanism since one is ESM for Node scripts and one is TypeScript for the Next.js server. The parity test is the CI gate. `sessionExcerpt` in DB is guaranteed ≤ 1500 chars.

**Evidence:** [`src/lib/redact.ts:1-75`](src/lib/redact.ts:1) — server implementation. [`lib/redact.mjs:1-55`](lib/redact.mjs:1) — client implementation (byte-identical rules). [`notes/axis-pulse-archdoc.md:849-861`](notes/axis-pulse-archdoc.md:849) — redaction pipeline specification.

---

## ADR-007: Rate Limiting — Upstash Redis Sliding Window, Fail-Open

**Status:** Accepted

**Context:** The ingest endpoint and all AI-triggering routes must be protected from abuse and runaway cost. Rate limiting must work across restarts without in-process state.

**Decision:** Upstash Redis (`@upstash/ratelimit`, `@upstash/redis`) with sliding window algorithm. Each route has its own limiter instance with documented limits (e.g. ingest: 60/min/token; login: 5/min/IP; summary: 10/hr/project). When `UPSTASH_REDIS_REST_URL` or `UPSTASH_REDIS_REST_TOKEN` env vars are absent, all limiters **fail-open** (allow all requests). Rate limits scale with `SCALE_TIER` env var: 4x tier multiplies the ingest limit by 4.

**Rationale:** Fail-open is an explicit choice: blocking all requests when Redis is unavailable would stop ingest and harm the demo. The risk of a brief unprotected window during a Redis outage is acceptable for v1. Upstash's HTTP-based Redis client works from Next.js Edge and serverless contexts where a TCP Redis connection is not available.

**Consequences:** Rate limit enforcement is a deferred dependency on production Redis provisioning — recorded in `notes/pending-work.txt` as "[P4] Production Redis for Rate Limiting — PENDING." In local dev, a Docker bridge workaround is documented in the same file.

**Evidence:** [`src/lib/ratelimit.ts:1-45`](src/lib/ratelimit.ts:15) — fail-open logic and scale-tier multiplier. [`notes/pending-work.txt:8-37`](notes/pending-work.txt:8) — pending production Redis wiring. [`build-evidence/P17/agent-wave-manifest.json:13`](build-evidence/P17/agent-wave-manifest.json:13) — conformance test verifying all 12 documented route limits.

---

## ADR-008: HMAC Request Signing on Ingest Endpoint

**Status:** Accepted

**Context:** Bearer token authentication alone does not prevent replay attacks or body tampering if a token leaks via logs. The ingest endpoint receives sensitive hook payloads from developer machines.

**Decision:** Every POST to `/api/ingest/event` and `/api/ingest/time` must carry `X-Pulse-Signature: HMAC_SHA256(rawBody, agentToken)`. The server recomputes the HMAC and performs a constant-time comparison. Bearer token + HMAC must both pass; either alone is rejected.

**Rationale:** Two-factor ingest authentication: token proves identity, HMAC proves body integrity. The archdoc explicitly states this "protects against replay if the bearer token leaks via logs and against a body-only tamper attack on the transport."

**Consequences:** The client script (`pulse-send.mjs`) must HMAC-sign every payload before sending. The raw token must be available on the client machine. The HMAC secret and the bearer token are the same value — if the token is rotated, the signing key changes immediately.

**Evidence:** [`notes/axis-pulse-archdoc.md:820-823`](notes/axis-pulse-archdoc.md:820) — request signing specification. [`build-evidence/P4/agent-wave-manifest.json`](build-evidence/P3/agent-wave-manifest.json) — HMAC failure-mode evidence in P4 artifacts.

---

## ADR-009: Multi-Tenancy — organisationId Column Promotion (Not a Full Rewrite)

**Status:** Accepted

**Context:** The v1 archdoc specified a single-org deployment with a nullable `tenantKey` reserved for future multi-tenancy. During implementation, the `Organisation` entity was introduced as a concrete entity.

**Decision:** An `Organisation` model was added post-P3 via migration `20260604000001_add_organisations`. All major tables (`User`, `Team`, `Project`, `ActivityEvent`, `Intelligence`, `AuditLogEntry`, `MemberDailyTime`, `ProductiveAppRule`, `ExecutiveSummary`, `Invitation`) received an `organisationId NOT NULL` column backfilled to a seeded `org_axis_seed` row. All `withAuthScoped()` helpers inject `organisationId` into every DB query automatically via `orgWhere(ctx)`.

**Rationale:** The column-promotion approach allows multi-tenancy to be enabled by provisioning a new `Organisation` row and assigning users/teams to it — no schema migrations required. The archdoc noted: "future multi-tenant migration is a non-breaking column promotion."

**Consequences:** The `tenantKey` nullable column remains on many tables from the original design but is superseded by `organisationId` as the operative scoping field. Both exist in the schema simultaneously.

**Evidence:** [`prisma/migrations/20260604000001_add_organisations/migration.sql:1-80`](prisma/migrations/20260604000001_add_organisations/migration.sql:1) — full promotion migration with backfill. [`src/lib/withAuthScoped.ts:68-76`](src/lib/withAuthScoped.ts:68) — `orgWhere()` helper. [`notes/axis-pulse-archdoc.md:61-63`](notes/axis-pulse-archdoc.md:61) — tenantKey reservation rationale.

---

## ADR-010: PDF Export — @react-pdf/renderer (Not Playwright Headless)

**Status:** Accepted

**Context:** The archdoc listed PDF generation as an open question: "Playwright headless vs `@react-pdf/renderer` — decision in Week 4 based on Coolify image size constraints."

**Decision:** `@react-pdf/renderer` was chosen, as evidenced by its presence in `package.json` (`"@react-pdf/renderer": "^4.5.1"`). Playwright is listed only as a devDependency (`"@playwright/test": "^1.60.0"`), not used for PDF generation.

**Rationale (inferred from package.json):** `@react-pdf/renderer` runs in the Node.js process without a headless browser binary, which avoids the large Chromium bundle size and container complexity on Coolify. Playwright remains as a test dependency only.

**Consequences:** PDF styling is limited to `@react-pdf/renderer`'s PDF primitives (no arbitrary CSS). The PDF is rendered server-side in the Next.js route handler at `GET /api/projects/:id/export`.

**Evidence:** [`package.json:23`](package.json:23) — `"@react-pdf/renderer": "^4.5.1"`. [`notes/axis-pulse-archdoc.md:1001`](notes/axis-pulse-archdoc.md:1001) — PDF library decision deferred. [`notes/user-story.md:670-698`](notes/user-story.md:670) — P14 executive summary & PDF spec.

---

### ADR-010-ext: PDF Export — MEMBER Role Ungated, Financial Data Excluded Server-Side (Phase 12)

**Status:** Accepted (extends ADR-010)

**Context:** The original export route (Phase 14) restricted `GET /api/projects/[id]/export` to MANAGER and LINE_MANAGER, returning 403 for MEMBER. Phase 12 removed this restriction.

**Decision:** MEMBER role users can now request a PDF export. Financial data (`spendUSD`, `MemberDailyTime`) is conditionally excluded server-side based on the authenticated user's `canSeeSpend` flag (derived from `withAuthScoped`). When `canSeeSpend` is false, `spendUSD: null` is passed to `SummaryPDF` and the Developer AI Spend section (§7.5) is omitted from the rendered PDF. `MemberDailyTime` is not fetched at all when `canSeeSpend` is false.

**Rationale:** MEMBER users have legitimate need to view and share project intelligence. Financial data exclusion is enforced at the DB-query level (the fetch is never made), not by client-side hiding — preserving the Rule 5 invariant that MEMBER users cannot access teammates' time data or spend figures. The `SummaryPDF` component receives `spendUSD: null` as a genuine signal, not a default; it branches on this to omit the spend section entirely.

**Consequences:** The RBAC table in ADR-004 remains consistent — MEMBER still cannot see financial data; the route change only removes the blanket 403. The rate limiter at 20/hr/project applies equally to all roles. `ExecSummaryButton` now shows "View report" and "Export PDF" buttons to all users who have a stored summary, regardless of role.

**Evidence:** `src/app/api/projects/[id]/export/route.ts` — `canSeeSpend` conditional fetch; `SummaryPDFProps.spendUSD?: number | null`. `src/app/projects/[id]/_components/ExecSummaryButton.tsx` — four-button cluster visible to all roles.

---

## ADR-011: GitHub Integration — GitHub App (Not PATs)

**Status:** Accepted

**Context:** The Repository Context Layer needs read access to private GitHub repos. The alternatives were Personal Access Tokens (PATs) tied to individual users, or a GitHub App installation.

**Decision:** A GitHub App named "Axis Pulse" with `contents: read` + `metadata: read` permissions only. Installation tokens are minted per-installation using the App's private key (RS256 JWT), cached in-process for 50 minutes (tokens expire in 60 minutes). The host allowlist in `githubFetch()` restricts all calls to `api.github.com` only.

**Rationale:** The archdoc documents the decision explicitly: "private repos work out of the box; 5000+ req/hour rate budget; no human-tied credential; centralised revocation." PATs would require a team member to own the token and would expire or become invalid when that person leaves.

**Consequences:** Requires one-time setup by an org admin to register the GitHub App and add `GITHUB_APP_ID` + `GITHUB_APP_PRIVATE_KEY` (base64-PEM) to Coolify env. The `GITHUB_APP_WEBHOOK_SECRET` env var is reserved but unused in v1. Installation token caching is in-process memory (not Redis) — acceptable for single-process deployment.

**Evidence:** [`src/lib/repo-context/client.ts:1-107`](src/lib/repo-context/client.ts:62) — host allowlist enforcement, JWT minting, token cache. [`notes/axis-pulse-archdoc.md:474-481`](notes/axis-pulse-archdoc.md:474) — GitHub App rationale. [`.env.example:8-13`](.env.example:8) — GitHub App env vars.

---

## ADR-012: Repository Context — Summarised Snapshot Only (No Full Indexing)

**Status:** Accepted

**Context:** Narration quality improves when the AI knows the project's tech stack and structure, but sending full source files on every narration call would be expensive and risk leaking code.

**Decision:** A bounded JSON snapshot (≤ 8 KB) is generated at link time and stored in `Project.repoSnapshot`. It contains: top-level directory tree (≤ 20 entries), excerpts from a curated allowlist of key config files (≤ 8 files, ≤ 4 KB each, redacted), and a deterministic framework detector result. On each narration call, only the framework list, top-level paths, and config file paths are appended to the prompt — never raw config bodies.

**Rationale:** The archdoc constraint: "No full repo indexing, no embeddings, no vector search. Lightweight contextual enhancement only." Config excerpt bodies are stored in the DB for dashboard display but are explicitly excluded from per-narration prompts. Total tokens added per narration: ~150–300.

**Consequences:** The `KEY_CONFIG_ALLOWLIST` is a fixed set — extending it requires a code change. The snapshot becomes stale as the repo evolves; a 14-day staleness hint is shown in the UI. The `repoContextRefreshedAt` field is part of the `inputHash` so a manual refresh forces a fresh narration.

**Evidence:** [`src/lib/repo-context/snapshot.ts:1-31`](src/lib/repo-context/snapshot.ts:8) — `KEY_CONFIG_ALLOWLIST` and 8 KB enforcement. [`src/lib/narrate.ts:209-219`](src/lib/narrate.ts:209) — only paths/frameworks sent to AI, not raw excerpts. [`notes/axis-pulse-archdoc.md:463-467`](notes/axis-pulse-archdoc.md:463) — "not index code, no vector search" constraint.

---

## ADR-013: Time Tracking — ActivityWatch Integration, Three Integers Only

**Status:** Accepted

**Context:** Axis wanted Insightful-style activity tracking without the privacy risks of screenshot capture or window-title collection. A lightweight, privacy-first alternative was required.

**Decision:** ActivityWatch (open-source, MIT, runs locally on each team member's machine) is used as the data source. The `pulse-time.mjs` script runs every 5 minutes, reads today's window/AFK events from the local ActivityWatch REST API at `localhost:5600`, applies `ProductiveAppRule` matching **locally on the machine**, and posts only three integers (`workedSeconds`, `productiveSeconds`, `unproductiveSeconds`) plus IANA timezone to `/api/ingest/time`. The `pulse-body-shape.ts` allowlist is a CI gate: any upload body key outside the allowlist fails the test suite.

**Rationale:** "Privacy by construction" — the script has no code path that uploads window titles, URLs, or app names. The compile-time invariant is asserted by a unit test. The only data that leaves the machine is an aggregate summary of seconds.

**Consequences:** ActivityWatch must be installed separately on each team member's machine (not bundled by Axis Pulse). Time tracking is opt-in per team via `Team.timeTrackingEnabled` toggle. Members must acknowledge a non-dismissable consent modal before using the dashboard when tracking is enabled. `MemberDailyTime` stores one row per (user, day) — bounded storage of 18,250 rows/year for 50 members.

**Evidence:** [`src/lib/pulse-body-shape.ts:1-25`](src/lib/pulse-body-shape.ts:3) — upload body allowlist and CI gate. [`src/lib/consentCheck.ts:1-13`](src/lib/consentCheck.ts:1) — consent enforcement. [`notes/axis-pulse-archdoc.md:567-651`](notes/axis-pulse-archdoc.md:567) — ActivityWatch integration design.

---

## ADR-014: Productive App Rules — Client-Side Matching, Global + Team Scopes

**Status:** Accepted

**Context:** Determining whether a focused window/URL is "productive work" requires matching app names against a rule library. The matching must happen without raw app/URL data leaving the developer's machine.

**Decision:** The rule library has two scopes: `GLOBAL` (org-wide, editable by MANAGER) and `TEAM` (per-team overrides, editable by LINE_MANAGER). The merged rule list is cached at `.pulse/rules.json` on each member's machine (24-hour TTL, `If-None-Match`-driven refresh). Matching runs locally in `pulse-time.mjs` — Pulse server never sees the app names. Match types are `EXACT`, `GLOB` (simple `*` wildcard), and `REGEX`. TEAM rules override GLOBAL rules by pattern.

**Rationale:** Local matching is the privacy guarantee. The server-side `productive-rules.ts` module implements the same matching logic for tests and for any server-side use, but the operative runtime execution is on the client machine.

**Consequences:** A rule change on the server takes up to 24 hours to propagate to client machines unless the member manually refreshes. The glob implementation uses a simple `*`→`.*` regex transformation without `minimatch` — noted in a source comment: "`minimatch` is NOT in package.json, so GLOB matching is implemented as a simple regex transformation."

**Evidence:** [`src/lib/productive-rules.ts:1-136`](src/lib/productive-rules.ts:9) — match types, merge precedence, comment on minimatch absence. [`notes/axis-pulse-archdoc.md:629-643`](notes/axis-pulse-archdoc.md:629) — rule scopes and client-side matching design.

---

## ADR-015: Monthly Cost Ceiling — Hard Gate, Startup Enforcement

**Status:** Accepted

**Context:** AI narration calls must be bounded to prevent runaway spend. The archdoc required a manager-visible cost ceiling that blocks new calls when exceeded.

**Decision:** `CLAUDE_MONTHLY_CEILING_USD` env var (e.g. `"50.00"`) sets a monthly spending ceiling. `isCeilingExceeded()` sums `Intelligence.inputTokens * $3/1M + outputTokens * $15/1M` for the current calendar month and compares to the ceiling. When exceeded, narration calls short-circuit with `cost_ceiling_exceeded`. A global banner appears on every authed page. **In production, the app refuses to start if `CLAUDE_MONTHLY_CEILING_USD` is unset** — enforced by a startup check in `instrumentation.ts`.

**Rationale:** The archdoc states: "over-budget routes fail closed with a visible banner, never silently." The startup guard (`instrumentation.ts:6-8`) makes the ceiling mandatory in production — an operator cannot accidentally deploy without it.

**Consequences:** Spend tracking uses Sonnet 4.6 pricing constants even though the actual model is Groq Llama 3.3 70B — making the ceiling display an approximation. The ceiling resets at the start of each calendar month (UTC-based window).

**Evidence:** [`src/lib/cost-ceiling.ts:1-37`](src/lib/cost-ceiling.ts:7) — pricing constants and ceiling enforcement. [`instrumentation.ts:5-8`](instrumentation.ts:5) — startup guard refusing production start without ceiling. [`build-evidence/P17/agent-wave-manifest.json:12`](build-evidence/P17/agent-wave-manifest.json:12) — ceiling enforcement listed as delivered capability.

---

## ADR-016: Observability — In-Process Prometheus Metrics + Lightweight OTel Spans

**Status:** Accepted

**Context:** A manager-visible latency and cost dashboard is required, but integrating a full observability stack (Prometheus server, Jaeger, etc.) would be disproportionate for v1.

**Decision:** Metrics are stored as module-level in-process maps (`_routeStore`, `_claudeInputTokens`, `_claudeOutputTokens`) in `src/lib/metrics.ts`. Prometheus text format is generated on demand at `GET /api/metrics`. OTel spans are stored in a single `_lastTrace` in-process variable in `src/lib/otel.ts` — only one trace is retained (for build-evidence capture). All known routes are pre-registered so they appear in metrics even with zero traffic.

**Rationale:** Comment in `src/lib/metrics.ts:3`: "Module-level singleton — persists across requests in a single Node.js process (1x deployment). Promoted to Redis-backed store at 4x if needed." This matches the incremental scaling philosophy: use the simplest working solution at the current scale tier.

**Consequences:** Metrics reset on every process restart. P95 latency is computed from a rolling 24-hour in-memory sample capped at 5,000 samples. This is not suitable for multi-instance deployments. The OTel trace store only keeps the most recent trace — adequate for build-evidence capture but not for production tracing at scale.

**Evidence:** [`src/lib/metrics.ts:1-4`](src/lib/metrics.ts:1) — in-process store with scale comment. [`src/lib/otel.ts:1-5`](src/lib/otel.ts:1) — "Stores the last completed trace in memory for /build-evidence capture." [`build-evidence/P17/agent-wave-manifest.json`](build-evidence/P17/agent-wave-manifest.json) — OTel trace sample artifact.

---

## ADR-017: Structured Logging — pino with Redacted Field List

**Status:** Accepted

**Context:** Application logs must be structured for Coolify's log viewer but must never contain PII, session content, tokens, or authorization headers.

**Decision:** `pino` structured logger with a static `REDACTED_PATHS` list that replaces sensitive fields with `"[Redacted]"` at serialization time. Redacted paths include: `password`, `passwordHash`, `agentToken`, `agentTokenHash`, `sessionExcerpt`, `manualText`, `tenantKey`, `secret`, `apiKey`, `jwt`, `bearerToken`, `req.headers.authorization`, `req.headers['x-pulse-signature']`, `req.headers['x-internal-token']`.

**Rationale:** Pino's built-in `redact` option operates at serialization time — field values are never touched by string formatting. This is more reliable than post-hoc log scrubbing. The archdoc requires: "PII / session-content fields are explicitly redacted in the logger config."

**Consequences:** Fields on nested objects are only redacted if their path is explicitly listed. Developers must add new sensitive paths to `REDACTED_PATHS` when introducing new fields that carry secrets. Log level is configurable via `LOG_LEVEL` env var (default `"info"`).

**Evidence:** [`src/lib/logger.ts:1-37`](src/lib/logger.ts:5) — full REDACTED_PATHS list and pino config. [`notes/axis-pulse-archdoc.md:897`](notes/axis-pulse-archdoc.md:897) — "pino structured logger to stdout; PII / session-content fields are explicitly redacted."

---

## ADR-018: Security Headers — Per-Request CSP Nonce via Middleware

**Status:** Accepted

**Context:** Next.js 14 App Router injects inline bootstrap scripts for hydration. A static `script-src 'self'` CSP would block these scripts. But `'unsafe-inline'` would defeat CSP entirely.

**Decision:** The middleware generates a per-request nonce (`btoa(crypto.randomUUID())`) and passes it as `x-nonce` header. The CSP uses `'nonce-${nonce}'` plus `'strict-dynamic'` so Next.js's generated scripts are stamped automatically. In development, `'unsafe-eval'` is appended for webpack source maps; in production it is omitted.

**Rationale:** Comment in `src/middleware.ts:14-17` documents this explicitly: "Next.js 14 App Router injects inline bootstrap scripts for hydration, so a static 'self'-only script-src blocks them. A per-request nonce passed via x-nonce lets Next.js stamp its own generated scripts automatically."

**Consequences:** The CSP includes `https://api.anthropic.com` in `script-src` — a holdover from the original Anthropic Claude design that may no longer be necessary given the switch to Groq.

**Evidence:** [`src/middleware.ts:14-35`](src/middleware.ts:14) — nonce generation and CSP construction. [`notes/axis-pulse-archdoc.md:826-829`](notes/axis-pulse-archdoc.md:826) — transport and headers spec.

---

## ADR-019: P-Number Detection — Jaccard Similarity (No Embeddings)

**Status:** Accepted

**Context:** The system must auto-classify which numbered prompt workflow a Claude Code session is running. The alternatives were embedding-based similarity or deterministic text matching.

**Decision:** Jaccard similarity on the first 200 tokenized, lowercased, punctuation-stripped characters of each event's session text against each active `Prompt.fingerprint`. Highest similarity ≥ 0.6 wins. Below threshold → `pNumberDetected = null` → dashboard shows "Free workflow." Managers can manually override per event.

**Rationale:** The archdoc states: "Free, deterministic, fast. No embeddings, no vector store." Jaccard avoids embedding API costs and operational complexity. The 0.6 threshold and manager override provide graceful fallback for mismatches.

**Consequences:** Fingerprint quality depends on the first 200 characters of each prompt being distinctive. The 0.6 threshold is hardcoded in the matcher. Similar prompts in the same category could produce false positives.

**Evidence:** [`notes/axis-pulse-archdoc.md:450-459`](notes/axis-pulse-archdoc.md:450) — Jaccard specification and rationale. [`notes/user-story.md:300-326`](notes/user-story.md:300) — P6 prompt library phase spec with acceptance criteria.

---

## ADR-020: Consent Model — Versioned, Non-Dismissable, Re-Consent on Schema Change

**Status:** Accepted

**Context:** Time tracking collects behavioural data. Informed consent is required. The initial design used a simple `acknowledgedAt` timestamp.

**Decision:** `TimeTrackingConsent` has a `consentVersion` integer (added in migration `20260612000001_three_bucket_time_top_apps`). `consentCheck.ts` requires `acknowledgedAt` to be set AND `consentVersion >= 2`. Members who consented under v1 (aggregate-only) must re-consent when v2 (which adds `topApps` app-name collection) is deployed. The consent modal blocks the entire dashboard until clicked.

**Rationale:** Migration comment: "v1=aggregate-only, v2=includes app names." The version bump was triggered by the `topApps` JSONB field addition to `MemberDailyTime` — a material change to what data is collected that warrants fresh consent.

**Consequences:** All previously-consented users see the modal again on upgrade to v2. Demo seed accounts must have `consentVersion = 2` to avoid blocking the demo.

**Evidence:** [`src/lib/consentCheck.ts:10-12`](src/lib/consentCheck.ts:10) — version gate `(consent.consentVersion ?? 1) < 2`. [`prisma/migrations/20260612000001_three_bucket_time_top_apps/migration.sql:7-9`](prisma/migrations/20260612000001_three_bucket_time_top_apps/migration.sql:7) — consentVersion column with v1/v2 comment.

---

## ADR-021: Hook-Based Event Capture (Not a Polling Daemon)

**Status:** Accepted

**Context:** Capturing Claude Code activity requires either a persistent polling daemon or Claude Code's native hook extension points.

**Decision:** Claude Code's `PostToolUse`, `Stop`, and `UserPromptSubmit` hooks invoke `pulse-send.mjs` as a subprocess on each event. The script exits 0 silently on success and logs to `.pulse/log` on failure — never blocking the Claude session. No persistent daemon is installed.

**Rationale:** The archdoc documents explicitly: "Event-driven: zero polling cost when idle. No daemon to install, monitor, or keep running. Cross-platform free (Claude Code abstracts OS). Strongest 'fully agentic' narrative for judges."

**Consequences:** Events only land when Claude Code is active. A network failure causes the event to be dropped (logged locally, not queued for retry). Install footprint is six JSON lines in `.claude/settings.json` plus `.pulse/pulse-send.mjs`.

**Evidence:** [`notes/axis-pulse-archdoc.md:756-796`](notes/axis-pulse-archdoc.md:756) — hook vs daemon rationale. [`build-evidence/P5`](build-evidence/) — P5 evidence confirming E2E hook installation and event flow.

---

## ADR-022: Risk Schema — Three-Level riskLevel Replaces Boolean hasRisk

**Status:** Accepted (supersedes original boolean design)

**Context:** The original `Intelligence` schema used `hasRisk: Boolean` and `riskLabel: String?`. A boolean cannot distinguish HIGH from MEDIUM risk.

**Decision:** Migration `20260613000001_risk_level_focus` dropped `hasRisk` and `riskLabel`, replacing them with `riskLevel TEXT NOT NULL DEFAULT 'LOW'` (LOW / MEDIUM / HIGH) and `riskFocus TEXT?`. Existing rows backfilled: `hasRisk=true → MEDIUM`. The narration system prompt was updated to output the new fields.

**Rationale:** Migration comment: "hasRisk=true → MEDIUM (conservative default; no HIGH signal available from boolean)." Three levels allow the UI to distinguish shared-area changes (MEDIUM) from core-infrastructure changes (HIGH).

**Consequences:** Historical `Intelligence` rows with `hasRisk=true` now show as MEDIUM even if they were genuinely HIGH. The `<RiskBadge />` component now branches on three values.

**Evidence:** [`prisma/migrations/20260613000001_risk_level_focus/migration.sql:1-10`](prisma/migrations/20260613000001_risk_level_focus/migration.sql:1) — migration with backfill logic. [`src/lib/narrate.ts:53-70`](src/lib/narrate.ts:53) — updated system prompt with riskLevel/riskFocus output schema.

---

## Technology Choices

| Technology | Role | Evidence |
|---|---|---|
| **Next.js 14 (App Router)** | Full-stack framework — server components for data, route handlers for API, streaming for exec summary | [`package.json:33`](package.json:33) `"next": "14.2.35"` |
| **PostgreSQL + Prisma** | Primary data store with typed query builder; migrations in `prisma/migrations/` | [`prisma.config.ts`](prisma.config.ts), 50+ migration files |
| **Prisma PG Adapter** | Driver adapter connecting Prisma to the `pg` connection pool (not the default Prisma connection) | [`src/lib/db.ts:1-23`](src/lib/db.ts:1) — `PrismaPg` adapter with connection pool |
| **NextAuth v5** | JWT session management and CSRF protection; no database sessions | [`src/lib/auth.ts`](src/lib/auth.ts) |
| **bcryptjs** | Password hashing (cost 12) and agent token hashing (cost 12 + pepper) | [`src/lib/password.ts`](src/lib/password.ts), [`src/lib/token.ts`](src/lib/token.ts) |
| **Upstash Redis** | HTTP-based Redis for rate limiting; works from Edge and serverless | [`src/lib/ratelimit.ts`](src/lib/ratelimit.ts) |
| **@ai-sdk/groq** | Groq Llama 3.3 70B for narration and feed summaries | [`src/lib/narrate.ts:1`](src/lib/narrate.ts:1), [`src/lib/gemini.ts:1`](src/lib/gemini.ts:1) |
| **ai (Vercel AI SDK)** | `generateText` and `streamText` abstraction layer over AI providers | [`package.json:25`](package.json:25) `"ai": "^6.0.194"` |
| **@react-pdf/renderer** | Server-side PDF generation for executive summary export | [`package.json:23`](package.json:23) |
| **shadcn/ui + Tailwind CSS** | Component library and utility CSS; radix-ui primitives | [`package.json:37`](package.json:37), [`components.json`](components.json) |
| **pino** | Structured JSON logging to stdout with field-level redaction | [`src/lib/logger.ts`](src/lib/logger.ts) |
| **Sentry** | Unhandled exception reporting; `sendDefaultPii: false` across all configs | [`sentry.client.config.ts`](sentry.client.config.ts), [`sentry.server.config.ts`](sentry.server.config.ts) |
| **vitest** | Unit and integration test runner | [`package.json:54`](package.json:54) `"vitest": "^4.1.7"` |
| **Playwright** | End-to-end test infrastructure (devDependency only; not used for PDF generation) | [`package.json:50`](package.json:50) |
| **Coolify** | Self-hosted deployment platform (Caddy reverse proxy, Docker containers, scheduled tasks for cron) | [`notes/axis-pulse-archdoc.md:905-917`](notes/axis-pulse-archdoc.md:905) |
| **ActivityWatch** | Open-source local time-tracking client (not bundled; installed separately by members) | [`notes/axis-pulse-archdoc.md:573`](notes/axis-pulse-archdoc.md:573) |
| **zod** | Runtime schema validation | [`package.json:46`](package.json:46) |

---

## Trade-offs Documented

### Fail-Open Rate Limiting
When `UPSTASH_REDIS_REST_URL` is absent, all rate limiters allow all requests rather than blocking everything. This means a misconfigured production deployment has no rate limiting. The trade-off is documented in [`notes/pending-work.txt:14-18`](notes/pending-work.txt:14): "If either env var is missing OR Redis is unreachable → fail-open (all requests pass)."

### In-Process Metrics and Cooldown State
Narration cooldowns (`cooldowns: Map<string, number>`) and Prometheus metrics (`_routeStore`) are in-process module-level state. They reset on every process restart and do not survive multi-instance scale-out. Source comments acknowledge this: "Acceptable for single-process 0.5x deployment; promoted to Redis in P17." The trade-off accepts operational simplicity over correctness at scale.

### Dual .ts/.mjs Redaction Files
The server (`src/lib/redact.ts`) and client (`lib/redact.mjs`) implement the same 7-rule redaction pipeline in two separate files — one TypeScript, one ESM JavaScript — with no automatic code-sharing. The parity test (`src/tests/redact-parity.test.ts`) is the only safety net. The trade-off accepts sync risk for architectural simplicity: keeping the client script free of TypeScript compilation.

### Cost Ceiling Pricing Mismatch
`src/lib/cost-ceiling.ts` uses Anthropic Claude Sonnet 4.6 pricing (`$3/1M input, $15/1M output`) but the actual model is Groq Llama 3.3 70B (which has different pricing). The `/admin/budget` dashboard is therefore an approximation. This is an implicit trade-off created by the provider switch mid-build.

### Jaccard P-Number Matching at 0.6 Threshold
The similarity threshold is hardcoded at 0.6 and is not tunable via config. The archdoc describes it as "tunable" but the code does not expose it as an env var or config field. A false match shows the wrong P-number; the manager override flow provides the escape hatch.

### Session Version Propagation Delay
Role changes take up to 30 seconds to propagate to live sessions because `_sessionVersionCache` in `withAuthScoped.ts` has a 30-second TTL. A promoted LINE_MANAGER may see stale permissions for up to 30 seconds.

### Snapshot Generation in Request Handler
The GitHub repo snapshot (`lib/repo-context/`) is generated synchronously inside the Next.js request handler — ~9 GitHub API calls, ~2 seconds typical. The archdoc acknowledges: "On timeout → ERROR state + retry button; never blocks event ingestion." This avoids a background worker but means snapshot requests have higher latency than typical API calls.

---

## Technical Debt

The following items are evidenced by source comments, pending-work.txt entries, or migration SQL comments:

1. **Production Redis not provisioned** — Rate limiting is fail-open until `UPSTASH_REDIS_REST_URL` and `UPSTASH_REDIS_REST_TOKEN` are set in Coolify. Documented in [`notes/pending-work.txt:8-37`](notes/pending-work.txt:8).

2. **Coolify deploy not yet wired** — CI/CD webhook to Coolify is pending manual setup. Documented in [`notes/pending-work.txt:89-124`](notes/pending-work.txt:89).

3. **Live GROQ_API_KEY not configured in production** — P13 and P14 acceptance criteria requiring live AI calls (streaming exec summary, time-enriched narration) are marked PENDING because no AI API key is set. Documented in [`notes/pending-work.txt:41-85`](notes/pending-work.txt:41). **Note**: `notes/pending-work.txt:44` still reads "ANTHROPIC_API_KEY not configured in .env" — this is a stale reference to the original provider. The actual runtime uses `GROQ_API_KEY` per `src/lib/narrate.ts:233` and `src/app/api/projects/[id]/summary/route.ts`. The pending-work notes themselves need updating.

4. **Legacy Project.agentTokenHash fallback** — The `AgentToken` table supersedes per-project hashes but `Project.agentTokenHash` columns are intentionally retained. Migration comment: "intentional migration tech-debt, should be retired once all projects have ≥1 AgentToken row." See [`prisma/migrations/20260607080000_per_developer_agent_tokens/migration.sql:7-8`](prisma/migrations/20260607080000_per_developer_agent_tokens/migration.sql:7).

5. **Narration cooldown not Redis-backed** — The `cooldowns: Map<string, number>` in `src/lib/narrate.ts` is in-process. Comment: "Acceptable for single-process 0.5x deployment; promoted to Redis in P17." However P17 was implemented without this promotion — the cooldown remains in-process.

6. **GitHub App webhook secret unused** — `GITHUB_APP_WEBHOOK_SECRET` is provisioned in `.env.example` and the archdoc secrets table, but the archdoc notes "Reserved for future webhook support; not used in v1."

7. **CSP includes api.anthropic.com** — The `script-src` directive in `src/middleware.ts` includes `https://api.anthropic.com` — a holdover from the original Anthropic design. This is unnecessary given the switch to Groq and is a minor CSP scope over-grant.

8. **tenantKey columns superseded by organisationId** — Many tables have both `tenantKey TEXT?` (from the original design) and `organisationId TEXT NOT NULL` (from migration `20260604000001`). The `tenantKey` columns are effectively dead but remain in the schema.

9. **OTel trace store keeps only one trace** — `_lastTrace` in `src/lib/otel.ts` is a single in-memory slot. This was intentionally designed "for /build-evidence capture" and is not a production-grade tracing solution.

10. **Consent version check hardcoded at 2** — [`src/lib/consentCheck.ts:12`](src/lib/consentCheck.ts:12) checks `(consent.consentVersion ?? 1) < 2`. Future consent changes would require a code change to bump this constant.

---

## Future Considerations

The following planned changes, migration paths, or scaling considerations are found in the documentation:

### Multi-Tenancy (Planned, Not Built)
The `Organisation` model and `organisationId` columns on every table form the foundation for multi-tenant SaaS. The archdoc states: "A nullable `tenantKey` column is reserved so a future multi-tenant migration is non-breaking." The actual migration (`20260604000001_add_organisations`) implements this as a non-breaking column promotion — new tenants require only a new `Organisation` row.
**Evidence:** [`prisma/migrations/20260604000001_add_organisations/migration.sql`](prisma/migrations/20260604000001_add_organisations/migration.sql), [`notes/axis-pulse-archdoc.md:29-31`](notes/axis-pulse-archdoc.md:29).

### Redis-Backed Cooldowns and Metrics at 4x
Both the narration cooldown map and the Prometheus metrics store are documented as candidates for Redis promotion at 4x scale. The metrics file comment explicitly says "Promoted to Redis-backed store at 4x if needed."
**Evidence:** [`src/lib/metrics.ts:3`](src/lib/metrics.ts:3), [`src/lib/narrate.ts:19`](src/lib/narrate.ts:19).

### Context Drift Analyser (Feature Planned)
`features/context-drift-analyser/context-drift-user-story.md` describes a feature that monitors whether a project's code has drifted from its context documentation. It uses Groq Llama 3.3 70B with an async PENDING/RUNNING/COMPLETE/ERROR status cycle, GitHub Compare API diffing, and findings stored as structured JSON. The feature is scoped, designed, and ready for implementation but not yet built.
**Evidence:** [`features/context-drift-analyser/context-drift-user-story.md`](features/context-drift-analyser/context-drift-user-story.md).

### Dashboard Sparklines and Activity Feed (Implemented)
`features/DASHBOARD-SPARKLINES/Build.md` planned 7-day sparkline bars on each dashboard stat card and a live activity feed. These ARE implemented: `src/lib/dashboard.ts` defines `SparklineWeek` type and runs two `$queryRaw` sparkline queries; `src/app/dashboard/_components/SparkBars.tsx` renders the bars; `ActivityFeedClient.tsx` renders the live feed. The `DashboardSummary` type includes `sparklines` and `activityFeed` fields.
**Evidence:** [`src/lib/dashboard.ts`](src/lib/dashboard.ts), [`src/app/dashboard/_components/SparkBars.tsx`](src/app/dashboard/_components/SparkBars.tsx).

### Dashboard Redesign (Implemented)
`features/DASHBOARD-REDESIGN/Build.md` planned a `/api/dashboard/summary` endpoint and redesigned stat cards. These ARE implemented: `GET /api/dashboard/summary` exists at `src/app/api/dashboard/summary/route.ts`; `DashboardStatsClient.tsx` renders four KPI cards (Events Today, Active Members, Prompts Matched Today, Active Projects) with sparklines; `StatCard.tsx` is the reusable card component. 30-second polling is active.
**Evidence:** [`src/app/api/dashboard/summary/route.ts`](src/app/api/dashboard/summary/route.ts), [`src/app/dashboard/_components/DashboardStatsClient.tsx`](src/app/dashboard/_components/DashboardStatsClient.tsx).

### Per-Project Daily Token Budget Removal
Migration `20260609000002_remove_daily_token_budget` removed `Project.dailyTokenBudget` (which was `100,000 tokens/day` per the archdoc design). This was superseded by the monthly ceiling approach in P17. A per-developer token budget was added separately (`20260609000003_add_dev_token_budget`).
**Evidence:** Migration files `20260609000002` and `20260609000003`.

### GitHub Webhooks (Reserved, Not Implemented)
`GITHUB_APP_WEBHOOK_SECRET` is in `.env.example` and provisioned in the archdoc secrets table. The archdoc says "Reserved for future webhook support; not used in v1." Webhooks would allow push-triggered snapshot refreshes without polling.
**Evidence:** [`.env.example:12`](.env.example:12), [`notes/axis-pulse-archdoc.md:843`](notes/axis-pulse-archdoc.md:843).

### MFA (Scoped Out)
No MFA is implemented. The archdoc's risk register documents this explicitly: "MFA not implemented — medium likelihood, low impact — mitigated by lockout + rate limit + strong password policy." Documented as v1 scope-out.
**Evidence:** [`notes/axis-pulse-archdoc.md:809`](notes/axis-pulse-archdoc.md:809), [`notes/axis-pulse-archdoc.md:1029`](notes/axis-pulse-archdoc.md:1029).

### 4x Scale Graduation (Partially Complete)
The archdoc specifies a P18 scale graduation to 12–20 teams / 8 managers / ~96–480 projects with index audits, load testing, and Redis tier increases. Migration `20260603110000_p18_scale_indexes` adds indexes for the hot query paths. The full 4x load test and scale-down rehearsal are still pending.
**Evidence:** [`prisma/migrations/20260603110000_p18_scale_indexes/migration.sql`](prisma/migrations/20260603110000_p18_scale_indexes/migration.sql), [`notes/user-story.md:856-889`](notes/user-story.md:856).

### ADR-code-health-001: Code Health — SonarQube CB + Synchronous execSync Scan

**Status:** Accepted

**Context:** Phase 9 adds code health scoring (reliability, security, maintainability ratings, quality gate, coverage, duplication) to the project detail page. The feature must work on a self-hosted Coolify deployment with no serverless function limits.

**Decision:**
1. Use **SonarQube Community Build** (LGPL-3.0 Docker image `sonarqube:community`) as the static analysis engine. Configured via `SONAR_HOST_URL`, `SONAR_SCANNER_HOST_URL`, `SONAR_TOKEN`, `SONAR_PROJECT_KEY` env vars.
2. Scans run **synchronously** via `child_process.execSync` inside the POST scan route. The `sonarsource/sonar-scanner-cli:latest` Docker image is invoked, which mounts the project root and communicates with SonarQube.
3. Scan results are stored in `CodeHealthSnapshot` and the project page **reads from DB only** — zero SonarQube calls in the render path.
4. The Refresh button triggers `POST /api/projects/[id]/code-health/scan`. Rate-limited to 1/120s per project (Upstash sliding window).

**Rationale:** SonarQube CB is LGPL-3.0 and runs entirely on-premise — no data leaves the self-hosted environment. `execSync` is acceptable for a single-process self-hosted deployment with no Vercel 10s limit. Async via a job queue would be over-engineering for the current deployment model.

**Consequences:** A slow scan (up to 5 minutes) blocks the Node.js process during `execSync`. This is acceptable in the current single-project self-hosted context. A future scale-up to many parallel scans would require migrating to an async job queue. The 120-second rate limit prevents concurrent scan accumulation.

**Evidence:** `src/lib/code-health.ts` — `execSync(cmd, { timeout: timeoutMs, stdio: "pipe" })`. `src/app/api/projects/[id]/code-health/scan/route.ts` — synchronous POST handler. `src/lib/ratelimit-code-health.ts` — 1/120s sliding window.

---

### ADR-code-health-002: Central Scan — Model D (server fetches tarball, scans, hard-deletes source)

**Status:** Accepted

**Context:** Phase 9b extends code health to any project with a connected GitHub repo, not just the local axis-pulse codebase. The server must fetch the repo source, scan it with SonarQube, and never retain the source after the scan.

**Decision:**
1. **Server fetches tarball** via `downloadRepoTarball()` using the existing GitHub App installation token (`authedGithubFetch`). The tarball is streamed directly to `/tmp/pulse-scan-{projectId}-{ts}/repo.tar.gz`.
2. **Per-project SonarQube key**: `org-{orgId}-proj-{projectId}`. The `orgId` comes from the authed session context (`ctx.organisationId`), never from the client request.
3. **Auto-provisioning**: `ensureSonarProject()` creates the SonarQube project before first scan using a separate `SONAR_ADMIN_TOKEN`. Idempotent — "already exist" is treated as success.
4. **Hard-delete in `finally`**: `rmSync(tmpDir, { recursive: true, force: true })` runs in a `finally` block so source is always deleted, even on scanner failure.
5. **Multi-tenancy proof**: route verifies `db.githubInstallation.findUnique({ where: { organisationId_installationId: { organisationId: ctx.organisationId, installationId: project.githubInstallationId } } })` → 403 if null. The installation ID comes from a project already fetched with `organisationId: ctx.organisationId`, so no org can claim another org's installation.
6. **Semaphore**: `MAX_CONCURRENT_SCANS = 2` (in-process). Returns 503 if full. Protects the host's Docker daemon from unbounded concurrent container launches.
7. **EDGE path retained**: projects without a GitHub link still use `process.cwd()` + `SONAR_PROJECT_KEY`; `scanSource = "EDGE"`.

**Rejected alternatives:**
- *Model A* (developer runs scanner, pushes results): requires developer-side tooling; no way to verify the scan ran against the declared commit.
- *Model B* (CI/CD pipeline pushes results): requires CI changes in every connected repo.
- *Model C* (clone via git): requires git binary, SSH keys, or PAT; tarball is simpler and avoids git history overhead.

**Evidence:** `src/lib/code-health.ts`, `src/app/api/projects/[id]/code-health/scan/route.ts`, `prisma/migrations/20260615000001_add_code_health_scan_source/migration.sql`.

---

### ADR-security-scan-001: Security Scanning — Local CLI Tools (Not a Cloud SAST Service)

**Status:** Accepted

**Context:** Phase 10 adds a "Needs a Look" security panel to the project detail page, surfacing findings from SAST, secret detection, and dependency vulnerability scanning. The options were: (a) a hosted SAST cloud service (Snyk, SonarCloud), (b) a Docker-based scanner, or (c) locally installed CLI tools invoked directly by the application server.

**Decision:**
1. **Three local CLI tools**: Semgrep CE (`semgrep`), gitleaks (`gitleaks`), and osv-scanner (`osv-scanner`) are invoked directly via `child_process` in `src/lib/security-scan.ts`. No Docker containers are required for the security scan path (unlike the SonarQube code-health scan).
2. **Reuse Phase 9b tarball plumbing entirely**: `downloadRepoTarball`, `extractTarball`, and `tryAcquireScanSlot`/`releaseScanSlot` from `src/lib/code-health.ts` are reused verbatim. The tarball lifecycle (download → extract → scan → `rmSync` in finally) is identical.
3. **Catch-and-continue per tool**: If any tool is not installed or exits with an error, that tool returns 0 findings and the others continue. A missing CLI is not a hard failure.
4. **Severity mapping is tool-specific**: Semgrep ERROR→HIGH, WARNING→MEDIUM, else INFO; gitleaks always CRITICAL; osv-scanner uses `database_specific.severity` (MODERATE→MEDIUM) or CVSS vector heuristic.
5. **`scanState` distinguishes "not scanned yet" from "scanned clean"**: The `ExceptionsPanel` shows different empty states for `"not_scanned"` vs `"scanned_clean"` — honest about whether a scan has ever run (§5.9 honest empty states principle).

**Rationale:** Local CLI tools run entirely on the self-hosted Coolify instance — no data leaves the private infrastructure. No SaaS API keys or per-scan billing. The three selected tools cover complementary attack surfaces: Semgrep (code patterns), gitleaks (secrets in source), osv-scanner (known CVEs in dependencies). Docker was not chosen because the existing SonarQube scan already uses Docker; adding a second Docker invocation path for security was not warranted when the CLI tools provide equivalent coverage.

**Rejected alternatives:**
- *Cloud SAST service*: requires external data transmission and per-scan billing.
- *Docker-based scanner*: would duplicate the existing Docker-in-scan infrastructure from Phase 9b without benefit.
- *Single tool only*: Semgrep alone would miss secrets and dependency CVEs; gitleaks alone would miss code patterns.

**Consequences:** CLI tools must be installed on the server where the Next.js process runs. Missing tools silently produce 0 findings with no error surfaced to the user — a known operational risk (see `docs/risk.md` P22). The semaphore from `src/lib/code-health.ts` is shared with SonarQube scans (`MAX_CONCURRENT_SCANS = 2`), so a simultaneous code-health scan and security scan will contend for slots.

**Evidence:** `src/lib/security-scan.ts`, `src/app/api/projects/[id]/security-scan/route.ts`, `src/lib/ratelimit-security-scan.ts`.

---

### ADR-security-scan-002: Security Findings Merged With Operational Exception Flags, Sorted Worst-First

**Status:** Accepted

**Context:** The project detail page already shows operational exception flags (high risk, cost spike, stall, repo stale) in the "Needs a Look" panel via `ExceptionsPanel`. Security findings from the new scan pipeline need to appear in the same panel without a separate UI section.

**Decision:** Security findings are converted to `ExceptionFlag` objects via `securityFindingToExceptionFlag()` in `src/lib/project-exceptions.ts`, merged with operational flags in `src/app/projects/[id]/page.tsx`, and sorted worst-first by the unified `SEVERITY_WEIGHT` map (CRITICAL:5, HIGH:4, MEDIUM:3, LOW:2, INFO:1). `ExceptionFlag.severity` was extended from `"high" | "medium"` to `"critical" | "high" | "medium" | "low" | "info"` to accommodate the 5-level security severity scale. An optional `source` field identifies the originating tool.

**Rationale:** A single merged and sorted list presents a coherent "things that need attention" view regardless of whether the finding is AI-detected (narration risk) or statically scanned (security tool). The project page server component bears the responsibility for merging and sorting — the panel component remains a pure display layer.

**Evidence:** `src/lib/project-exceptions.ts` (`securityFindingToExceptionFlag`), `src/app/projects/[id]/page.tsx`, `src/app/projects/[id]/_components/ExceptionsPanel.tsx`.

---

### ADR-readiness-001: Readiness signals are per-tool, never aggregated into a single %
**Decision:** The ReadinessWidget shows each signal (markers, coverage, CI, forecast) in its own labelled cell. There is no aggregated "X% ready to ship" or "completion %" of any kind.
**Rationale:** An aggregated readiness % requires a denominator of total scope, which is not measurable from code metrics alone. Any such number would be misleading — a project could have 0 markers but still be missing critical requirements. Each signal is honest about its source and its own limits.
**Evidence:** [`src/app/projects/[id]/_components/ReadinessWidget.tsx`](src/app/projects/[id]/_components/ReadinessWidget.tsx).

### ADR-readiness-002: Forecast requires ≥ 3 snapshots (FORECAST_MIN_SNAPSHOTS = 3)
**Decision:** `computeForecast` returns `{ kind: "insufficient" }` when fewer than 3 snapshots exist. The forecast is never extrapolated from 1 or 2 data points.
**Rationale:** Two points trivially define any line — a "trend" from two points is not a trend, it's the definition of the line through those points. Three points is the minimum for the OLS residual to be non-trivially informative. Any fewer is noise.
**Evidence:** [`src/lib/readiness.ts`](src/lib/readiness.ts) — `FORECAST_MIN_SNAPSHOTS = 3`.

### ADR-readiness-003: Scan root is process.cwd() (LOCAL ONLY, no CI integration in Phase 8)
**Decision:** `scanMarkersInDir` and `readCoveragePct` both use `process.cwd()` as the scan root. CI status (GitHub Actions) and SonarQube quality gate are shown as "not measured yet" in Phase 8.
**Rationale:** This is a single-project self-hosted Coolify deployment. `process.cwd()` is the application root, which is also the codebase root on this deployment. External CI and SonarQube integration are deferred to Phase 9 to avoid scope creep.
**Known risk:** On a container or non-root-mounted deployment, `process.cwd()` may not be the project codebase root — see risk register. Evidence: [`src/app/api/projects/[id]/readiness/scan/route.ts`](src/app/api/projects/[id]/readiness/scan/route.ts).

---

### ADR-security-scan-003: Security Finding Path Normalization (stripSrcPrefix)

**Status:** Accepted

**Context:** When the security scan pipeline extracts a repo tarball to a temp directory, tool output contains absolute paths rooted at that temp dir. Storing these absolute paths in `SecurityFinding.file` would make findings meaningless after the temp dir is deleted and would leak internal server path structure.

**Decision:** After all three tools complete, `runAllSecurityTools` calls `stripSrcPrefix(filePath, srcDir)` on every finding's `file` field. This converts absolute temp-dir paths to repo-relative paths (e.g. `safeai-intelli/security_pwa/index.html`) before the findings are written to the DB. `stripSrcPrefix` removes the leading `srcDir` prefix (normalised to forward slashes) and any leading slash from the result. If the file path does not start with `srcDir`, it is left unchanged.

**Rationale:** Repo-relative paths are stable across re-scans and deployments, match what developers see in their editor, and do not reveal server directory structure. The conversion is lossy (absolute path is gone) but intentionally so — the temp dir is deleted immediately after the scan anyway.

**Evidence:** `src/lib/security-scan.ts` (stripSrcPrefix, runAllSecurityTools), `src/app/api/projects/[id]/security-scan/route.ts`.

---

### ADR-security-scan-004: GNU tar Windows Compatibility (try-with-fallback extraction)

**Status:** Accepted

**Context:** The tarball extraction step uses `child_process.execSync("tar ...")` on Windows. Two tar implementations are in use depending on how the server process was started: (1) GNU tar 1.35 (from Git Bash / MinGW in PATH), which requires `--force-local` to prevent drive letters (C:) being interpreted as remote hostnames; (2) Windows built-in bsdtar (`C:\Windows\System32\tar.exe`), which does NOT support `--force-local` but handles Windows drive-letter paths natively. Statically detecting which tar is present at module load time is unreliable because Next.js HMR does not reinitialize module-level IIFEs on hot reload.

**Decision:** Both paths are normalised to forward slashes (C:\path → C:/path) before being passed to tar. The extraction first attempts `tar --force-local -xf ...`; if that throws an error containing "--force-local is not supported" (bsdtar's exact message), it retries with `tar -xf ...`. All other errors are re-thrown. Tested: GNU tar path PASS, bsdtar path PASS (bsdtar 3.8.4 libarchive).

**Rationale:** A try-with-fallback avoids module-level state (which has HMR caching issues in Next.js dev mode) and works correctly regardless of which tar binary is in the server process's PATH. The one extra execSync call on bsdtar is negligible — it runs only on the first failure, and tarball extraction is fast compared to the GitHub download that precedes it.

**Evidence:** `src/lib/code-health.ts` (extractTarball).

---

## ADR-spend-lm: LINE_MANAGER Spend Visibility in Member Detail Card

**Status:** Accepted  
**Date:** 2026-06-16

**Full ADR:** `features/member-detail-card/adr-spend-visibility.md`

**Decision:** LINE_MANAGER can see spend (USD + tokens) for their own team members via `GET /api/admin/members/[id]/card`. This widens spend visibility beyond the previous MANAGER-only access on the cost-dashboard. Spend is omitted server-side (key absent from response) when `canSeeSpend=false`; this flag is computed from `ctx.role` on the server and never read from the request.

**Evidence:** `src/lib/member-card.ts` (`getMemberCard`, `canSeeSpend` param); `src/app/api/admin/members/[id]/card/route.ts` (role gate); `src/tests/member-card.test.ts` (`"spend" in result!` assertion); `src/tests/member-card-route.test.ts` (cross-org + cross-team 403/404 proofs).

---

## ADR-member-card-dual-tz: Dual-Timezone Windowing in Member Detail Card

**Status:** Accepted  
**Date:** 2026-06-16

**Context:** The member detail card has two logically distinct data categories that require different timezone references for windowing:
1. **Time-at-keyboard (MemberDailyTime):** rows represent the member's local calendar day, stored as UTC midnight. A member in Asia/Dubai working on "June 17 locally" stores `day = 2026-06-17T00:00:00Z`. Filtering by viewer UTC would return the wrong day.
2. **Event-based data (spend, sessions, commits, projects):** these are `ingestedAt` timestamps. The viewer wants to see "what happened today from my perspective," matching the existing `resolveTimeWindow` pattern on the project page.

**Decision:** Two separate windowing mechanisms are applied and must not be cross-applied:

- **Time section:** `computeMemberTimeRange(window, memberTz)` uses `Intl.DateTimeFormat("en-CA", { timeZone: memberTz })` to derive the member's local calendar date as UTC midnight. `memberTz` is read from `MemberDailyTime.findFirst({ orderBy: { day: "desc" } })` — the most recent row's timezone. Falls back to UTC if no rows exist. This is DAY-ROW SUMMING, not an event cutoff.

- **Event sections:** `resolveTimeWindow(rawWindow, viewerTodayMs).start` produces a `since` Date in the viewer's timezone. Applied as `ingestedAt: { gte: since }` on spend, sessions, commits, and projects queries. This is an EVENT CUTOFF, not day-row summing.

- **lastSeenAt:** NOT windowed. The `_max: { ingestedAt }` aggregate for identity has no `ingestedAt` filter — it answers "last seen ever."

**Security invariant:** The `window` and `viewerTodayMs` params are time-narrowing only. They are applied as additive `where` clause filters on top of the existing `organisationId + userId + role/team` gate. A malicious or forged `window` param can only reduce the data returned, never expose data from another user, team, or org.

**Evidence:** `src/lib/member-card.ts` (`computeMemberTimeRange`, `resolveTimeWindow` import, Phase 1 `latestTzRow` lookup); `src/tests/member-card.test.ts` (tz-windowing describe block with Dubai/NY fake-timer edge cases); `src/tests/member-card-route.test.ts` (window param forwarding tests).
