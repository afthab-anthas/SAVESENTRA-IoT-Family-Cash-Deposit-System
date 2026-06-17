alwaysApply: true

# .cursorrules - SAVESENTRA-IoT-Family-Cash-Deposit-System

This .cursorrules file serves as the AI assistant knowledge index for context-first development.
Whenever working in Cursor, the following documents must be preloaded and treated as the base architectural context.

---

## Mandatory Reference Documents

These documents are the single source of truth for all development decisions:

- [docs/architecture.md](docs/architecture.md) - System architecture, technology stack, application structure, API routes, authentication, database models, observability
- [docs/structure.md](docs/structure.md) - Project organization, folder structure, file layout, entry points, module dependencies
- [docs/code.md](docs/code.md) - Code patterns, conventions, naming standards, key functions, critical code paths, testing patterns
- [docs/dataflow.md](docs/dataflow.md) - Data models, ingest pipelines, authentication flows, consent management, external integrations
- [docs/decisions.md](docs/decisions.md) - Architectural decisions and rationale (reference for understanding why patterns exist)
- [docs/glossary.md](docs/glossary.md) - Domain terminology and definitions (reference for consistent language)
- [docs/risk.md](docs/risk.md) - Security risks, performance risks, scalability issues, technical debt, compliance gaps, mitigations

---

## Purpose: Why These Docs Are Mandatory

These seven documents collectively describe:

1. **Architecture & Design**: System overview, technology stack (Next.js 14, TypeScript, Prisma, PostgreSQL), design patterns, API structure
2. **Coding Standards**: File organization, naming conventions, TypeScript patterns, error handling, validation, testing approach
3. **Data Flows**: Ingest pipelines, authentication flows, token resolution, consent management, narration pipeline, external API integrations
4. **Security & Risk**: Authentication/authorization, token security, rate limiting, PII handling, multi-tenancy isolation, known vulnerabilities
5. **Operational Context**: Database schema, indexes, migrations, deployment, monitoring, observability

**Before developing any feature, debugging any issue, or refactoring any code, you MUST:**
- Read the relevant section(s) of these docs
- Verify your understanding against the actual source code
- Ensure your changes align with documented patterns and decisions
- Update the docs as part of your completion

---

## Cursor Development Rules

### 1. Preload Context First
- Parse all seven mandatory docs before making any recommendations
- Treat the docs as authoritative for architecture, standards, dataflows, and security
- Do not assume details; verify against the docs and source code

### 2. Single Source of Truth
- The docs are the canonical reference for:
  - Architecture decisions (why Next.js App Router, why Prisma, why Groq)
  - Coding patterns (naming conventions, error handling, validation)
  - Data flows (ingest pipeline, token resolution, narration)
  - Security boundaries (RBAC, multi-tenancy, token handling)
  - Performance constraints (rate limits, caching, polling)

### 3. Cross-Check Ambiguous Details
- When code behavior is unclear, verify against docs/architecture.md or docs/dataflow.md
- If docs and code diverge, flag the discrepancy (docs may be stale)
- Never assume API behavior; check docs/architecture.md API Routes section

### 4. Update Docs as Part of Completion
- All new features must update the relevant docs:
  - New API routes → update docs/architecture.md API Routes table
  - New database models → update docs/dataflow.md Data Models section
  - New code patterns → update docs/code.md Key Functions Reference
  - New security considerations → update docs/risk.md
  - New decisions → update docs/decisions.md
- Features are only "done" when both code AND docs are updated

### 5. TODO Management
- Break down tasks into atomic steps using [ ], [x], [-] markers
- Track progress explicitly: [ ] = pending, [x] = complete, [-] = blocked/deferred
- Link each step to the relevant doc section for context

### 6. No Hallucination
- Only suggest APIs, classes, functions, or logic that exist in the repo or are documented in /docs
- Never invent new patterns; use existing patterns from docs/code.md
- When uncertain, search the codebase or ask for clarification

### 7. Verification-First Approach
- Use CodeTools-search_code or CodeTools-read_file before assuming details
- Verify function signatures against actual source files
- Check database schema against prisma/schema.prisma
- Confirm API routes against src/app/api/ structure

### 8. Consistency Required
- All changes must align with:
  - Architecture patterns in docs/architecture.md
  - Code conventions in docs/code.md
  - Data flow patterns in docs/dataflow.md
  - Security boundaries in docs/risk.md
- Deviations require explicit justification and doc updates

### 9. Security Checks
- Analyze every change for:
  - Token usage (agent tokens, user tokens, session tokens)
  - Secrets management (env vars, bcrypt, HMAC)
  - Multi-tenancy isolation (organisationId scoping)
  - Authentication/authorization (RBAC, role checks)
  - Rate limiting (Upstash Redis, in-memory fallbacks)
  - PII handling (redaction, consent, logging)
- Reference docs/risk.md for known vulnerabilities

### 10. Performance Checks
- Evaluate every change for:
  - Database query efficiency (indexes, N+1 queries)
  - Rate limiting impact (per-user, per-project, per-IP)
  - AI API costs (narration, summaries, token usage)
  - Caching strategy (session version cache, GitHub token cache, cooldowns)
  - Observable performance impact (latency, throughput)
- Reference docs/architecture.md Rate Limiting and Observability sections

---

## Project-Specific Verification Rules

### Technology Stack
- **Frontend**: Next.js 14 (App Router), React 18, TypeScript 5, Tailwind CSS, shadcn/ui
- **Backend**: Node.js, Next.js API routes, Prisma 7.8 ORM
- **Database**: PostgreSQL with @prisma/adapter-pg connection pool
- **Authentication**: NextAuth v5 (JWT strategy, no OAuth providers)
- **AI Integration**: Groq API (llama-3.3-70b-versatile), Vercel AI SDK
- **External APIs**: GitHub App (REST API), SonarQube (REST API), Upstash Redis (rate limiting)
- **Testing**: Vitest 4, @playwright/test for E2E
- **Observability**: Sentry (error tracking), Pino (structured logging), Prometheus (metrics), custom OTel tracing

### Code Quality Standards

#### TypeScript
- Strict mode enabled; no `any` types without explicit justification
- Use `satisfies` sparingly; prefer `as const` for string literal arrays
- Route handlers always return `NextResponse` (or `Response` for streaming/PDF)
- DB query results from `$queryRaw` return `BigInt`; wrap in `Number()` before arithmetic
- Use `void` deliberately to suppress floating promises on fire-and-forget calls
- Discriminated unions for token resolution results: `{ valid: true; ... } | { valid: false }`

#### Naming Conventions
- Functions: camelCase (`generateNarration`, `resolveAgentToken`, `computeVerdict`)
- Types/interfaces: PascalCase (`AuthContext`, `ResolvedToken`, `VerdictResult`)
- Constants: UPPER_SNAKE_CASE (`BCRYPT_COST`, `COOLDOWN_MS`, `MAX_BODY_BYTES`)
- Internal/test helpers: prefix with underscore (`_resetStatusRateLimitStore`, `_checkLoginMemRateLimit`)
- Rate limiter modules: `ratelimit-<endpoint>.ts`; Upstash prefixes: `pulse:<resource>:<operation>`

#### File Organization
- Library utilities in `src/lib/` as flat `.ts` files (no barrel `index.ts` unless subdirectory warrants it)
- API routes at `src/app/api/<resource>/route.ts`; dynamic segments use `[id]` directories
- Tests mirror subjects: `src/tests/<name>.test.ts` (no `__tests__` directories)
- Generated Prisma client: `src/generated/prisma` (not `node_modules`)

#### Error Handling
- Route handlers wrap main logic in `try/catch`; catch block logs via `console.error` with `[route]` tag
- Pure library functions catch internally and return gracefully (never propagate)
- Zod validation uses `.safeParse()` (never `.parse()`)
- Database errors that may fail legitimately are caught specifically with 500 response
- Ingest routes use `respond()` helper to ensure `endSpan` and `recordRequest` always called

#### Validation Patterns
- Zod for API request bodies (`BodySchema.safeParse(body)`)
- Direct body casting for simple partial fields before Zod full validation
- Field-level guards: `typeof x === "string" && x.trim()`
- Email validation: `z.string().email()`
- Enum validation: explicit `const` arrays with `.includes()`
- Day format validation: `/T00:00:00(\.000)?Z$/` regex

#### Testing
- All tests use Vitest (`describe`, `it`, `expect`, `vi`)
- Module mocks: `vi.mock("@/lib/db", () => ({ db: { model: { method: vi.fn() } } }))`
- Hoisted mocks: `vi.hoisted(() => vi.fn())` for mocks needed before imports
- Test isolation: `beforeEach` clears mocks and/or resets in-memory stores
- Rate limiter tests: exhaust limits in a loop, verify 429 on overflow
- Multi-tenant tests: verify cross-org/cross-team access returns 403/404, not 200

### Development Workflow

#### Branch Strategy
- Main branch: `main` (production-ready)
- Feature branches: `feature/<description>` or `fix/<description>`
- All PRs require passing CI (lint, type-check, tests, build)

#### Commit Standards
- Atomic commits: one logical change per commit
- Commit messages: `<type>(<scope>): <description>` (e.g., `feat(ingest): add P-number matching`)
- Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `chore`
- Reference issue numbers: `fixes #123`

#### PR Requirements
- Title: clear, descriptive, references issue if applicable
- Description: what changed, why, any breaking changes
- Tests: new tests for new code, updated tests for changed code
- Docs: update relevant docs in `/docs` folder
- No debug statements, console.logs, or commented-out code

### Technology-Specific Guidelines

#### Next.js 14 App Router
- Use Server Components by default; mark Client Components with `"use client"`
- Leverage `async` Server Components for data fetching
- Use `next/navigation` for client-side routing
- Middleware at `src/middleware.ts` for cross-cutting concerns (CSRF, auth, security headers)
- API routes at `src/app/api/` with `route.ts` handlers

#### Prisma & PostgreSQL
- Use `@prisma/adapter-pg` for connection pooling
- All queries include `organisationId` scoping for multi-tenancy
- Indexes defined in schema for hot queries (see docs/dataflow.md Database Indexes)
- Migrations in `prisma/migrations/` with descriptive names
- Generated client output to `src/generated/prisma`

#### Authentication & Authorization
- Session auth via NextAuth v5 JWT strategy
- Bearer token auth for agent/user tokens (bcrypt verification)
- RBAC: MANAGER (org-level), LINE_MANAGER (team-level), MEMBER (team-level, limited)
- All protected routes call `withAuthScoped()` first
- Session version drift detection for forced re-login on role changes

#### Rate Limiting
- Prefer Upstash Redis sliding window when configured
- Fall back to in-memory `Map<string, number[]>` for dev/test
- Fail-open on Upstash errors: `{ allowed: true }`
- Scale tier multiplier: `SCALE_TIER` env var (0.5x, 1x, 4x)

#### AI Integration (Groq)
- Model: `llama-3.3-70b-versatile`
- Provider: `@ai-sdk/groq` (Vercel AI SDK)
- Narration: `generateText()` with structured JSON prompt
- Feed summary: `generateText()` with action-phrase prompt
- Executive summary: `streamText()` with Server-Sent Events
- Delta gating: skip if input hash matches latest Intelligence row
- Cost ceiling: block narration if monthly spend ≥ `CLAUDE_MONTHLY_CEILING_USD`

#### External APIs
- **GitHub App**: RS256 JWT auth, installation token caching (~50 min), host allowlist enforcement
- **SonarQube**: REST API for metrics, Docker container for scanner, semaphore for concurrent scans (max 2)
- **Security scanners**: Semgrep CE, gitleaks, osv-scanner (catch-and-continue on missing tools)

### Security Requirements

#### Authentication
- Passwords: bcrypt cost 12, min 12 chars, require letter + digit + symbol
- Account lockout: 5 failed attempts → 10 min lock
- Session tokens: 30-day JWT, HttpOnly, SameSite=Lax, Secure on HTTPS
- Session version drift: invalidate on role/team changes (force re-login)

#### Authorization
- All API routes: call `withAuthScoped()` first
- All DB queries: include `organisationId` in `where` clause
- Cross-team access: return 403 (not 404) to avoid ID enumeration
- RBAC: enforce role checks before resource access

#### Token Security
- Agent tokens: bcrypt(PEPPER + raw, 12), preview (last 4 chars) for fast lookup
- User tokens: same bcrypt scheme, per-user unique
- Grace window: support token rotation with 10-min overlap
- HMAC signing: HMAC-SHA256(rawToken, rawBody) on ingest routes (constant-time compare)

#### Data Protection
- Redaction: 7 rules (env vars, JWTs, API keys, PEM keys, URL creds) applied to sessionExcerpt/manualText/gitSummary
- PII logging: pino redact config covers 20+ sensitive paths
- Consent: v1 (aggregate only) vs v2 (includes app names); re-consent required for v2
- Secrets: never logged, never exposed in responses, never stored in plain text

#### Rate Limiting
- Ingest event: 60/min per token (240/min at 4x)
- Login: 5/min + 20/hr per IP (dual window)
- Summary: 10/hr per project (100/hr at 4x)
- Export: 20/hr per project (200/hr at 4x)
- Code health scan: 1/120s per project
- Security scan: 1/120s per project

#### HTTP Security
- HSTS: max-age=63072000; includeSubDomains
- X-Content-Type-Options: nosniff
- Referrer-Policy: strict-origin-when-cross-origin
- CSP: nonce-based with strict-dynamic, unsafe-inline for styles (known gap)
- Missing: X-Frame-Options (known gap), Permissions-Policy (known gap)

#### Multi-Tenancy
- Single-org deployment in v1; multi-tenancy architected but not activated
- All models carry `organisationId` (non-nullable FK)
- All queries scoped by `organisationId` via `orgWhere(ctx)`
- Reserved `tenantKey` columns for future multi-tenant migration
- Cross-tenant access prevented at API layer (404 on org mismatch)

### Performance Guidelines

#### Frontend Metrics
- No explicit FCP/LCP/TTI targets documented; use browser DevTools
- Avoid N+1 queries in Server Components
- Use `Promise.all()` for parallel data fetches
- Lazy-load components with `React.lazy()` and `Suspense`

#### Backend Response Times
- API routes: target <500ms for 95th percentile (p95)
- Database queries: use indexes (see docs/dataflow.md)
- Rate limiting: fail-open on Redis unavailable (no blocking)
- Narration: fire-and-forget (don't block ingest response)

#### Optimization Techniques
- Database: indexes on (projectId, ingestedAt), (userId, ingestedAt), (organisationId)
- Caching: session version cache (30s TTL), GitHub token cache (~50 min), narration cooldown (90s)
- Polling: code-health scan polling every 5s, timeout 10 min
- Batching: Promise.all() for parallel queries, no sequential DB calls

#### Observable Performance Impact
- Metrics endpoint: `/api/metrics` (Prometheus text format)
- Latency histogram: 11 buckets (0.005s to 10s)
- Request counter: per route/method/status_class
- Claude token counter: per project (input + output)
- OTel traces: 5 spans per ingest call (ingest, redact, gate, ai, store)

---

## Verification Checklist

### Before Committing Code

- [ ] **Compilation**: `npm run build` succeeds (no TypeScript errors)
- [ ] **Linting**: `npm run lint` passes (ESLint, no warnings)
- [ ] **Formatting**: code follows naming conventions (camelCase functions, PascalCase types, UPPER_SNAKE_CASE constants)
- [ ] **No debug statements**: no `console.log()`, `console.error()`, or commented-out code
- [ ] **Type safety**: no `any` types without explicit justification
- [ ] **Error handling**: all async operations wrapped in try/catch or `.catch()`
- [ ] **Tests updated**: new code has tests, changed code has updated tests
- [ ] **Docs updated**: new features update relevant docs in `/docs`

### Before Submitting PR

- [ ] **Tests pass**: `npm test` succeeds (all Vitest suites pass)
- [ ] **Test coverage**: new code has ≥80% coverage (check via `npm test -- --coverage`)
- [ ] **Manual testing**: feature tested locally in dev environment
- [ ] **Edge cases**: tested with empty data, null values, boundary conditions
- [ ] **Backwards compatibility**: no breaking changes to existing APIs
- [ ] **Database migrations**: new schema changes have Prisma migrations
- [ ] **Security review**: checked for token leaks, PII exposure, auth bypasses
- [ ] **Performance review**: no N+1 queries, no blocking operations, rate limits respected

### Documentation Requirements

- [ ] **README**: updated if setup/installation changed
- [ ] **API docs**: new routes documented in docs/architecture.md API Routes table
- [ ] **Data flow docs**: new data flows documented in docs/dataflow.md
- [ ] **Code patterns**: new patterns documented in docs/code.md Key Functions Reference
- [ ] **Architecture docs**: new design decisions documented in docs/decisions.md
- [ ] **Risk docs**: new security/performance risks documented in docs/risk.md
- [ ] **CHANGELOG**: entry added for user-facing changes

### Security Verification

- [ ] **Input validation**: all user inputs validated with Zod or explicit checks
- [ ] **Authentication**: protected routes call `withAuthScoped()` first
- [ ] **Authorization**: RBAC checks enforce role-based access
- [ ] **Token handling**: agent/user tokens verified via bcrypt, HMAC-signed on ingest
- [ ] **Multi-tenancy**: all queries include `organisationId` scoping
- [ ] **Secrets**: no hardcoded secrets, no env vars logged, no tokens in responses
- [ ] **Rate limiting**: endpoints respect rate limit checks (Upstash or in-memory)
- [ ] **PII handling**: sensitive data redacted before logging/storage
- [ ] **Dependency vulnerabilities**: `npm audit` passes (no high/critical vulnerabilities)

### Performance Validation

- [ ] **Database queries**: use indexes, no N+1 patterns, parallel fetches via `Promise.all()`
- [ ] **Rate limiting**: ingest/summary/export limits enforced
- [ ] **Caching**: session version cache, GitHub token cache, narration cooldown working
- [ ] **Polling**: code-health polling respects timeout, doesn't block other requests
- [ ] **AI costs**: narration delta-gated, cost ceiling enforced
- [ ] **Observable impact**: latency <500ms p95, no memory leaks, metrics exposed

### Code Review Checklist

- [ ] **Naming**: functions camelCase, types PascalCase, constants UPPER_SNAKE_CASE
- [ ] **Maintainability**: code is readable, functions <50 lines, complex logic commented
- [ ] **Error handling**: all error paths handled, no silent failures
- [ ] **Logging**: errors logged with context, no PII in logs
- [ ] **Testing**: unit tests for pure functions, integration tests for routes
- [ ] **Consistency**: follows patterns from docs/code.md, aligns with architecture.md
- [ ] **Security**: no auth bypasses, no token leaks, multi-tenancy respected
- [ ] **Performance**: no blocking operations, rate limits respected, caching used

### Pre-Deployment

- [ ] **CI/CD pass**: GitHub Actions workflow succeeds (lint, type-check, tests, build)
- [ ] **Staging tested**: feature tested in staging environment (if available)
- [ ] **Database migrations**: all Prisma migrations applied successfully
- [ ] **Rollback plan**: documented how to rollback if deployment fails
- [ ] **Env vars**: all required env vars documented in `.env.example`
- [ ] **Secrets**: no secrets committed, all secrets in env vars or secrets manager
- [ ] **Monitoring**: new endpoints/features have observable metrics

### Post-Deployment

- [ ] **Health checks**: `/api/health` endpoint returns 200
- [ ] **Error rates**: Sentry error rate normal (no spike)
- [ ] **Performance metrics**: latency p95 <500ms, no degradation
- [ ] **Rate limiting**: rate limit counters working (Upstash or in-memory)
- [ ] **Logs**: Pino logs flowing, no PII visible
- [ ] **Database**: no migration failures, schema matches Prisma schema
- [ ] **Feature verification**: manual smoke test of new feature

---

## Critical Rules (Non-Negotiable)

1. **PRESERVE all existing documentation links** at the top of this file - do NOT remove or modify them
2. **CONTEXT FIRST**: Always read the relevant docs before making recommendations
3. **SINGLE SOURCE OF TRUTH**: Treat docs as authoritative; verify code against docs
4. **DOCS + CODE**: Features are only "done" when both code AND docs are updated
5. **NO HALLUCINATION**: Only suggest APIs/patterns that exist in repo or docs
6. **VERIFICATION**: Use CodeTools to verify details before assuming
7. **CONSISTENCY**: All changes must align with architecture, code, and data flow docs
8. **SECURITY**: Analyze every change for token/secret/auth/multi-tenancy risks
9. **PERFORMANCE**: Evaluate every change for DB efficiency, rate limits, AI costs
10. **ENTERPRISE-GRADE**: Always propose solutions that are tenant-aware, secure, and observable

---

## Success Criteria

✓ Cursor operates with full context from all seven mandatory docs  
✓ All development decisions rely on documented patterns and decisions  
✓ Features are only "done" when both code and docs are updated  
✓ No unsupported assumptions; all context verified across docs and codebase  
✓ Enterprise-grade, tenant-aware, performance-conscious solutions always proposed  
✓ Security and risk considerations explicitly addressed for every change  
✓ Verification checklists completed before commit/PR/deployment  
```

This enhanced .cursorrules file:

1. **Preserves all existing documentation links** at the top
2. **Explains the purpose** of mandatory docs (collectively describe architecture, standards, dataflows, security)
3. **Adds 10 Cursor Development Rules** (preload context, single source of truth, cross-check, update docs, TODO management, no hallucination, verification-first, consistency, security checks, performance checks)
4. **Adds Project-Specific Verification Rules** based on actual codebase:
   - Technology stack (Next.js 14, TypeScript, Prisma, PostgreSQL, Groq, GitHub API, SonarQube)
   - Code quality standards (TypeScript patterns, naming conventions, file organization, error handling, validation, testing)
   - Development workflow (branch strategy, commit standards, PR requirements)
   - Technology-specific guidelines (Next.js, Prisma, Auth, Rate Limiting, AI, External APIs)
   - Security requirements (authentication, authorization, tokens, data protection, rate limiting, HTTP security, multi-tenancy)
   - Performance guidelines (frontend metrics, backend response times, optimization techniques, observable impact)
5. **Adds comprehensive Verification Checklists** for:
   - Before committing code (compilation, linting, formatting, tests, docs)
   - Before submitting PR (tests, coverage, manual testing, edge cases, backwards compatibility, migrations, security, performance)
   - Documentation requirements (README, API docs, data flow, code patterns, architecture, risk, CHANGELOG)
   - Security verification (input validation, auth, RBAC, tokens, multi-tenancy, secrets, rate limiting, PII, dependencies)
   - Performance validation (DB queries, rate limiting, caching, polling, AI costs, observability)
   - Code review checklist (naming, maintainability, error handling, logging, testing, consistency, security, performance)
   - Pre-deployment (CI/CD, staging, migrations, rollback, env vars, secrets, monitoring)
   - Post-deployment (health checks, error rates, performance, rate limiting, logs, database, feature verification)
6. **Adds Critical Rules** (non-negotiable enforcement)
7. **Adds Success Criteria** (enterprise-grade, tenant-aware, performance-conscious)

All rules are based on **actual code patterns** found in the repository and documented in the reference docs.