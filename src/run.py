import os
import subprocess
import re
import sys
import argparse
import time
from pathlib import Path
from multiprocessing import Pool, cpu_count

# --- 1. 配置区域 ---
CPP_SOURCE_FILE = "main.cpp"
CPP_EXECUTABLE_NAME = "main.exe" if sys.platform == "win32" else "main"
VALIDATOR_EXECUTABLE = "./validator.exe"

# 【修改点】输出目录基础路径已更改，并删除了 LOG_DIR_BASE
OUTPUT_DIR_BASE = Path("../tmp")

REPORT_FILE = Path("report.txt")
FULL_SCORE_PER_CASE = 5_000_000.0
MAX_PROCESSES = 4

# --- 2. 核心函数 ---

def compile_cpp():
    """编译 C++ 源代码"""
    print(f"--- 正在编译 {CPP_SOURCE_FILE} -> {CPP_EXECUTABLE_NAME} ---")
    try:
        compile_command = [
            "g++", CPP_SOURCE_FILE,
            "-o", CPP_EXECUTABLE_NAME,
            "-O2", "-std=c++17"
        ]
        subprocess.run(compile_command, check=True, capture_output=True, text=True)
        print("编译成功。")
        return True
    except FileNotFoundError:
        print("错误: 未找到 g++ 编译器。请确保它已添加到系统的 PATH 中。")
        return False
    except subprocess.CalledProcessError as e:
        print("错误: 编译失败。")
        print(e.stderr)
        return False

def run_single_test(input_path: Path):
    """
    运行单个测试用例、评分、记录时间并返回结果字典。
    """
    case_name = input_path.stem
    relative_parent = input_path.parent.name
    output_dir = OUTPUT_DIR_BASE / relative_parent
    output_dir.mkdir(parents=True, exist_ok=True)
    
    output_path = output_dir / f"{case_name}.out"
    
    print(f"--- 开始运行测试用例: {input_path.name} ---")

    runtime = 0.0
    # 1. 运行你的 C++ 程序并记录时间
    try:
        with open(input_path, 'r') as f_in, open(output_path, 'w') as f_out:
            start_time = time.monotonic()
            subprocess.run(
                [f"./{CPP_EXECUTABLE_NAME}"],
                stdin=f_in, stdout=f_out, check=True
            )
            end_time = time.monotonic()
            runtime = end_time - start_time
            
    except Exception as e:
        print(f"程序在处理 {input_path.name} 时发生运行时错误: {e}")
        return {'case': input_path.name, 'k': 'Runtime Error', 'score': 0.0, 'ratio': 0.0, 'runtime': 0.0}

    # 2. 运行验证器 (Validator)
    try:
        validator_command = [VALIDATOR_EXECUTABLE, str(input_path), str(output_path)]
        result = subprocess.run(
            validator_command,
            check=True, capture_output=True, text=True,
            encoding='utf-8', errors='ignore'
        )
        validator_output = result.stdout
        
        k_match = re.search(r"Late Users \(K\):\s*(\d+)", validator_output)
        score_match = re.search(r"FINAL SCORE:\s*([\d\.]+)", validator_output)

        if k_match and score_match:
            late_users = int(k_match.group(1))
            final_score = float(score_match.group(1))
            score_ratio = (final_score / FULL_SCORE_PER_CASE) * 100 if FULL_SCORE_PER_CASE > 0 else 0
            
            print(f"结果: {input_path.name}: Runtime = {runtime:.2f}s, K = {late_users}, Score = {final_score:.4f}")
            return {'case': input_path.name, 'k': late_users, 'score': final_score, 'ratio': score_ratio, 'runtime': runtime}
        else:
            print(f"错误: 无法解析 {input_path.name} 的验证器输出。")
            return {'case': input_path.name, 'k': 'Parse Error', 'score': 0.0, 'ratio': 0.0, 'runtime': runtime}

    except subprocess.CalledProcessError as e:
        print(f"错误: 验证器在处理 {input_path.name} 时异常退出。")
        print("--- Validator Standard Error (错误流信息) ---")
        print(e.stderr)
        return {'case': input_path.name, 'k': 'Validator Crash', 'score': 0.0, 'ratio': 0.0, 'runtime': runtime}
    except FileNotFoundError:
        print(f"错误: 验证器未找到 '{VALIDATOR_EXECUTABLE}'")
        return {'error': 'Validator Not Found'}
    except Exception as e:
        print(f"运行验证器时发生未知错误 ({input_path.name}): {e}")
        return {'case': input_path.name, 'k': 'Scorer Error', 'score': 0.0, 'ratio': 0.0, 'runtime': runtime}

def generate_report(results, total_files):
    """根据收集到的所有结果生成报告文件"""
    print(f"\n--- 正在生成报告文件: {REPORT_FILE} ---")
    total_score = 0.0
    
    with open(REPORT_FILE, 'w', encoding='utf-8') as f:
        f.write("--- Automated Test Report ---\n\n")
        header = f"{'Case Name':<20} {'Late Users (K)':<20} {'Final Score':<20} {'Est. Score Ratio (%)':<25} {'Runtime (s)':<15}\n"
        f.write(header)
        f.write("-" * len(header) + "\n")

        def sort_key(res):
            match = re.search(r'\d+', res['case'])
            return int(match.group()) if match else float('inf')

        sorted_results = sorted(results, key=sort_key)

        for res in sorted_results:
            total_score += res['score']
            runtime_str = f"{res.get('runtime', 0.0):.2f}"
            ratio_str = f"{res.get('ratio', 0.0):.2f}%"
            
            f.write(
                f"{res['case']:<20} "
                f"{str(res['k']):<20} "
                f"{res['score']:<20.4f} "
                f"{ratio_str:<25} "
                f"{runtime_str:<15}\n"
            )

        total_possible_score = total_files * FULL_SCORE_PER_CASE
        overall_ratio = (total_score / total_possible_score) * 100 if total_possible_score > 0 else 0

        f.write("-" * len(header) + "\n")
        f.write(f"{'TOTAL SCORE:':<40} {total_score:<20.4f}\n")
        f.write(f"{'OVERALL SCORE RATIO:':<40} {overall_ratio:<20.2f}%\n")

    print("报告生成成功。")

# --- 3. 主逻辑 ---
def main(args):
    """主逻辑：协调编译、测试和报告生成"""
    if not compile_cpp():
        sys.exit(1)

    input_path = Path(args.input_path)
    if not input_path.exists():
        print(f"错误: 提供的路径不存在 '{input_path}'")
        sys.exit(1)

    input_files = []
    if input_path.is_file():
        print(f"检测到单个输入文件: {input_path}")
        input_files.append(input_path)
    elif input_path.is_dir():
        print(f"检测到输入目录: {input_path}")
        input_files = sorted(
            list(input_path.glob('*.in')),
            key=lambda p: int(p.stem) if p.stem.isdigit() else float('inf')
        )

    if not input_files:
        print(f"警告: 在 '{input_path}' 中未找到任何 '.in' 文件。")
        return

    print(f"共找到 {len(input_files)} 个测试用例。")
    
    results = []
    # 【修改点】根据命令行参数决定是并行还是顺序执行
    if len(input_files) > 1 and args.parallel:
        # 默认行为：串行处理
        num_processes = min(MAX_PROCESSES, cpu_count() or 1)
        print(f"使用 {num_processes} 个进程并行测试...")
        with Pool(processes=num_processes) as pool:
            results = pool.map(run_single_test, input_files)
    else:
        # 并行执行（当只有一个文件或指定了 --sequential 时）
        if len(input_files) > 1:
            print("按顺序执行测试...")
        for file_path in input_files:
            result = run_single_test(file_path)
            results.append(result)
            # 如果出现致命错误，则提前中止
            if result.get('error'):
                break
    
    if any(res.get('error') for res in results):
        print("因致命错误导致测试中止。")
        sys.exit(1)
        
    valid_results = [res for res in results if res is not None]

    if valid_results:
        generate_report(valid_results, len(input_files))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="编译并运行C++代码的自动化测试脚本，并记录运行时间。",
        epilog="示例: \n"
               "  并行测试 (默认): python run_test.py ../benchmark1/in\n"
               "  顺序测试: python run_test.py ../benchmark1/in --sequential",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "input_path",
        help="输入文件的路径，或包含'.in'文件的目录路径。"
    )
    # 【修改点】增加 --sequential 参数
    parser.add_argument(
        '--parallel',
        action='store_true',
        help='如果设置此项，则按并行执行测试'
    )
    
    args = parser.parse_args()
    main(args)
