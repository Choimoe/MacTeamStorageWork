#!/bin/bash

LOG_FILE="./run_output.log"

# 编译C++程序
clang++ main.cpp -o ../../main.exe -std=c++17 -O2
# clang++ -fsanitize=address -g main.cpp -o ../../main.exe -std=c++17
if [ $? -ne 0 ]; then
    echo "build.sh: 编译失败，退出打包流程"
    exit 1
fi

# 运行测试并捕获输出
python ../../run.py ../../interactor/macos/interactor ../../data/sample_practice.in ../../main.exe -r 10000 20000 50000 2>&1 | tee "$LOG_FILE"

# 解析输出结果
if grep -q '"error_code":"interaction_successful"' "$LOG_FILE"; then
    # 提取分数
    score=$(grep -o '"score":"[^"]*' "$LOG_FILE" | cut -d\" -f4)
    
    if [ -n "$score" ]; then
        # 生成日期格式（年.月.日，去除前导零）
        timestamp=$(date "+(%Y.%-m.%-d_%-H.%-M)" | sed 's/^-//g; s/ \?-\?/0/g')
        
        # 构建zip文件名
        zip_name="score-${score}-${timestamp}.zip"
        
        # 打包文件（假设源文件在demos/cpp目录下）
        zip -r "$zip_name" ./*.h ./*.cpp ./CMakeLists.txt
        
        echo "build.sh: 成功打包：$zip_name"
    else
        echo "build.sh: 警告：无法提取分数值"
    fi
else
    echo "build.sh: 测试未通过，不进行打包"
fi

# 清理临时文件
# rm -f "$LOG_FILE"