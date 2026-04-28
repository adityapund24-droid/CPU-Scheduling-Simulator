const API = "http://127.0.0.1:5000";

const INPUT_STATE_KEY = "lastInputState";

function addRow(p = {}) {
  const tbody = document.querySelector("#processTable tbody");
  if (!tbody) return;

  const tr = document.createElement("tr");
  tr.innerHTML = `
    <td><input value="${p.pid || "P" + (tbody.children.length + 1)}"></td>
    <td><input type="number" value="${p.arrival ?? 0}"></td>
    <td><input type="number" value="${p.burst ?? 5}"></td>
    <td><input type="number" value="${p.priority ?? 1}"></td>
    <td><input type="number" value="${p.ioStart ?? -1}"></td>
    <td><input type="number" value="${p.ioDuration ?? 0}"></td>
    <td><button type="button" onclick="this.closest('tr').remove()">Delete</button></td>
  `;
  tbody.appendChild(tr);
}

function loadSample() {
  const tbody = document.querySelector("#processTable tbody");
  tbody.innerHTML = "";
  [
    { pid: "P1", arrival: 0, burst: 8, priority: 2, ioStart: -1, ioDuration: 0 },
    { pid: "P2", arrival: 1, burst: 4, priority: 1, ioStart: -1, ioDuration: 0 },
    { pid: "P3", arrival: 2, burst: 9, priority: 3, ioStart: -1, ioDuration: 0 },
    { pid: "P4", arrival: 3, burst: 5, priority: 2, ioStart: -1, ioDuration: 0 }
  ].forEach(addRow);
}

function getPayload() {
  const rows = [...document.querySelectorAll("#processTable tbody tr")];

  const processes = rows.map(row => {
    const cells = row.querySelectorAll("input");
    return {
      pid: cells[0].value,
      arrival: Number(cells[1].value),
      burst: Number(cells[2].value),
      priority: Number(cells[3].value),
      ioStart: Number(cells[4].value),
      ioDuration: Number(cells[5].value)
    };
  });

  return {
    algorithm: document.querySelector("#algorithm")?.value || "fcfs",
    quantum: Number(document.querySelector("#quantum").value),
    contextSwitch: Number(document.querySelector("#contextSwitch").value),
    cores: Number(document.querySelector("#cores").value),
    agingThreshold: Number(document.querySelector("#agingThreshold").value),
    processes
  };
}

function saveInputState() {
  if (!document.querySelector("#processTable")) return;
  localStorage.setItem(INPUT_STATE_KEY, JSON.stringify(getPayload()));
}

function restoreInputState() {
  const raw = localStorage.getItem(INPUT_STATE_KEY);
  if (!raw) return false;

  try {
    const payload = JSON.parse(raw);
    if (!Array.isArray(payload?.processes) || payload.processes.length === 0) return false;

    const tbody = document.querySelector("#processTable tbody");
    if (!tbody) return false;

    tbody.innerHTML = "";
    payload.processes.forEach(addRow);

    const algorithmEl = document.querySelector("#algorithm");
    if (algorithmEl && payload.algorithm) algorithmEl.value = payload.algorithm;

    const quantumEl = document.querySelector("#quantum");
    if (quantumEl && Number.isFinite(payload.quantum)) quantumEl.value = payload.quantum;

    const contextSwitchEl = document.querySelector("#contextSwitch");
    if (contextSwitchEl && Number.isFinite(payload.contextSwitch)) contextSwitchEl.value = payload.contextSwitch;

    const coresEl = document.querySelector("#cores");
    if (coresEl && Number.isFinite(payload.cores)) coresEl.value = payload.cores;

    const agingThresholdEl = document.querySelector("#agingThreshold");
    if (agingThresholdEl && Number.isFinite(payload.agingThreshold)) agingThresholdEl.value = payload.agingThreshold;

    return true;
  } catch (_) {
    return false;
  }
}

async function runSingle() {
  const payload = getPayload();
  saveInputState();

  const res = await fetch(`${API}/run-algorithm`, {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(payload)
  });

  const data = await res.json();
  localStorage.setItem("lastSimulation", JSON.stringify(data));

  renderMetrics(data.metrics);
  renderGantt(data.gantt);
  renderResultTable(data.processes);
  renderSnapshots(data.snapshots);
}

function restoreLastSimulation() {
  if (!document.querySelector("#metrics")) return;

  const raw = localStorage.getItem("lastSimulation");
  if (!raw) return;

  try {
    const data = JSON.parse(raw);
    if (!data?.metrics || !data?.gantt || !data?.processes || !data?.snapshots) return;

    renderMetrics(data.metrics);
    renderGantt(data.gantt);
    renderResultTable(data.processes);
    renderSnapshots(data.snapshots);
  } catch (_) {
    // Ignore malformed persisted data.
  }
}

async function runCompare() {
  const payload = getPayload();
  saveInputState();

  const res = await fetch(`${API}/compare`, {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(payload)
  });

  const data = await res.json();
  localStorage.setItem("lastCompare", JSON.stringify(data));

  document.querySelector("#bestBox").innerHTML = `
    <div class="metric-card">
      <h3>Auto Scheduler Suggestion</h3>
      <p>${data.bestAlgorithm.toUpperCase()}</p>
      <small>${data.reason}</small>
    </div>
  `;

  renderCompareTable(data.results);
  renderCompareCharts(data.results);
}

function restoreLastCompare() {
  if (!document.querySelector("#bestBox")) return;

  const raw = localStorage.getItem("lastCompare");
  if (!raw) return;

  try {
    const data = JSON.parse(raw);
    if (!data?.bestAlgorithm || !data?.reason || !Array.isArray(data?.results)) return;

    document.querySelector("#bestBox").innerHTML = `
      <div class="metric-card">
        <h3>Auto Scheduler Suggestion</h3>
        <p>${data.bestAlgorithm.toUpperCase()}</p>
        <small>${data.reason}</small>
      </div>
    `;

    renderCompareTable(data.results);
    renderCompareCharts(data.results);
  } catch (_) {
    // Ignore malformed persisted data.
  }
}

function renderMetrics(m) {
  document.querySelector("#metrics").innerHTML = `
    <div class="metric-card"><h3>Avg Waiting</h3><p>${m.averageWaiting}</p></div>
    <div class="metric-card"><h3>Avg Turnaround</h3><p>${m.averageTurnaround}</p></div>
    <div class="metric-card"><h3>Avg Response</h3><p>${m.averageResponse}</p></div>
    <div class="metric-card"><h3>CPU Utilization</h3><p>${m.cpuUtilization}%</p></div>
    <div class="metric-card"><h3>Throughput</h3><p>${m.throughput}</p></div>
    <div class="metric-card"><h3>Starved</h3><p>${m.starvedProcesses}</p></div>
  `;
}

function renderGantt(gantt) {
  const box = document.querySelector("#gantt");
  box.innerHTML = "";
  gantt.forEach(seg => {
    const div = document.createElement("div");
    div.className = `gantt-seg ${seg.type}`;
    div.style.minWidth = `${Math.max(65, (seg.end - seg.start) * 35)}px`;
    div.innerHTML = `<b>${seg.pid}</b><br>${seg.start} - ${seg.end}`;
    box.appendChild(div);
  });
}

function renderResultTable(processes) {
  const tbody = document.querySelector("#resultTable tbody");
  tbody.innerHTML = "";
  processes.forEach(p => {
    tbody.innerHTML += `
      <tr>
        <td>${p.pid}</td>
        <td>${p.completion}</td>
        <td>${p.waiting}</td>
        <td>${p.turnaround}</td>
        <td>${p.response}</td>
        <td>${p.starvation ? "Yes" : "No"}</td>
      </tr>
    `;
  });
}

function renderSnapshots(snaps) {
  const box = document.querySelector("#snapshots");
  box.innerHTML = "";
  snaps.slice(0, 25).forEach(s => {
    box.innerHTML += `
      <div class="snapshot">
        <b>Time ${s.time}</b><br>
        Ready Queue: ${s.ready || "-"}<br>
        Running: ${s.running || "-"}<br>
        Waiting: ${s.waiting || "-"}
      </div>
    `;
  });
}

function renderCompareTable(results) {
  const tbody = document.querySelector("#compareTable tbody");
  tbody.innerHTML = "";
  results.forEach(r => {
    const m = r.metrics;
    tbody.innerHTML += `
      <tr>
        <td>${r.algorithm.toUpperCase()}</td>
        <td>${m.averageWaiting}</td>
        <td>${m.averageTurnaround}</td>
        <td>${m.averageResponse}</td>
        <td>${m.cpuUtilization}%</td>
        <td>${m.throughput}</td>
      </tr>
    `;
  });
}

let waitingChartObj = null;
let utilChartObj = null;

function renderCompareCharts(results) {
  const labels = results.map(r => r.algorithm.toUpperCase());

  if (waitingChartObj) waitingChartObj.destroy();
  waitingChartObj = new Chart(document.querySelector("#waitingChart"), {
    type: "bar",
    data: {
      labels,
      datasets: [{
        label: "Average Waiting Time",
        data: results.map(r => r.metrics.averageWaiting)
      }]
    }
  });

  if (utilChartObj) utilChartObj.destroy();
  utilChartObj = new Chart(document.querySelector("#utilChart"), {
    type: "bar",
    data: {
      labels,
      datasets: [{
        label: "CPU Utilization %",
        data: results.map(r => r.metrics.cpuUtilization)
      }]
    }
  });
}

function saveSimulation() {
  const data = localStorage.getItem("lastSimulation");
  if (!data) {
    alert("Run a simulation first.");
    return;
  }

  const blob = new Blob([data], {type: "application/json"});
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "simulation-result.json";
  a.click();
}

document.addEventListener("DOMContentLoaded", () => {
  if (document.querySelector("#processTable")) {
    const restored = restoreInputState();
    if (!restored) loadSample();

    const controls = document.querySelector(".main") || document;
    controls.addEventListener("input", (event) => {
      if (event.target.closest("#processTable") || event.target.closest(".grid")) {
        saveInputState();
      }
    });
  }
  restoreLastSimulation();
  restoreLastCompare();
});
