import argparse
import numpy as np
import random
import os
import json
import itertools
from collections import OrderedDict

def generate_case_from_config(config, output_file=None):
    """
    (此函数与上一版基本一致, 仅为完整性展示)
    根据给定的配置字典生成并打印完整的输入样例。
    """
    # --- 服务器信息 ---
    # N_SERVERS 现在根据 SERVER_SPECS 的长度动态确定
    n_servers = len(config["SERVER_SPECS"])
    print(n_servers, file=output_file)
    for spec in config["SERVER_SPECS"]:
        print(*spec, file=output_file)

    # --- 用户信息 ---
    m_users = config["M_USERS"]
    print(m_users, file=output_file)

    counts = np.random.normal(loc=config["CNT_MEAN"], scale=config.get("CNT_STD_DEV", 1000), size=m_users)
    counts = np.clip(counts, config["MIN_CNT"], config["MAX_CNT"]).astype(int)

    for i in range(m_users):
        cnt = counts[i]
        extra_factor = max(5.0, np.random.normal(loc=config.get("EXTRA_FACTOR_MEAN", 5.5), scale=config.get("EXTRA_FACTOR_STD", 0.5)))
        min_window_len = int(extra_factor * cnt)
        if (cnt <= 10): min_window_len += 50

        max_time = config["MAX_TIME"]
        max_s = max_time - min_window_len

        s = 0
        if max_s > 0:
            if config["S_DIST_MODE"] == "normal":
                s_mean = config.get("S_DIST_MEAN", max_time / 3)
                s_std = config.get("S_DIST_STD", max_time / 5)
                s = int(np.random.normal(loc=s_mean, scale=s_std))
            elif config["S_DIST_MODE"] == "uniform":
                s = random.randint(0, max_s)

        s = np.clip(s, 0, max_s)
        s = int(s)

        e = s + min_window_len
        e = min(e, max_time)

        if s >= e: s = e - 1

        print(s, e, cnt, file=output_file)

    # --- 通信延迟矩阵 ---
    for _ in range(n_servers):
        latencies = [random.randint(config["MIN_LATENCY"], config["MAX_LATENCY"]) for _ in range(m_users)]
        print(*latencies, file=output_file)

    # --- 显存公式参数 ---
    for _ in range(m_users):
        A, B = random.randint(10, 20), random.randint(100, 200)
        print(A, B, file=output_file)

def main():
    parser = argparse.ArgumentParser(
        description="Automatically generate diverse sample cases by enumerating parameter combinations from a JSON config file.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "config_file",
        type=str,
        help="Path to the JSON file defining the parameter space."
    )
    parser.add_argument(
        "-n", "--num_files",
        type=int,
        default=1,
        help="Number of sample files to generate for EACH parameter combination. Default is 1."
    )
    parser.add_argument(
        "-o", "--output_dir",
        type=str,
        default="generated_dataset",
        help="Specify the root output directory to save all generated files. Default is 'generated_dataset'."
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="The base random seed for reproducibility. Each file will use seed, seed+1, seed+2, ... ."
    )

    args = parser.parse_args()

    # --- 1. 读取并解析配置文件 ---
    with open(args.config_file, 'r', encoding='utf-8') as f:
        config_spec = json.load(f, object_pairs_hook=OrderedDict)

    base_config = config_spec.get("base_config", {})
    param_space = config_spec.get("parameter_space", {})

    param_names = list(param_space.keys())
    param_values_lists = list(param_space.values())

    # --- 2. 生成所有参数组合 ---
    all_combinations = list(itertools.product(*param_values_lists))
    total_combos = len(all_combinations)
    print(f"Found {len(param_names)} parameters to vary, generating {total_combos} combinations.")

    # --- 3. 遍历每种组合并生成文件 ---
    os.makedirs(args.output_dir, exist_ok=True)

    combo_count = 0
    for combo in all_combinations:
        combo_count += 1

        # a. 构建当前组合的完整配置
        current_config = base_config.copy()
        combo_dir_parts = []
        for i, param_name in enumerate(param_names):
            param_value = combo[i]
            current_config[param_name] = param_value
            # 创建一个对文件名友好的参数值字符串
            param_str_val = str(param_value).replace(" ", "").replace(",", "_").replace("[","").replace("]","").replace("(","").replace(")","")
            combo_dir_parts.append(f"{param_name}_{param_str_val}")

        # b. 创建描述性的子文件夹
        combo_dir_name = "-".join(combo_dir_parts)
        combo_path = os.path.join(args.output_dir, combo_dir_name)
        os.makedirs(combo_path, exist_ok=True)
        print(f"\n({combo_count}/{total_combos}) Generating for combination: {combo_dir_name}")

        # c. 为当前组合生成 n 个不同种子的文件
        for i in range(args.num_files):
            current_seed = args.seed + i

            # 设置随机种子
            random.seed(current_seed)
            np.random.seed(current_seed)

            file_path = os.path.join(combo_path, f"seed_{current_seed}.in")
            try:
                with open(file_path, "w") as f:
                    generate_case_from_config(current_config, output_file=f)
                print(f"  - Generated: {file_path}")
            except Exception as e:
                print(f"  - Failed to generate file for seed {current_seed}. Error: {e}")

    print(f"\n--- Generation finished. All files are in '{args.output_dir}' ---")

if __name__ == "__main__":
    main()