#!/bin/bash

RUN_PARAMS="-r 10000 20000 50000"

LOG_FILE="./run_output.log"
ROOT_DIR="../../"
EXEC_FILE="$ROOT_DIR/main.exe"
NAME="choimoe"
SUBMIT_DIR="submit"

# 编译C++程序
clang++ *.cpp -o $EXEC_FILE -std=c++17 -O2
# clang++ -fsanitize=address -g main.cpp -o $EXEC_FILE -std=c++17
if [ $? -ne 0 ]; then
    echo "build.sh: 编译失败，退出打包流程"
    exit 1
fi

# 运行测试并捕获输出
python $ROOT_DIR/run.py $ROOT_DIR/interactor/macos/interactor-live $ROOT_DIR/data/sample_official.in $EXEC_FILE $RUN_PARAMS 2>&1 | tee "$LOG_FILE"

# 解析输出结果
if grep -q '"error_code":"interaction_successful"' "$LOG_FILE"; then
    # 提取分数
    score=$(grep -o '"score":"[^"]*' "$LOG_FILE" | cut -d\" -f4)
    
    if [ -n "$score" ]; then
        # 生成日期格式（年.月.日，去除前导零）
        timestamp=$(date "+(%Y.%-m.%-d_%-H.%-M)" | sed 's/^-//g; s/ \?-\?/0/g')
        
        # 构建zip文件名
        zip_name="$NAME-${score}-${timestamp}.zip"
        
        mkdir -p $SUBMIT_DIR
        find . -type f \( -name "*.h" -o -name "*.cpp" -o -name "CMakeLists.txt" \) -print0 | \
            xargs -0 zip -r "$SUBMIT_DIR/$zip_name"
        
        echo "build.sh: 成功打包：$zip_name"
    else
        echo "build.sh: 警告：无法提取分数值"
    fi
else
    echo "build.sh: 测试未通过，不进行打包"
fi

# 清理临时文件
# rm -f "$LOG_FILE"