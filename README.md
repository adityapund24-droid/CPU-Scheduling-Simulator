# CPU Scheduling Simulator

A full advanced OS project with:

- C backend scheduling engine
- Python Flask API wrapper
- Modern web frontend
- FCFS, SJF, Round Robin, Priority
- Context switching cost
- Multi-core simulation support
- Starvation detection + aging concept
- Gantt chart visualization
- Ready queue / waiting queue visualization
- Metrics dashboard
- Algorithm comparison
- Save/load simulation
- Export report using browser print/PDF

---

## Project Structure

```txt
advanced_os_scheduler/
├── backend/
│   ├── scheduler.c
│   ├── scheduler.exe          # generated after compile
│   ├── input.json
│   └── output.json
├── server/
│   ├── app.py
│   └── requirements.txt
├── frontend/
│   ├── index.html
│   ├── pages/
│   ├── css/
│   └── js/
└── README.md
```

---

## Process Input Format

Each process has:

```json
{
  "pid": "P1",
  "arrival": 0,
  "burst": 5,
  "priority": 2,
  "ioStart": -1,
  "ioDuration": 0
}
```

- `ioStart = -1` means no I/O burst
- smaller priority number means higher priority

---

## Viva Explanation

This simulator demonstrates process scheduling in operating systems using multiple algorithms. It supports preemptive and non-preemptive scheduling, context switch overhead, multi-core simulation, starvation detection, and performance comparison using Gantt charts and metrics.
