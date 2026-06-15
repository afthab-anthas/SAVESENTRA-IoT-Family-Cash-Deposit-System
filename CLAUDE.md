# SaveSentra — CLAUDE.md

IoT-based Family Cash Deposit System with ML Predictive Analytics (UAE Dirhams / AED).

## Project Structure

| Path | Purpose |
|------|---------|
| `savesentra_iot/savesentra_iot.ino` | Production ESP32 firmware (main device) |
| `Development_Sketches/` | Iterative dev sketches (not production) |
| `rpi_server.py` | Flask server running on Raspberry Pi |
| `ml/savesentra_ml.py` | Full 8-step ML pipeline (called by rpi_server) |
| `ml/steps/` | Step-by-step breakdown of ML pipeline stages |
| `Helpers/generate_dataset.py` | Synthetic dataset generator for testing |
| `ml/savings_dataset.csv` | Live transaction ledger (CSV) |

## System Architecture

1. **ESP32 Edge Device** — RFID auth → stepper motor intake → IR sensor measures note length → denomination detected (5 AED: 4990–5080 steps, 10 AED: 5090–5200 steps) → POSTs to Pi
2. **Raspberry Pi Server** (`rpi_server.py`) — receives `/deposit` POST, appends to CSV, triggers ML script via subprocess; `/predict` endpoint reruns ML on goal change
3. **ML Pipeline** (`savesentra_ml.py`) — cleans CSV, engineers temporal features, trains SVR (vs RF vs GBM), simulates future deposits day-by-day, pushes predicted goal date + top contributor to Blynk via HTTPS

## Key Constants

- Blynk Auth Token: set in `.ino` sketch — do not commit; store in env or local config
- Default family goal: 1000 AED (configurable from app via V7)
- Pi server URL: `http://raspberrypi.local:5000`
- NTP timezone: UTC+4 (UAE)

## Blynk Virtual Pins

| Pin | Role |
|-----|------|
| V0 | Name input (register user) |
| V1 | Status label |
| V3 | Reset all users button |
| V4 | Step counter display |
| V5 | Transaction log terminal |
| V6 | Individual balance gauge |
| V7 | Family goal slider (admin) |
| V8 | Delete selected user (admin) |
| V10 | Total family savings gauge |
| V11 | Goal % contribution bar |
| V12 | Logout button |
| V16 | Predicted goal date (from ML) |
| V17 | Predicted top contributor UID (from ML) |
| V20 | User selection menu |
| V21 | Role dropdown (Parent/Child/Other) |
| V30 | Admin-only hidden widget |

## Context Navigation (Graphify)

### 3-Layer Query Rule
1. **First:** query `graphify-out/graph.json` or use `/graphify query "<question>"`
2. **Second:** query the Obsidian vault (`~/vault/SAVESENTRA-IoT-Family-Cash-Deposit-System/`) for decisions and context
3. **Third:** only read raw code files when editing or when the first two layers don't answer

### When to rebuild the graph
- After structural changes (new modules, major refactors)
- Command: `/graphify . --update` (only processes modified files)
- First run requires: `$env:ANTHROPIC_API_KEY = "sk-..."` then `graphify . --obsidian --obsidian-dir ~/vault/graphify/SAVESENTRA-IoT-Family-Cash-Deposit-System`

### Vault project folder
- `~/vault/SAVESENTRA-IoT-Family-Cash-Deposit-System/` — decisions, architecture notes, session logs
