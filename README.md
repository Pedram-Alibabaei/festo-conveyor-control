# ESEP SoSe 2026 — Team 3-3

Steuerungssoftware für die **Festo-Förderanlage** (QNX Neutrino, C++14).  
Zwei gekoppelte Teilanlagen sortieren Werkstücke und übergeben sie per Netzwerk.

| Anlage | Rolle | Besonderheit |
|--------|-------|--------------|
| **Anlage 1** (`-s`) | Vorn / Master | Sortierweiche → Rutsche |
| **Anlage 2** (`-c`) | Hinten / Slave | Auswerfer → Bandende, Netzwerk-Handover |

---

## Projektstruktur

```
esep-2026sose-team-3-3/
├── README.md
├── .gitignore
└── esep-SoSe26-3-3/              # QNX-Momentics-Projekt
    ├── Makefile
    ├── .project / .cproject      # Eclipse CDT
    ├── diagrams/                 # UML-Zustandsdiagramme (UMLET)
    ├── manual_tests/             # Hardwarenahe Einzeltests
    ├── test/                       # Google-Test / GMock
    └── src/
        ├── esep-SoSe26-3-3.cpp   # main(): Threads, Init, Start
        ├── shared/               # types, Events, IPC
        ├── hal/                  # Hardware-Abstraktion (Sensoren/Aktoren)
        ├── dispatcher/           # Zentrale Nachrichten- & HW-Schnittstelle
        ├── connectionManager/    # Heartbeat, Partner-Sync, Connection Lost
        ├── calibrationManager/   # Kalibrierdaten persistent speichern/laden
        ├── timer/                # Belt-/Sorter-Timer
        └── logik/                  # FSM, Werkstücke, Fehlerbehandlung
            ├── logik.cpp/hpp
            ├── eventManager/       # Events → FSM
            ├── errorHandler/       # Timeouts, STUCK, Eskalation → ERROR
            ├── workpieceManager/   # Digitales Schieberegister (max. 2 WPs A2)
            └── fsm/
                ├── Fsm.cpp         # Root-FSM
                ├── common/         # BaseState
                ├── level0/         # Safety & Connectivity
                │   ├── ActiveRegionState
                │   ├── ErrorState, EStopState
                │   ├── ConnectionLostState, ConnectionLostAndEStopState
                │   ├── Level0Actions
                │   └── estop/      # ESTOP-Unterzustände
                └── level1/         # Betriebszustände (nur in ActiveRegion)
                    ├── Idle, ReadyForService, ReadyForRun
                    ├── Running, Service
                    ├── L1StateRegistry (Deep History H*)
                    ├── Level1Actions
                    └── service/      # Service-Kalibrierungs-Sub-FSM
```

---

## Architektur (Kurzüberblick)

```
Sensoren/HW ──► HAL ──► Dispatcher ◄──► ConnectionManager
                           │
                           ▼
                      EventManager ──► FSM (Ebene 0 + 1)
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        WorkpieceManager  ErrorHandler  CalibrationManager
```

**Datenfluss:** Hardware-Ereignisse (Pulse/ADC) → `Dispatcher` → `EventManager` → FSM.  
Der `WorkpieceManager` trackt Werkstücke tickbasiert anhand kalibrierter Segmentzeiten.

---

## FSM — Ebene 0 (Safety & Netz)

| Zustand | Zweck | Rückkehr |
|---------|-------|----------|
| **ActiveRegion** | Normalbetrieb, Ebene 1 inside | — |
| **ErrorState** | Fehler quittieren → START | **H\*** (letzter L1-Zustand) |
| **EStopState** | Not-Aus (lokal + Partner) | **Immer Idle** (kein H*) |
| **ConnectionLost** | Netz weg, WPs bleiben | **H\*** bei RESTORED |
| **ConnectionLost+EStop** | Netz weg + ESTOP | H\* oder EStop |

Diagramme: `esep-SoSe26-3-3/diagrams/Level0FSM.uxf`, `Start.uxd`

---

## FSM — Ebene 1 (Betrieb)

| Zustand | Beschreibung |
|---------|--------------|
| **Idle** | Anlage bereit, keine Förderung |
| **ReadyForService / ReadyForRun** | START-Vorstufen (kurz/lang) |
| **Running** | Sortierbetrieb, Werkstück-Tracking |
| **Service** | Kalibrierung (Slow + Normal Lauf) |

Service-Sub-FSM: `diagrams/ServiceCalibFSM.uxf`  
Ebene 1: `diagrams/ActiveLevel1.uxd`

---

## Build & Start

**Voraussetzungen:** QNX SDP 7.x, Momentics/Eclipse CDT, ARM-Target (`armv7le`)

```bash
cd esep-SoSe26-3-3
make                    # → build/armv7le-debug/esep-SoSe26-3-3
make clean              # Build-Artefakte entfernen
```

**Auf dem Target deployen und starten:**

```bash
# Anlage 1 (Server / Weiche)
./esep-SoSe26-3-3 -s

# Anlage 2 (Client / Auswerfer)
./esep-SoSe26-3-3 -c
```

> E-Stop muss beim Start **frei** sein, sonst bricht `main` ab.

---

## Tests

| Target | Beschreibung |
|--------|--------------|
| `make tests` | Google-Test-Suite (Logik, Dispatcher, …) |
| `make calibration_manager_tests` | CalibrationManager |
| `make dispatcher_tests` | Dispatcher |
| `make ejector_test` | Auswerfer (Manual) |
| `make profile_analyzer_test` | Profilanalyse / ADC |
| `make dispatcher_hal_test` | Dispatcher + HAL |
| `make connection_gns_test` | Connection / GNS |

---

## Wichtige Regeln (Implementierung)

- **Max. 2 aktive Werkstücke** auf Anlage 2 (inkl. pending Handover)
- **Handover:** Werkstück wartet am Bandende (A1) → Netzwerk an A2
- **Estop-Reset:** beide Reset-Taster (lokal + Partner) → Idle, Anlage geleert
- **Connection Lost:** Werkstücke bleiben, Service-Phase in `ServiceCalibRegistry`
- **Error:** RESET quittieren → START → Rückkehr mit Deep History **H\***

---

## Team & Repo

- **Kurs:** Embedded Systems Engineering (ESE), HAW Hamburg, SoSe 2026
- **Team:** 3-3
- **Remote:** `https://git.haw-hamburg.de/ese2026-team-3-3/esep-2026sose-team-3-3`

---

## Diagramme öffnen

UML-Diagramme liegen im Ordner `esep-SoSe26-3-3/diagrams/` und können mit **[UMLET](https://www.umlet.com/)** geöffnet werden (`.uxf` / `.uxd`).
