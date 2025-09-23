#!/bin/bash

cd speccpu2017-execute/speccpu2017
source ./shrc
ulimit -s unlimited
rm -rf logs/exp2b
mkdir -p logs/exp2b
runcpu -config=copy-rv64gcv-fullat 500.perlbench_r 502.gcc_r 520.omnetpp_r 523.xalancbmk_r 507.cactuBSSN_r 510.parest_r 521.wrf_r 526.blender_r 527.cam4_r 538.imagick_r 
rm -rf logs/exp2b/fullat
mv result logs/exp2b/fullat
runcpu -config=copy-rv64gcv-chimera 500.perlbench_r 502.gcc_r 520.omnetpp_r 523.xalancbmk_r 507.cactuBSSN_r 510.parest_r 521.wrf_r 526.blender_r 527.cam4_r 538.imagick_r 
rm -rf logs/exp2b/chimera
mv result logs/exp2b/chimera
runcpu -config=copy-rv64gcv 500.perlbench_r 502.gcc_r 520.omnetpp_r 523.xalancbmk_r 507.cactuBSSN_r 510.parest_r 521.wrf_r 526.blender_r 527.cam4_r 538.imagick_r 
rm -rf logs/exp2b/base
mv result logs/exp2b/base
runcpu -config=copy-rv64gcv-armore 500.perlbench_r 502.gcc_r 520.omnetpp_r 523.xalancbmk_r 507.cactuBSSN_r 510.parest_r 521.wrf_r 526.blender_r 527.cam4_r 538.imagick_r 
rm -rf logs/exp2b/armore
mv result logs/exp2b/armore
runcpu -config=copy-rv64gcv-text1mb 500.perlbench_r 502.gcc_r 520.omnetpp_r 523.xalancbmk_r 507.cactuBSSN_r 510.parest_r 521.wrf_r 526.blender_r 527.cam4_r 538.imagick_r 
rm -rf logs/exp2b/text1mb
mv result logs/exp2b/text1mb
runcpu -config=copy-rv64gcv-encode 500.perlbench_r 502.gcc_r 520.omnetpp_r 523.xalancbmk_r 507.cactuBSSN_r 510.parest_r 521.wrf_r 526.blender_r 527.cam4_r 538.imagick_r 
rm -rf logs/exp2b/encode
mv result logs/exp2b/encode

cp -r logs logsexp2Bbk

RES_FILE="logs/exp2b/res.txt"
for dir in logs/exp2b/base logs/exp2b/fullat logs/exp2b/armore logs/exp2b/chimera; do
    if [ -d "$dir" ]; then
        # 查找目录中的CSV文件
        csv_file=$(find "$dir" -name "*.csv" | head -n 1)
        if [ -n "$csv_file" ]; then
            # 运行getres.py并捕获输出
            # 注意：这里假设getres.py在当前目录或PATH中可用
            echo "test $csv_file"
            output=$(python3 getres.py < "$csv_file" 2>/dev/null)
            # 将结果写入res.txt
            echo "$(basename $dir): $output" >> "$RES_FILE"
        else
            echo "$(basename $dir): 未找到CSV文件" >> "$RES_FILE"
        fi
    else
        echo "$(basename $dir): 目录不存在" >> "$RES_FILE"
    fi
done

echo "处理完成，结果保存在 $RES_FILE"

cd $HOME
rm -rf ./figures
mkdir figures
cd ISA_trans
python3 caculateB.py
python3 speccpu.py
mv speccpu.pdf /home/eurosys2026/figures/exp2B.pdf
