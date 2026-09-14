from typing import List

import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap, BoundaryNorm
import matplotlib.patches as mpatches

def parse_simulation_file(filepath):
    """
    Parses continuous simulation matrix streams in the format:
    L C P
    <LxC matrix>
    L C P
    <LxC matrix>

    Returns:
        list of dict: Each element contains {'L': L, 'C': C, 'P': P, 'DATA': numpy_array}
    """
    data = []

    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    line_idx = 0
    num_lines = len(lines)

    while line_idx < num_lines:
        line = lines[line_idx].strip()

        if not line:
            line_idx += 1
            continue

        parts = line.split()

        if len(parts) == 3:
            L = int(parts[0])
            C = int(parts[1])
            P = int(parts[2])
            line_idx += 1

            matrix_rows = []
            for _ in range(L):
                if line_idx >= num_lines:
                    break
                row_data = [int(val) for val in lines[line_idx].strip().split()]
                matrix_rows.append(row_data)
                line_idx += 1

            # print(matrix_rows)
            matrix = np.array(matrix_rows, dtype=np.int32)

            data.append({
                'L': L,
                'C': C,
                'P': P,
                'DATA': matrix
            })
            continue

        line_idx += 1

    return data

def plot_simulation_step(matrix: List[List[int]], timestep: int, save_path: str):
    """
    Renderizar heatmap por passo temporal
    """

    colors = [
        '#808080',  # 0: Não combustível
        '#2ca02c',  # 1: Intacta
        '#d62728',  # 2: Em chamas
        '#222222',  # 3: Queimada
        '#1f77b4'   # 4: Contenção
    ]

    cmap = ListedColormap(colors=colors)

    bounds = [-0.5, 0.5, 1.5, 2.5, 3.5, 4.5]
    norm = BoundaryNorm(bounds, cmap.N)

    fig, ax = plt.subplots(figsize=(10,8), dpi=100)
    im = ax.imshow(X=matrix, cmap=cmap, norm=norm, interpolation='nearest')

    labels = {
        0: '0: Não combustivel',
        1: '1: Intacta',
        2: '2: Em chamas',
        3: '3: Queimada',
        4: '4: Contenção'
    }
    legend_patches = [
        mpatches.Patch(color=colors[i], label=labels[i]) for i in range(5)
    ]

    ax.legend(
        handles=legend_patches,
        bbox_to_anchor=(1.05, 1),
        loc='upper left',
        borderaxespad=0.,
        fontsize=11
    )

    ax.set_title(f"Simulação de Incêndio - Passo de Tempo: {timestep}", fontsize=14, fontweight='bold')
    ax.set_xlabel("Coluna (C)", fontsize=11)
    ax.set_ylabel("Linha (L)", fontsize=11)
    
    plt.tight_layout()

    if save_path:
        plt.savefig(save_path, bbox_inches='tight')
        plt.close()
    else:
        plt.show()


if __name__ == "__main__":
    filepath = "output.out"
    
    data = parse_simulation_file(filepath)
    # print(data[0], data[1], data[2])

    for i in [1,20,60,80]:
        timestep = i
        data_step = data[timestep]
        save_path = f'plot_{data_step['P']}'

        plot_simulation_step(data_step['DATA'], timestep=data_step['P'], save_path=save_path)