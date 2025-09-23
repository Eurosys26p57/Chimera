import matplotlib
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import matplotlib.patches as mpatches
import numpy as np
from module import colors,hatches,fs_labels
from matplotlib.ticker import MultipleLocator
import sys

matplotlib.rcParams['pdf.fonttype'] = 42
matplotlib.rcParams['ps.fonttype'] = 42

# fs_order_wr = ['ext4', 'p2cache', 'ext4_dax', 'nova', 'xcache']
# fs_order_rd = ['ext4_dax', 'nova', 'p2cache', 'tmpfs', 'xcache']
# modes = ['Unmodified', 'Encoding', 'Full-AT']
#modes = ['ARMore', 'CHBP (our work)', 'Native patching', 'Safer']
modes = ['Safer', 'ARMore', 'CHBP (our work)', 'Native patching']
modes_order = ['Native patching', 'Safer', 'ARMore', 'CHBP (our work)']

def parse_log_data(filepath):
    data = {}
    with open(filepath, 'r') as file:
        for line in file:
            entry = line.split()
            #print(entry)
            data[entry[0]] = []
            #for i in range(1,len(entry)):
            for i in [1,2,3,4,5]:
                data[entry[0]].append(float(entry[i]))
    data_percent = {}
    for benchmark in data.keys():
        data_percent[benchmark] = []
        for i in range(1, len(data[benchmark])):
            data_percent[benchmark].append(data[benchmark][i]/data[benchmark][0]-1)
    return data_percent

def scale_data(n):
    if n < 0.5:
        return n
    else:
        return 0.5+ (n)/10


def plot_data(ax: plt.Axes, modes, data : dict, ylim_n, ylim_p, xtick = True):
    # benchmarks = sorted(data.keys(), key=time_of_benchmark)
    benchmarks = data
    num_benchmarks = len(benchmarks)
    num_modes = len(modes)


    plot_data = {mode: [] for mode in modes}
    for benchmark in benchmarks:
        for i, mode in enumerate(modes):
            sc_data = scale_data(data[benchmark][i])
            plot_data[mode].append(sc_data)
            
    width = 0.8 / num_modes  # Width of each bar
    ind = np.arange(num_benchmarks)  # The x locations for the groups

    bars = []
    for i, mode in enumerate(modes_order):
        # Plot each mode
        bar = ax.bar(ind + i * width, plot_data[mode], width,
               label=fs_labels[mode], color='white', hatch=hatches[mode],
               edgecolor=colors[mode], linewidth=1.5, zorder=3)
        bar = ax.bar(ind + i * width, plot_data[mode], width,
               color='none', edgecolor='black', linewidth=1.5, zorder=3)
        bars.append(bar)

    ax.tick_params(axis='y', labelsize=22)
    if xtick:
        ax.tick_params(axis='x', labelsize=18)
        ax.set_xticks(ind + width * (num_modes - 1) /2, benchmarks, rotation=30)
    ax.grid(axis='y', linestyle='--', zorder=0)
    ax.set_ylim(ylim_n, ylim_p)
    #internal = ylim_p - ylim_n
    #if internal > 1:
    #    ax.yaxis.set_major_locator(MultipleLocator(0.4))  # 设置 y 轴间隔为 0.05
    #else:
    #    ax.yaxis.set_major_locator(MultipleLocator(0.05))  # 设置 y 轴间隔为 0.05
    #ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda y, _: '{:.0%}'.format(y)))
    #ax.axhline(y=break_position, color='black', linestyle='--', zorder=3)

    # 在 ax1 上添加断开的 y 轴
    #add_broken_y_axis(ax, low_ticks, high_ticks, low_labels, high_labels, break_position)
    #ax.set_ylim(ylim_n, ylim_p)


if len(sys.argv) < 2:
    print("need args")
else:
    dpath = sys.argv[1]  # 获取第一个用户参数
#data = parse_log_data('log_data/speccpuB.log')
data = parse_log_data('log_data/speccpu2B.log')
#print(data)
# fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(24, 7))
# fig, (ax1,ax2) = plt.subplots(1, 2, figsize=(24, 10))
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(24, 4), sharex=True)
fig.subplots_adjust(hspace=0.1)
plot_data(ax1, modes, data, 0.51, 1.0, False)
plot_data(ax2, modes, data, 0, 0.5)
ax1.tick_params(axis='x', bottom=False, labelbottom=False)
ax1.spines.bottom.set_visible(False)
ax2.spines.top.set_visible(False)
lowyticks = [0, 0.2, 0.4]
lowylabel = ['0%', '20%', '40%']
highyticks = [0.6, 0.8, 1.0]
highylabel = ['100%', '300%', '500%']
ax2.set_yticks(lowyticks)
ax2.set_yticklabels(lowylabel)
ax1.set_yticks(highyticks)
ax1.set_yticklabels(highylabel)
d = 0.5  # proportion of vertical to horizontal extent of the slanted line
kwargs = dict(marker=[(-1, -d), (1, d)], markersize=12,
              linestyle="none", color='k', mec='k', mew=1, clip_on=False)
ax1.plot([0, 1], [0, 0], transform=ax1.transAxes, **kwargs)
ax2.plot([0, 1], [1, 1], transform=ax2.transAxes, **kwargs)
#    ax.set_ylim(0, 0.5)
#    yticks = [0, 0.2, 0.4, 0.6, 0.8, 1.0]
#    low_labels = ['0%', '20%', '40%']
#    high_labels = ['100%', '300%', '500%']
#    ax.set_yticks(yticks)
#    ax.set_yticklabels(low_labels + high_labels)
#
#gs = gridspec.GridSpec(1, 2, width_ratios=[7, 12])  # 设置宽度比例 ax1:ax2 = 2:1


# data1 = parse_log_data('log_data/wr_nofsync.log', '1T_seq_write_')
#threshold = 0.3

# 分成两类
#category1 = {k: v for k, v in data.items() if (v[3] >= threshold or v[0] >= threshold)}  # content2 >= 0.3
#category2 = {k: v for k, v in data.items() if (v[3] < threshold and v[0] < threshold)}   # content2 < 0.3
#category1 = {k: v for k, v in data.items() if max(v) >= threshold}  # content2 >= 0.3
#category2 = {k: v for k, v in data.items() if max(v) < threshold} # content2 < 0.3
#category1 = {k: v for i, (k, v) in enumerate(data.items()) if i <= 6}  # content2 >= 0.3
#category2 = {k: v for i, (k, v) in enumerate(data.items()) if i > 6}   # content2 < 0.3
#print(category1)
#print(category2)
# 按 content2 排序
#sorted_category1 = dict(sorted(category1.items(), key=lambda item: item[1][1]))
#sorted_category2 = dict(sorted(category2.items(), key=lambda item: item[1][1]))

# 设置 y 轴分段

# 绘制数据
#plot_data(ax2, modes, sorted_category1, 0, 1.88)
# ax1.set_ylabel('Throughput (GiB/s)', fontsize=24)
# ax1.set_xlabel('(a) Write throughput w/o fsync', fontsize=24)
# data2 = parse_log_data('log_data/wr_fsync.log', '1T_seq_write_')
# plot_data(ax2, fs_order_wr, data2, 3)
# ax2.set_xlabel('(b) Write throughput w/ fsync', fontsize=24)
# data3 = parse_log_data('log_data/fio_read.log', '1T_seq_read_')
# plot_data(ax3, fs_order_wr, data3, 8)
# ax3.set_xlabel('(c) Read throughput', fontsize=24)
# # plt.legend(bbox_to_anchor=(0.5, 1), loc=8, borderaxespad=1, ncols=len(fs_order), fontsize=12)

#plt.subplots_adjust(left=0.05, right=0.98, wspace=0.1, top=0.85, bottom=0.3)
handles, labels = ax1.get_legend_handles_labels()
# handles3, labels3 = ax3.get_legend_handles_labels()
# for handle, label in zip(handles3, labels3):
#     if label not in labels:
#         handles.append(handle)
#         labels.append(label)
for i, label in enumerate(labels):
    if label == 'CHBP (our work)':  # 检查是否是 Chimera (our work)
        labels[i] = r"$\bf{CHBP\ (our\ work)}$"  # 使用 LaTeX 格式加粗并保留空格
    if label == 'Native patching':
        labels[i] = 'Strawman binary patching'
fig.legend(handles, labels, loc='upper center', ncols=6, fontsize=24, bbox_to_anchor=(0.5, 1.1))
fig.text(0.5, -0.25, 'Benchmark Runtime on SPEC CPU2017', ha='center', va='center', fontsize=24)
#fig.text(0.5, -0.25, 'Test Runtime on Real-world Application', ha='center', va='center', fontsize=24)
fig.text(0.075, 0.5, 'Perf. Degradation (%)', ha='center', va='center', rotation='vertical', fontsize=24)
plt.savefig('speccpuB.pdf', format='pdf', bbox_inches='tight')
#plt.savefig('realword.pdf', format='pdf', bbox_inches='tight')
