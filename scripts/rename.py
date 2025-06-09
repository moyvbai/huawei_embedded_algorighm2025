import os
import zipfile
from datetime import datetime

# 获取当前时间，格式为YYYYMMDD-HHMMSS
now = datetime.now().strftime('%Y%m%d-%H%M%S')

# 源文件路径
src_file = os.path.join(os.path.dirname(__file__), '../src/main.cpp')
# 目标zip文件名
zip_filename = f'{now}.zip'
# zip文件输出到submissions目录
zip_path = os.path.join(os.path.dirname(__file__), '../submissions', zip_filename)

with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
    zipf.write(src_file, arcname='main.cpp')

print(f'Created zip: {zip_path}')
