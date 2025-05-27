import os
import re
import csv
import argparse
import subprocess
import matplotlib.pyplot as plt
import pandas as pd

class ExperimentRunner:
    def __init__(self, mpi_ranks, omp_threads, build_dir, executable, config_file, csv_file):
        self.mpi_ranks = mpi_ranks
        self.omp_threads = omp_threads
        self.build_dir = build_dir
        self.executable = executable
        self.config_file = config_file
        self.csv_file = csv_file
        self.time_re = re.compile(r"Compute time:\s+([\d\.]+)\s+s")

    def run_all(self):
        os.makedirs(self.build_dir, exist_ok=True)
        with open(self.csv_file, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=["ranks", "threads", "time_s"])
            writer.writeheader()

            for ranks in self.mpi_ranks:
                for threads in self.omp_threads:
                    print(f"\n=== Running: ranks={ranks}, threads={threads} ===\n", flush=True)
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
                        "time_s": time_s
                    })
                    f.flush()

        print(f"Results saved in {self.csv_file}")

    """
    def plot_results(self, output_file="speedup_plot.png"):
        if not os.path.exists(self.csv_file):
            print(f"[ERROR] CSV file '{self.csv_file}' not found.")
            return

        import pandas as pd
        import matplotlib.pyplot as plt

        df = pd.read_csv(self.csv_file)
        base_row = df[(df["ranks"] == 1) & (df["threads"] == 1)]
        if base_row.empty:
            print("[ERROR] No base configuration (1x1) found for speed-up calculation.")
            return

        T_base = base_row.iloc[0]["time_s"]
        df["speedup"] = T_base / df["time_s"]
        df["config"] = df["ranks"].astype(str) + "x" + df["threads"].astype(str)

        plt.figure(figsize=(10, 6))
        plt.title("Speed-up vs MPI x OpenMP Configuration")
        plt.xlabel("MPI Ranks × OMP Threads")
        plt.ylabel("Speed-up (1x1 baseline)")
        plt.grid(True, linestyle="--", alpha=0.5)
        plt.plot(df["config"], df["speedup"], marker="o")
        plt.xticks(rotation=45, ha="right")
        plt.tight_layout()

        plt.savefig(output_file)
        print(f"[✓] Saved speed-up plot to '{output_file}'")

        plt.show()

    """
    def plot_results(self, output_file="runtime_plot.png"):
        if not os.path.exists(self.csv_file):
            print(f"[ERROR] CSV file '{self.csv_file}' not found.")
            return

        import pandas as pd
        import matplotlib.pyplot as plt

        df = pd.read_csv(self.csv_file)

        df["total_threads"] = df["ranks"] * df["threads"]
        df["config"] = df["ranks"].astype(str) + "x" + df["threads"].astype(str)

        # Plot
        plt.figure(figsize=(10, 6))
        plt.title("Compute Time per Configuration")
        plt.xlabel("MPI Ranks × OMP Threads")
        plt.ylabel("Time (s)")
        plt.grid(True, linestyle="--", alpha=0.5)
        plt.plot(df["config"], df["time_s"], marker="o")
        plt.xticks(rotation=45, ha="right")
        plt.tight_layout()

        plt.savefig(output_file)
        print(f"[✓] Saved runtime plot to '{output_file}'")

        plt.show()
    

def parse_args():
    parser = argparse.ArgumentParser(description="Benchmark MPI + OpenMP compute times.")
    subparsers = parser.add_subparsers(dest="mode", help="Mode: run or plot")

    run_parser = subparsers.add_parser("run", help="Run benchmarks")
    run_parser.add_argument("--csv", type=str, default="results.csv")
    run_parser.add_argument("--build-dir", type=str, default="build")
    run_parser.add_argument("--executable", type=str, default="./aiquamplusplus")
    run_parser.add_argument("--config-file", type=str, default="aiquam.json")

    plot_parser = subparsers.add_parser("plot", help="Plot results from CSV")
    plot_parser.add_argument("--csv", type=str, default="results.csv")

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    if args.mode == "run":
        mpi_ranks = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
        omp_threads = [1, 2]

        runner = ExperimentRunner(
            mpi_ranks=mpi_ranks,
            omp_threads=omp_threads,
            build_dir=args.build_dir,
            executable=args.executable,
            config_file=args.config_file,
            csv_file=args.csv
        )
        runner.run_all()

    elif args.mode == "plot":
        runner = ExperimentRunner([], [], "", "", "", args.csv)
        runner.plot_results()

    else:
        print("Specify a mode: run or plot. Use -h for help.")


# python3 scripts/run_experiments.py run --csv results.csv
# python3 scripts/run_experiments.py plot --csv results.csv