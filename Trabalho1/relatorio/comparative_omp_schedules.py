import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import pandas as pd

seq_csv = 'output_timesOmp_seq.txt'

seq_df = pd.read_csv(seq_csv)
seq_mean = seq_df['seq'].mean()
seq_std = seq_df['seq'].std()


schedules_data = {
    'static': 'output_timesOmp_static.txt',
    'guided5': 'output_timesOmp_guided5.txt',
    'guided20': 'output_timesOmp_guided20.txt',
    'dynamic5': 'output_timesOmp_dynamic5.txt',
    'dynamic20': 'output_timesOmp_dynamic20.txt',
}

records = []
for sched_name, fname in schedules_data.items():
    df = pd.read_csv(fname)
    for col in df.columns:
        records.append({
            'Schedule': sched_name,
            'Stage': col,
            'Mean': df[col].mean(),
            'Std': df[col].std()
        })

summary_df = pd.DataFrame(records)

stages = ['omp1_zona_contencao', 'omp2_calculate_metrics_reduce', 'omp3_upate_matrix']
stage_labels = ['OMP 1\n(Zona Contenção)', 'OMP 1+2\n(Cálculo Métricas)', 'OMP 1+2+3\n(Update Matrix)']
schedules = ['static', 'guided5', 'guided20', 'dynamic5', 'dynamic20']

x = np.arange(len(stages))  # 0, 1, 2
total_width = 0.75
bar_width = total_width / len(schedules)

colors = ['#2b5c8f', '#41b6c4', '#a1dab4', '#fecc5c', '#e31a1c']

sns.set_theme(style="whitegrid")
fig, ax = plt.subplots(figsize=(12, 6))

for i, sched in enumerate(schedules):
    sched_data = summary_df[summary_df['Schedule'] == sched]
    means = [sched_data[sched_data['Stage'] == st]['Mean'].values[0] for st in stages]
    stds = [sched_data[sched_data['Stage'] == st]['Std'].values[0] for st in stages]
    
    offset = x - (total_width / 2) + (i * bar_width) + (bar_width / 2)
    bars = ax.bar(offset, means, yerr=stds, width=bar_width, label=sched, color=colors[i], capsize=3, edgecolor='black', alpha=0.85)

ax.axhline(y=seq_mean, color='#555555', linestyle='--', linewidth=2.5, label=f'Baseline sequencial ({seq_mean:.4f} s)')
ax.axhspan(seq_mean - seq_std, seq_mean + seq_std, color='#888888', alpha=0.15)

ax.set_xticks(x)
ax.set_xticklabels(stage_labels, fontsize=11, fontweight='bold')
ax.set_ylabel('Tempo (seg)', fontsize=12, labelpad=10)
ax.set_title('Tempos de execução por múltiplos schedules', fontsize=14, pad=15, fontweight='bold')
ax.set_ylim(0, max(summary_df['Mean'].max(), seq_mean) * 1.25)
ax.legend(title='Schedule', frameon=True, facecolor='white', edgecolor='none')

plt.tight_layout()
plt.savefig('grouped_schedule_benchmark.png', dpi=300)
print("Plot successfully generated.")