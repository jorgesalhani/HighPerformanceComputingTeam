import pandas as pd
import matplotlib.pyplot as plt

def plot_simulation_metrics(csv_filepath, save_path=None):
    df = pd.read_csv(csv_filepath)

    df.columns = df.columns.str.strip()

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(11, 10), sharex=True, dpi=100)

    # ax1.set_yscale('log')
    ax1.plot(df['PASSO'], df['INTACTAS'], label='Intactas', color='#2ca02c', linewidth=2)
    ax1.plot(df['PASSO'], df['EM_CHAMAS'], label='Em Chamas', color='#d62728', linewidth=2)
    ax1.plot(df['PASSO'], df['QUEIMADAS'], label='Queimadas', color='#222222', linewidth=2)
    ax1.plot(df['PASSO'], df['CONTENCAO'], label='Contenção', color='#1f77b4', linewidth=2, linestyle='--')
    ax1.plot(df['PASSO'], df['NAO_COMBUSTIVEIS'], label='Não Combustíveis', color='#808080', linewidth=1.5, linestyle=':')
    ax1.plot(df['PASSO'], df['COMBUSTIVEIS'], label='Combustíveis', color="#FDB312", linewidth=1.5, linestyle=':')

    ax1.set_title('Evolução da Matriz de Estados', fontsize=13, fontweight='bold')
    ax1.set_ylabel('Quantidade de Células', fontsize=11)
    ax1.grid(True, linestyle='--', alpha=0.6)
    ax1.legend(loc='best', framealpha=0.9)

    ax2.set_yscale('log')
    ax2.plot(df['PASSO'], df['INTACTAS'], label='Intactas', color='#2ca02c', linewidth=2)
    ax2.plot(df['PASSO'], df['EM_CHAMAS'], label='Em Chamas', color='#d62728', linewidth=2)
    ax2.plot(df['PASSO'], df['QUEIMADAS'], label='Queimadas', color='#222222', linewidth=2)
    ax2.plot(df['PASSO'], df['CONTENCAO'], label='Contenção', color='#1f77b4', linewidth=2, linestyle='--')
    ax2.plot(df['PASSO'], df['NAO_COMBUSTIVEIS'], label='Não Combustíveis', color='#808080', linewidth=1.5, linestyle=':')
    ax2.plot(df['PASSO'], df['COMBUSTIVEIS'], label='Combustíveis', color="#FDB312", linewidth=1.5, linestyle=':')

    ax2.set_title('Evolução da Matriz de Estados', fontsize=13, fontweight='bold')
    ax2.set_ylabel('Quantidade de Células', fontsize=11)
    ax2.grid(True, linestyle='--', alpha=0.6)
    ax2.legend(loc='best', framealpha=0.9)


    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, bbox_inches='tight')
        plt.close()
        print(f"Gráfico salvo em: {save_path}")
    else:
        plt.show()


if __name__ == "__main__":
    csv_file = "output.out"
    save_path = "time_series"
    plot_simulation_metrics(csv_filepath=csv_file, save_path=save_path)