import os
import subprocess
import re
import sys
from pathlib import Path

# --- 1. 配置区域 ---
# 您可以在这里修改文件和目录名
CPP_SOURCE_FILE = "main.cpp"
CPP_EXECUTABLE_NAME = "main.exe" if sys.platform == "win32" else "main"
VALIDATOR_EXECUTABLE = "./validator.exe"

BENCHMARK_DIR = Path("../benchmark1")
INPUT_DIR = BENCHMARK_DIR / "in"
OUTPUT_DIR = BENCHMARK_DIR / "out"

LOG_DIR = Path("./log") / "benchmark1"
REPORT_FILE = Path("report.txt")

# --- 2. 主逻辑 ---

def compile_cpp():
    """编译 C++ 源代码"""
    print(f"--- Compiling {CPP_SOURCE_FILE} -> {CPP_EXECUTABLE_NAME} ---")
    try:
        # 使用 -O2 优化和 C++11 标准
        compile_command = [
            "g++", CPP_SOURCE_FILE,
            "-o", CPP_EXECUTABLE_NAME,
            "-O2", "-std=c++17"
        ]
        subprocess.run(compile_command, check=True, capture_output=True, text=True)
        print("Compilation successful.")
        return True
    except FileNotFoundError:
        print("Error: g++ compiler not found. Please ensure it's in your system's PATH.")
        return False
    except subprocess.CalledProcessError as e:
        print("Error: Compilation failed.")
        print(e.stderr)
        return False

def run_tests():
    """执行所有测试、评分并生成报告 (已增强错误捕获)"""
    # 确保输出目录存在
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    # 查找所有输入文件并按数字排序
    try:
        input_files = sorted(
            INPUT_DIR.glob('*.in'),
            key=lambda p: int(p.stem)
        )
    except FileNotFoundError:
        print(f"Error: Input directory not found at '{INPUT_DIR}'")
        return

    if not input_files:
        print(f"Warning: No input files found in '{INPUT_DIR}'")
        return
    # 定义每个用例的满分
    FULL_SCORE_PER_CASE = 5_000_000.0
    results = []
    total_score = 0.0
    sample_count = 0

    for input_path in input_files:
        case_name = input_path.stem
        output_path = OUTPUT_DIR / f"{case_name}.out"

        print(f"\n--- Running case: {input_path.name} ---")

        # 1. 运行你的 C++ 程序 (逻辑不变)
        try:
            with open(input_path, 'r') as f_in, open(output_path, 'w') as f_out:
                subprocess.run(
                    [f"./{CPP_EXECUTABLE_NAME}"],
                    stdin=f_in, stdout=f_out, check=True, timeout=35
                )
        except subprocess.TimeoutExpired:
            print(f"Error: Your program timed out on {input_path.name}.")
            results.append({'case': input_path.name, 'k': 'Timeout', 'score': 0.0})
            continue
        except Exception as e:
            print(f"An error occurred while running your program on {input_path.name}: {e}")
            results.append({'case': input_path.name, 'k': 'Runtime Error', 'score': 0.0})
            continue

        # --- 【修改点】: 增强 validator 的错误捕获和显示 ---
        try:
            validator_command = [VALIDATOR_EXECUTABLE, str(input_path), str(output_path)]
            result = subprocess.run(
                validator_command,
                check=True,          # 如果validator失败，则抛出异常
                capture_output=True, # 捕获stdout和stderr
                text=True,           # 以文本模式处理流
                encoding='utf-8',    # 指定编码
                errors='ignore'      # 忽略解码错误
            )
            validator_output = result.stdout

            # 正常解析
            k_match = re.search(r"Late Users \(K\):\s*(\d+)", validator_output)
            score_match = re.search(r"FINAL SCORE:\s*([\d\.]+)", validator_output)

            if k_match and score_match:
                late_users = int(k_match.group(1))
                final_score = float(score_match.group(1))

                # ---【修改点】---
                # 计算并存储得分率
                score_ratio = (final_score / FULL_SCORE_PER_CASE) * 100 if FULL_SCORE_PER_CASE > 0 else 0
                results.append({'case': input_path.name, 'k': late_users, 'score': final_score, 'ratio': score_ratio})

                total_score += final_score
                print(f"Result: K = {late_users}, Score = {final_score:.4f}, Ratio = {score_ratio:.2f}%")
            else:
                print("Error: Could not parse validator output.")
                results.append({'case': input_path.name, 'k': 'Parse Error', 'score': 0.0, 'ratio': 0.0})


        except subprocess.CalledProcessError as e:
            # 当 validator.exe 返回非零退出码时，捕获此异常
            print(f"Error: Validator exited with a non-zero status for {input_path.name}.")
            print("--- Validator Standard Output ---")
            print(e.stdout)
            print("--- Validator Standard Error (错误流信息) ---")
            print(e.stderr) # <-- 核心改动：打印错误流
            print("------------------------------------------")
            results.append({'case': input_path.name, 'k': 'Validator Crash', 'score': 0.0})
        except FileNotFoundError:
            print(f"Error: Validator not found at '{VALIDATOR_EXECUTABLE}'")
            return
        except Exception as e:
            print(f"An unexpected error occurred while running the validator on {input_path.name}: {e}")
            results.append({'case': input_path.name, 'k': 'Scorer Error', 'score': 0.0})

    # 生成包含新列的最终报告
    print(f"\n--- Generating report file: {REPORT_FILE} ---")
    with open(REPORT_FILE, 'w', encoding='utf-8') as f:
        f.write("--- Automated Test Report ---\n\n")
        # 增加表头并调整宽度
        header = f"{'Case Name':<20} {'Late Users (K)':<20} {'Final Score':<20} {'Est. Score Ratio (%)':<25}\n"
        f.write(header)
        f.write("-" * len(header) + "\n")

        for res in results:
            # 写入每一行的数据，包括格式化的百分比
            f.write(f"{res['case']:<20} {str(res['k']):<20} {res['score']:<20.4f} {res['ratio']:.2f}%\n")

        # 计算并写入总分和总得分率
        total_possible_score = len(input_files) * FULL_SCORE_PER_CASE
        overall_ratio = (total_score / total_possible_score) * 100 if total_possible_score > 0 else 0

        f.write("-" * len(header) + "\n")
        f.write(f"{'TOTAL SCORE:':<40} {total_score:<20.4f}\n")
        f.write(f"{'OVERALL SCORE RATIO:':<40} {overall_ratio:<20.2f}%\n")

    print("Report generated successfully.")


if __name__ == "__main__":
    if compile_cpp():
        run_tests()