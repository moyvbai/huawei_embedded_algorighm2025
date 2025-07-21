import argparse
import os
import subprocess
import re
import sys
import time
import json
import pandas as pd
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor, as_completed
from tqdm import tqdm

# --- 1. 配置区域 ---
# 默认的文件和程序名
DEFAULT_GEN_SCRIPT = "gen.py"
DEFAULT_CONFIG_FILE = "config.json"
DEFAULT_CPP_SOURCE_FILE = "main.cpp"
DEFAULT_VALIDATOR_EXECUTABLE = "validator.exe"
# 根据操作系统确定可执行文件名
DEFAULT_CPP_EXECUTABLE_NAME = "main.exe" if sys.platform == "win32" else "main"
FULL_SCORE_PER_CASE = 5000000

def run_command(command, description):
    """通用命令执行函数，包含错误处理。"""
    print(f"--- {description} ---")
    try:
        # 使用 shell=True 可能在某些情况下更方便，但要注意安全风险
        # 对于 g++ 和 python 这类常用命令，直接使用列表是更安全的选择
        result = subprocess.run(
            command,
            check=True,
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='ignore'
        )
        print(f"Success: {description} completed.")
        return True, result.stdout
    except FileNotFoundError:
        print(f"Error: Command '{command[0]}' not found. Please ensure it's in your system's PATH.")
        return False, None
    except subprocess.CalledProcessError as e:
        print(f"Error: {description} failed with exit code {e.returncode}.")
        print("--- STDOUT ---")
        print(e.stdout)
        print("--- STDERR ---")
        print(e.stderr)
        return False, e.stderr

def generate_data(args):
    """调用数据生成脚本。"""
    gen_command = [
        "python", args.gen_script,
        args.config,
        "-n", str(args.num_files),
        "-o", args.data_dir,
        "--seed", str(args.seed)
    ]
    success, _ = run_command(gen_command, "Generating Test Data")
    if not success:
        sys.exit(1) # 如果数据生成失败，则终止整个流程

def compile_cpp(args):
    """编译 C++ 源代码。"""
    # 确保可执行文件路径的目录存在
    executable_path = Path(args.solution_exe)
    executable_path.parent.mkdir(parents=True, exist_ok=True)

    compile_command = [
        "g++", args.solution_src,
        "-o", str(executable_path),
        "-O2", "-std=c++17", "-Wall" # 添加 -Wall 以显示更多警告
    ]
    success, _ = run_command(compile_command, f"Compiling {args.solution_src}")
    if not success:
        sys.exit(1) # 如果编译失败，则终止

def run_single_test(args_tuple):
    """
    处理单个测试用例的完整逻辑。
    这个函数将在独立的进程中被执行。
    """
    # 解包传入的参数
    input_path, solution_exe, validator_exe, solution_outputs_root = args_tuple

    # 准备路径
    relative_path = input_path.relative_to(Path(solution_outputs_root).parent / "generated_data")
    output_path = solution_outputs_root / relative_path.with_suffix(".out")
    output_path.parent.mkdir(parents=True, exist_ok=True)

    case_name = f"{relative_path.parent}/{input_path.name}"

    # 1. 运行 C++ 程序
    execute_time = -1.0
    status = "OK"
    start_time = time.time()
    try:
        with open(input_path, 'r') as f_in, open(output_path, 'w') as f_out:
            subprocess.run(
                [solution_exe],
                stdin=f_in, stdout=f_out, check=True, timeout=31
            )
        execute_time = time.time() - start_time
    except subprocess.TimeoutExpired:
        status = "Timeout"
    except Exception:
        status = "Runtime Error"

    # 2. 调用评测器
    score, late_users_k = 0.0, -1
    if status == "OK":
        validator_command = [validator_exe, str(input_path), str(output_path)]
        val_success, val_output = run_command(validator_command, f"Validating {case_name}")
        if val_success:
            k_match = re.search(r"Late Users \(K\):\s*(\d+)", val_output)
            score_match = re.search(r"FINAL SCORE:\s*([\d\.]+)", val_output)
            if k_match and score_match:
                late_users_k = int(k_match.group(1))
                score = float(score_match.group(1))
            else:
                status = "Validator Parse Error"
        else:
            status = "Validator Crash"

    # --- 【修改点】: 计算并添加得分率 ---
    score_rate = (score / FULL_SCORE_PER_CASE) if FULL_SCORE_PER_CASE > 0 else 0.0

    combo_name = relative_path.parts[0]
    return {
        "combination": combo_name,
        "case_name": input_path.name,
        "status": status,
        "score": score,
        "late_users_k": late_users_k,
        "execution_time_s": execute_time,
        "score_rate": score_rate, # 新增字段
    }
# --- 重构后的主测试函数 ---
def run_all_tests_parallel(args):
    """使用多进程并行地运行所有测试用例。"""
    data_root = Path(args.data_dir)
    solution_outputs_root = Path(args.output_dir) / "solution_outputs"

    input_files = sorted(list(data_root.rglob('*.in')))

    if not input_files:
        print("Warning: No input files found. Skipping tests.")
        return []

    print(f"\n--- Found {len(input_files)} test cases. Starting parallel execution with {args.workers} workers... ---")

    # 准备要传递给每个工作进程的参数
    tasks_args = [
        (path, args.solution_exe, args.validator, solution_outputs_root) for path in input_files
    ]

    results = []
    # 使用 ProcessPoolExecutor 创建进程池
    with ProcessPoolExecutor(max_workers=args.workers) as executor:
        # 使用 executor.map 来分发任务，并用 tqdm 创建进度条
        future_to_task = {executor.submit(run_single_test, arg): arg for arg in tasks_args}

        for future in tqdm(as_completed(future_to_task), total=len(tasks_args), desc="Running tests"):
            try:
                result = future.result()
                results.append(result)
            except Exception as e:
                print(f"A task failed with an unexpected error: {e}")

    # 对结果进行排序，以保证报告的顺序一致性
    results.sort(key=lambda r: (r['combination'], r['case_name']))
    return results


def generate_reports(results, args):
    """生成详细的CSV报告和可读的摘要报告。"""
    if not results:
        print("No results to report.")
        return

    report_dir = Path(args.output_dir) / "reports"
    report_dir.mkdir(parents=True, exist_ok=True)

    df = pd.DataFrame(results)

    # 1. 生成详细的 CSV 报告
    csv_path = report_dir / "report_details.csv"
    df.to_csv(csv_path, index=False, float_format='%.4f')
    print(f"\n--- Detailed report saved to: {csv_path} ---")

    # 2. 生成可读的摘要报告
    summary_path = report_dir / "report_summary.txt"
    with open(summary_path, 'w', encoding='utf-8') as f:
        f.write("========== Automated Test Summary Report ==========\n\n")

        # --- 【修改点 1】: 同时聚合 score 和 score_rate ---
        summary = df.groupby('combination').agg(
            score_mean=('score', 'mean'),
            score_rate_mean=('score_rate', 'mean'), # 新增聚合
            score_min=('score', 'min'),
            score_max=('score', 'max'),
            count=('score', 'count'),
            score_std=('score', 'std')
        ).sort_values(by='score_mean', ascending=False)

        # 重命名列并添加格式化的百分比列
        summary = summary.rename(columns={'score_mean': 'mean', 'score_min': 'min', 'score_max': 'max', 'score_std': 'std'})
        summary['mean_rate(%)'] = summary['score_rate_mean'].map('{:.2%}'.format)

        # 调整列顺序，将百分比放在均分旁边
        summary = summary[['mean', 'mean_rate(%)', 'min', 'max', 'count', 'std']]

        f.write("--- Performance Summary by Parameter Combination ---\n")
        f.write("(Sorted by average score, descending)\n\n")
        f.write(summary.to_string(float_format="%.2f"))
        f.write("\n\n")

        f.write("--- Error & Timeout Summary ---\n")
        errors = df[df['status'] != 'OK']
        if errors.empty:
            f.write("Congratulations! No errors or timeouts detected.\n")
        else:
            error_summary = errors.groupby(['combination', 'status']).size().unstack(fill_value=0)
            f.write(error_summary.to_string())

        # --- 【修改点 2】: 计算并写入总分和总百分比 ---
        total_score = df['score'].sum()
        total_possible_score = len(df) * FULL_SCORE_PER_CASE
        overall_ratio = (total_score / total_possible_score) if total_possible_score > 0 else 0

        f.write("\n\n--- Overall Benchmark Performance ---\n")
        f.write(f"{'TOTAL SCORE:':<25} {total_score:,.2f}\n")
        f.write(f"{'OVERALL SCORE RATIO:':<25} {overall_ratio:.2%}\n")

    print(f"--- Human-readable summary saved to: {summary_path} ---")

    # 在控制台也打印一份摘要，方便快速查看
    print("\n--- Performance Summary by Parameter Combination ---")
    print(summary.to_string(float_format="%.2f"))

def main():
    parser = argparse.ArgumentParser(
        description="A full pipeline script to generate data, compile, run, and validate a C++ solution.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    # --- 核心控制参数 ---
    parser.add_argument("--config", default=DEFAULT_CONFIG_FILE, help=f"Path to the JSON config for data generator. Default: {DEFAULT_CONFIG_FILE}")
    parser.add_argument("--gen-script", default=DEFAULT_GEN_SCRIPT, help=f"Path to the data generator python script. Default: {DEFAULT_GEN_SCRIPT}")
    parser.add_argument("--solution-src", default=DEFAULT_CPP_SOURCE_FILE, help=f"Path to your C++ source file. Default: {DEFAULT_CPP_SOURCE_FILE}")
    parser.add_argument("--validator", default=DEFAULT_VALIDATOR_EXECUTABLE, help=f"Path to the validator executable. Default: {DEFAULT_VALIDATOR_EXECUTABLE}")

    # --- 流程控制参数 ---
    parser.add_argument("-o","--output-dir", default="benchmark_run", help="Root directory for all outputs (data, solution outputs, reports). Default: 'benchmark_run'")
    parser.add_argument("-n", "--num-files", type=int, default=1, help="Number of files to generate per combination. Default: 1")
    parser.add_argument("--seed", type=int, default=42, help="Base random seed for data generation. Default: 42")
    parser.add_argument("--no-gen", action="store_true", help="Skip the data generation step and use existing data.")
    parser.add_argument("--no-compile", action="store_true", help="Skip the compilation step and use existing executable.")
    parser.add_argument(
        "-w", "--workers",
        type=int,
        default=os.cpu_count(),  # 默认使用所有CPU核心
        help=f"Number of parallel processes to use for testing. Default is your CPU core count ({os.cpu_count()})."
    )
    args = parser.parse_args()

    # --- 派生路径 ---
    args.data_dir = os.path.join(args.output_dir, "generated_data")
    args.solution_exe = os.path.join(args.output_dir, "bin", DEFAULT_CPP_EXECUTABLE_NAME)

    # --- 执行流水线 ---
    start_time = time.time()

    if not args.no_gen:
        generate_data(args)
    else:
        print("--- Skipping data generation as per --no-gen flag. ---")

    if not args.no_compile:
        compile_cpp(args)
    else:
        print("--- Skipping compilation as per --no-compile flag. ---")
        if not os.path.exists(args.solution_exe):
            print(f"Error: Executable not found at '{args.solution_exe}'. Please compile first or remove --no-compile.")
            sys.exit(1)

    results = run_all_tests_parallel(args)
    generate_reports(results, args)

    total_time = time.time() - start_time
    print(f"\n--- Pipeline finished in {total_time:.2f} seconds. ---")


if __name__ == "__main__":
    main()