import io
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Load data into pandas
df = pd.read_csv('output_timesOmp.txt')

# Compute mean and standard deviation for all columns
means = df.mean()
std_errs = df.std()

# Define clean display labels
labels = [
    'Sequencial\n(Total)',
    'OMP 1\n(Zona Contenção)',
    'OMP 1+2\n(Metrics Reduce)',
    'OMP 1+2+3\n(Update Matrix)',
    'OMP 1+2+3\n(Run simulation - Persistente)'
]

# Color palette: Neutral gray for Sequential, distinct colors for OpenMP stages
colors = ['#7f7f7f', '#2b5c8f', '#d95f02', '#7570b3', "#60c664"]

# Set seaborn design style
sns.set_theme(style="whitegrid")
plt.figure(figsize=(10, 6))

# Plot bars with error bars (standard deviation)
bars = plt.bar(
    labels,
    means,
    yerr=std_errs,
    capsize=5,
    color=colors,
    edgecolor='black',
    alpha=0.88,
    width=0.55
)

# Render formatted mean value text on each bar
for bar in bars:
    height = bar.get_height()
    plt.text(
        bar.get_x() + bar.get_width() / 2.0,
        height / 2.0,
        f'{height:.4f} s',
        ha='center',
        va='center',
        color='white',
        fontsize=11,
        weight='bold'
    )

# Formatting chart titles and axes
plt.title('Tempos de execução: ', fontsize=14, pad=15, weight='bold')
plt.ylabel('Tempo (seg)', fontsize=12, labelpad=10)
plt.ylim(0, max(means) * 1.25)

plt.tight_layout()
plt.savefig('benchmark_bars_seq_first.png', dpi=300)
plt.show()