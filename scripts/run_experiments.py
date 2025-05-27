import os
import re
import csv
import argparse
import subprocess
import matplotlib.pyplot as plt
import pandas as pd

class ExperimentRunner:
    def __init__(self, mpi_ranks, omp_threads, build_dir, executable, config_file, csv_file, device_label):
        self.mpi_ranks = mpi_ranks
        self.omp_threads = omp_threads
        self.build_dir = build_dir
        self.executable = executable
        self.config_file = config_file
        self.csv_file = csv_file
        self.device_label = device_label  # "CPU" or "GPU"
        self.time_re = re.compile(r"Compute time:\s+([\d\.]+)\s+s")

    def run_all(self):
        os.makedirs(self.build_dir, exist_ok=True)
        with open(self.csv_file, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=["ranks", "threads", "device", "time_s"])
            writer.writeheader()

            for ranks in self.mpi_ranks:
                for threads in self.omp_threads:
                    print(f"\n=== Running: ranks={ranks}, threads={threads}, device={self.device_label} ===\n", flush=True)

                    env = os.environ.copy()
                    env["OMP_NUM_THREADS"] = str(threads)

                    cmd = ["mpirun", "-n", str(ranks), self.executable, self.config_file]
                    proc = subprocess.Popen(
                        cmd,
                        cwd=self.build_dir,
                        env=env,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        bufsize=1,
                    )

                    time_s = None
                    for line in proc.stdout:
                        print(line, end="")
                        m = self.time_re.search(line)
                        if m:
                            time_s = float(m.group(1))

                    ret = proc.wait()
                    if ret != 0:
                        print(f"[ERROR] ranks={ranks} threads={threads} → exit code {ret}")
                        continue

                    if time_s is None:
                        print(f"[WARN] time not found for ranks={ranks}, threads={threads}")
                        continue

                    writer.writerow({
                        "ranks": ranks,
                        "threads": threads,
                        "device": self.device_label,
                        "time_s": time_s
                    })
                    f.flush()

        print(f"\n[✓] Results saved in {self.csv_file}")


def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark MPI + OpenMP + GPU compute times.")
    parser.add_argument("--csv", type=str, default="results.csv")
    parser.add_argument("--build-dir", type=str, required=True)
    parser.add_argument("--executable", type=str, default="./aiquamplusplus")
    parser.add_argument("--config-file", type=str, default="aiquam.json")
    parser.add_argument("--device", choices=["cpu", "gpu"], required=True, help="Select device type")

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    mpi_ranks = [1, 2, 4, 6, 8]
    omp_threads = [1, 2, 4, 8]

    runner = ExperimentRunner(
        mpi_ranks=mpi_ranks,
        omp_threads=omp_threads,
        build_dir=args.build_dir,
        executable=args.executable,
        config_file=args.config_file,
        csv_file=args.csv,
        device_label=args.device.upper()
    )
    runner.run_all()

# python3 scripts/run_experiments.py --device gpu --build-dir build_gpu --csv results_gpu.csv
# python3 scripts/run_experiments.py --device cpu --build-dir build_cpu --csv results_cpu.csv