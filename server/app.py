from flask import Flask, request, jsonify
from flask_cors import CORS
import subprocess, json, os, uuid, platform

app = Flask(__name__)
CORS(app)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BACKEND = os.path.join(ROOT, "backend")
EXE = os.path.join(BACKEND, "scheduler.exe" if platform.system() == "Windows" else "scheduler")

ALGORITHMS = ["fcfs", "sjf", "rr", "priority"]

def ensure_compiled():
    if os.path.exists(EXE):
        return
    src = os.path.join(BACKEND, "scheduler.c")
    subprocess.run(["gcc", src, "-o", EXE], check=True)

def run_engine(payload):
    ensure_compiled()
    run_id = str(uuid.uuid4())
    input_path = os.path.join(BACKEND, f"input_{run_id}.json")
    output_path = os.path.join(BACKEND, f"output_{run_id}.json")

    with open(input_path, "w", encoding="utf-8") as f:
        json.dump(payload, f)

    subprocess.run([EXE, input_path, output_path], check=True)

    with open(output_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    try:
        os.remove(input_path)
        os.remove(output_path)
    except OSError:
        pass

    return data

@app.route("/")
def home():
    return jsonify({"status": "Advanced OS Scheduler API running"})

@app.route("/run-algorithm", methods=["POST"])
def run_algorithm():
    payload = request.json
    result = run_engine(payload)
    return jsonify(result)

@app.route("/compare", methods=["POST"])
def compare():
    payload = request.json
    results = []
    for algo in ALGORITHMS:
        test_payload = dict(payload)
        test_payload["algorithm"] = algo
        results.append(run_engine(test_payload))
    best = min(results, key=lambda r: r["metrics"]["averageWaiting"])
    return jsonify({"results": results, "bestAlgorithm": best["algorithm"], "reason": "Lowest average waiting time"})

@app.route("/load-input", methods=["POST"])
def load_input():
    return jsonify({"message": "Input received", "data": request.json})

@app.route("/export", methods=["POST"])
def export():
    return jsonify({"message": "Use frontend Print / Save as PDF option for report export."})

if __name__ == "__main__":
    app.run(debug=True, port=5000)
