/**
 * TestPicker - Minimalist Light Mode Engine (Vanilla JS)
 * Connected directly to compiled C++ backend (testpicker.exe)
 */

const State = {
  projectFiles: [],
  changedFiles: [4, 14, 24],
  budgetSec: 60,
  activeResults: [],
  activeSchedule: [],
  candsCount: 0,
  uncoveredCount: 0,
  isCppConnected: false
};

// ── API Service calling C++ Executable ─────────────────────────────

async function fetchProjectFromCpp() {
  try {
    const res = await fetch('/api/project');
    if (!res.ok) throw new Error("API failed");
    const data = await res.json();
    if (data.files && data.files.length > 0) {
      State.projectFiles = data.files.slice(0, 40); // show first 40 files in chips
      State.isCppConnected = true;
      updateStatusBadge(true);
      return true;
    }
  } catch (err) {
    console.warn("Could not reach C++ backend API, using fallback mode:", err);
    updateStatusBadge(false);
  }
  return false;
}

async function analyzeCommitCpp(budgetSec, changedFiles) {
  const fileStr = changedFiles.join(',');
  try {
    const res = await fetch(`/api/analyze?budget=${budgetSec}&files=${fileStr}`);
    if (!res.ok) throw new Error("API analysis failed");
    const data = await res.json();
    return data;
  } catch (err) {
    console.error("C++ analyze error:", err);
    return null;
  }
}

async function simulateRunCpp(stratIdx, budgetSec, changedFiles) {
  const fileStr = changedFiles.join(',');
  try {
    const res = await fetch(`/api/simulate?strat=${stratIdx}&budget=${budgetSec}&files=${fileStr}`);
    if (!res.ok) throw new Error("API simulate failed");
    const data = await res.json();
    return data;
  } catch (err) {
    console.error("C++ simulate error:", err);
    return null;
  }
}

// ── UI Controller ──────────────────────────────────────────────────

async function runPipeline() {
  const metaText = document.getElementById("metaAffectedCount");
  if (metaText) metaText.innerText = "Executing C++ algorithm engine (testpicker.exe)...";

  const cppData = await analyzeCommitCpp(State.budgetSec, State.changedFiles);

  if (cppData && cppData.results) {
    State.activeResults = cppData.results;
    State.activeSchedule = cppData.kahnSchedule || [];
    State.candsCount = cppData.candsCount;
    State.uncoveredCount = cppData.uncoveredCount;

    if (metaText) {
      metaText.innerText = `${cppData.candsCount} affected tests evaluated by native C++ (testpicker.exe) · All 5 strategies`;
    }

    renderCards(cppData.results, State.budgetSec);
    renderKahnSequence(cppData.kahnSchedule, "Knapsack DP (Winner)");
  } else {
    // Fallback notice
    if (metaText) metaText.innerText = "Backend evaluating...";
  }
}

function renderCards(results, budgetSec) {
  const container = document.getElementById("strategyGrid");
  if (!container) return;
  container.innerHTML = "";

  results.forEach((r, idx) => {
    const card = document.createElement("div");
    card.className = "card-strategy";
    if (idx === 3) card.classList.add("highlight-knapsack");
    if (idx === 4) card.classList.add("highlight-bnb");

    let badge = "";
    if (idx === 3) badge = `<span class="card-badge badge-winner">C++ Optimal Detect</span>`;
    else if (idx === 4) badge = `<span class="card-badge badge-cover">C++ Min Cover</span>`;

    const pct = Math.min(100, Math.round((r.actualTime / budgetSec) * 100));
    const fillCls = idx === 3 ? "fill-indigo" : idx === 4 ? "fill-emerald" : "";

    card.innerHTML = `
      ${badge}
      <div class="card-head">
        <div class="card-strat-title">${r.name}</div>
        <div class="card-strat-type">${getParadigm(idx)}</div>
      </div>

      <div class="metric-primary">
        <div class="metric-primary-val ${idx === 3 ? 'indigo' : idx === 4 ? 'emerald' : ''}">${(r.detectProb * 100).toFixed(1)}%</div>
        <div class="metric-primary-lbl">Detection Probability</div>
      </div>

      <div class="progress-track">
        <div class="progress-fill ${fillCls}" style="width: ${pct}%;"></div>
      </div>

      <div class="details-list">
        <div class="detail-row">
          <span class="detail-label">Wall-Clock Time</span>
          <span class="detail-val">${r.actualTime.toFixed(1)}s <span style="font-size:0.7rem; color:var(--text-muted);">/ ${budgetSec}s</span></span>
        </div>
        <div class="detail-row">
          <span class="detail-label">Tests Selected</span>
          <span class="detail-val">${r.selectedCount || 0}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">File Coverage</span>
          <span class="detail-val">${r.coveredFiles}/${r.totalCoverableFiles}</span>
        </div>
        <div class="detail-row">
          <span class="detail-label">C++ Latency</span>
          <span class="detail-val">${r.algoMs.toFixed(2)} ms</span>
        </div>
      </div>

      <button class="btn-simulate" onclick="simulateStrategy(${idx})">
        Simulate Run in C++
      </button>
    `;
    container.appendChild(card);
  });
}

function getParadigm(idx) {
  switch (idx) {
    case 0: return "Uniform Shuffle";
    case 1: return "Greedy Ratio (p/t)";
    case 2: return "Greedy Set Cover";
    case 3: return "0/1 Knapsack DP";
    case 4: return "Branch and Bound";
    default: return "";
  }
}

function renderKahnSequence(schedule, stratName) {
  const container = document.getElementById("kahnSequenceContainer");
  if (!container) return;
  container.innerHTML = "";

  document.getElementById("kahnOrderCount").innerText = `${schedule.length} scheduled tests (${stratName})`;

  if (schedule.length === 0) {
    container.innerHTML = `<span style="font-size:0.8rem; color:var(--text-muted);">No tests scheduled under current budget.</span>`;
    return;
  }

  schedule.forEach((node, i) => {
    const item = document.createElement("div");
    item.className = `kahn-node ${node.isSetup ? 'is-setup' : ''}`;
    item.innerHTML = `
      <span class="kahn-node-name">${node.name}</span>
      <span class="kahn-node-time">${node.timeSec.toFixed(1)}s</span>
      ${node.isSetup ? '<span style="font-size:0.65rem; color:var(--text-muted);">(setup)</span>' : ''}
    `;
    container.appendChild(item);

    if (i < schedule.length - 1) {
      const arrow = document.createElement("span");
      arrow.className = "kahn-arrow";
      arrow.innerText = "→";
      container.appendChild(arrow);
    }
  });
}

async function simulateStrategy(idx) {
  const alertBox = document.getElementById("simulationAlertBox");
  if (!alertBox) return;

  alertBox.innerHTML = `
    <div class="sim-alert">
      <div class="sim-alert-desc">Executing simulation in native C++ binary...</div>
    </div>
  `;

  const simData = await simulateRunCpp(idx, State.budgetSec, State.changedFiles);
  if (!simData) return;

  const caught = simData.bugCaught;
  alertBox.innerHTML = `
    <div class="sim-alert ${caught ? 'caught' : 'missed'}">
      <div class="sim-alert-title">${caught ? '✓ Regression Bug Detected by C++' : '✕ Bug Undetected'}</div>
      <div class="sim-alert-desc">
        ${caught
      ? `Planted bug in <b>file_${simData.buggyFileId}.cpp</b> was caught by tests selected under <b>${simData.strategyName}</b> (${simData.testsFailed} test failures detected).`
      : `Selected tests under <b>${simData.strategyName}</b> did not exercise the planted bug in <b>file_${simData.buggyFileId}.cpp</b>.`}
      </div>
    </div>
  `;
}

function renderChips() {
  const container = document.getElementById("changedChipsContainer");
  if (!container) return;
  container.innerHTML = "";

  State.projectFiles.forEach(f => {
    const chip = document.createElement("div");
    chip.className = `chip ${State.changedFiles.includes(f.id) ? 'active' : ''}`;
    chip.innerText = f.name;
    chip.addEventListener("click", () => {
      if (State.changedFiles.includes(f.id)) {
        State.changedFiles = State.changedFiles.filter(id => id !== f.id);
      } else {
        State.changedFiles.push(f.id);
        State.changedFiles.sort((a, b) => a - b);
      }
      chip.classList.toggle("active");
      document.getElementById("changedCount").innerText = `${State.changedFiles.length} files changed`;
      runPipeline();
    });
    container.appendChild(chip);
  });
}

function updateStatusBadge(connected) {
  const badge = document.querySelector(".status-badge");
  if (badge) {
    if (connected) {
      badge.innerHTML = `<div class="status-dot"></div><span>C++ Binary Connected</span>`;
      badge.style.color = "var(--emerald-primary)";
      badge.style.background = "var(--emerald-subtle)";
      badge.style.borderColor = "rgba(5, 150, 105, 0.2)";
    } else {
      badge.innerHTML = `<div class="status-dot" style="background:var(--amber-primary);"></div><span>Client Mode</span>`;
      badge.style.color = "var(--amber-primary)";
    }
  }
}

function setupEvents() {
  const slider = document.getElementById("budgetSlider");
  if (slider) {
    slider.addEventListener("input", (e) => {
      State.budgetSec = parseFloat(e.target.value);
      document.getElementById("budgetVal").innerText = `${State.budgetSec}s`;
      runPipeline();
    });
  }

  const btnRandom = document.getElementById("btnRandomCommit");
  if (btnRandom) {
    btnRandom.addEventListener("click", () => {
      const maxF = Math.max(5, State.projectFiles.length);
      const count = 2 + Math.floor(Math.random() * 3);
      const picked = new Set();
      while (picked.size < count) {
        picked.add(Math.floor(Math.random() * maxF));
      }
      State.changedFiles = Array.from(picked).sort((a, b) => a - b);
      document.getElementById("changedCount").innerText = `${State.changedFiles.length} files changed`;
      renderChips();
      runPipeline();
    });
  }
}

window.addEventListener("DOMContentLoaded", async () => {
  setupEvents();
  const connected = await fetchProjectFromCpp();
  if (!connected) {
    // Generate fallback files if server not started yet
    State.projectFiles = Array.from({ length: 40 }, (_, i) => ({ id: i, name: `file_${i}.cpp` }));
  }
  renderChips();
  runPipeline();
});
