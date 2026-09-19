#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Definições de foco de incêndio
 * - Coordenadas de célula em uma matriz
 * 
 * =================
 * L: linha (matrix)
 * C: coluna (matrix)
 */
typedef struct {
  int L, C;
} FocoIncendio ;

/**
 * Definições de zonas de contenção
 * - Coordenadas de retângulo (limites inclusos) em uma matriz
 * 
 * =================
 * PASSO_ATIVACAO: instante de ativação
 * LI: linha inicial
 * CI: coluna inicial
 * LF: linha final
 * CF: coluna final
 */
typedef struct {
  int PASSO_ATIVACAO;
  int LI, CI, LF, CF;
} ZonaContencao;

/**
 * Definições de célula
 * 
 * ====================
 * ID_ESTADO: Código do estado da célula
 * ID_COBERTURA: Código da cobertura da celula
 * FATOR_INCENDIO: Fator para inicio de incendios, relativo ao tipo de cobertura
 * UMIDADE: Valor de umidade da celula
 * TEMPO_QUEIMA: Total de passos até transitar de estado 2 -> 3 (em chamas -> queimada)
 */
typedef struct {
  int ID_ESTADO;
  int ID_COBERTURA;
  int FATOR_INCENDIO;
  int UMIDADE;
  int TEMPO_QUEIMA;
} Celula;


/**
 * Vetor de tempo com métricas
 * 
 * ===========================
 * PASSO: Valor incremental
 * NAO_COMBUSTIVEIS: total de células em estado 0 (não combustível)
 * COMBUSTIVEIS: total de celulas com cobertura 2 ou 3 (cobertura de vegestação rasteira ou floresta)
 * INTACTAS: total de células em estado 1 (intacta)
 * EM_CHAMAS: total de células em estado 2 (em chamas)
 * QUEIMADAS: total de células em estado 3 (queimada)
 * CONTENCAO: total de células em estado 4 (contenção)
 * TOTAL_IGNICOES: total de ignoções ocorridas no passo atual
 * PERCENTUAL_QUEIMADO: perc. de queimadas em relação ao estado inicial
 * PERCENTUAL_PROTEGIDO: perc. de celulas de contenção em relação aos combustiveis no estado inicial
 */
typedef struct {
  int PASSO;
  int COMBUSTIVEIS;
  int NAO_COMBUSTIVEIS;
  int INTACTAS;
  int EM_CHAMAS;
  int QUEIMADAS;
  int CONTENCAO;
  int TOTAL_IGNICOES;
  float PERCENTUAL_QUEIMADO;
  float PERCENTUAL_PROTEGIDO;
} Metrics;

typedef struct {
  unsigned int L, C, P, T, SEED, LIMIAR;
  int VENTO_LINHA, VENTO_COLUNA, V;
  int F, Z;
  FocoIncendio** FOCOS_INCENDIO;
  ZonaContencao** ZONAS_CONTENCAO;
} InputConfigs;


// Definições de funções
// =====================


InputConfigs* load_input_configs(const char* filepath);
void free_input_configs(InputConfigs* configs);

void print_loaded_input_configs(InputConfigs* configs);

bool is_valid_input_configs(InputConfigs* configs);
bool is_valid_single_input_argument(int argc);

bool is_valid_configs_first_line(InputConfigs* configs);
bool is_valid_configs_vento(InputConfigs* configs);
bool is_valid_F_Z(InputConfigs* configs);
bool is_valid_matrix_focos_incendio(InputConfigs* configs);
bool is_valid_matrix_zonas_contencao(InputConfigs* configs);
bool is_valid_foco_incendio_sobre_celula_combustivel(InputConfigs* configs, int id_cobertura, unsigned long long);

Celula *build_linear_state_matrix(InputConfigs* configs);


bool is_valid_configs_first_line(InputConfigs* configs) {
  if (configs->L <= 0) {
    printf("L > 0. Informado: L = %d\n", configs->L);
    return false;
  }
  if (configs->C <= 0) {
    printf("C > 0. Informado: C = %d\n", configs->C);
    return false;
  }
  if (configs->P < 0) {
    printf("P >= 0. Informado: P = %d\n", configs->P);
    return false;
  }
  if (configs->T < 0) {
    printf("T >= 0. Informado: T = %d\n", configs->T);
    return false;
  }
  if (configs->LIMIAR <= 0) {
    printf("LIMIAR > 0. Informado: LIMIAR = %d\n", configs->LIMIAR);
    return false;
  }
  return true;
}

InputConfigs* load_input_configs(const char* filepath) {
  FILE* file = fopen(filepath, "r");

  if (!file) {
    perror("Erro ao abrir arquivo\n");
    return NULL;
  }

  InputConfigs* configs = malloc(sizeof(InputConfigs));
  if (!configs) {
    perror("Erro ao armazenar estrutura de configuração\n");
    fclose(file);
    free(configs);
    return NULL;
  }

  int fscanf_return;
  fscanf_return = fscanf(
    file, 
    "%d %d %d %d %d %d", 
    &configs->L, &configs->C, &configs->P, &configs->T,
    &configs->SEED, &configs->LIMIAR
  );

  if (fscanf_return !=6) {
    perror("Erro em leitura de parâmetros de arquivo de entrada. Configurações da linha 1\n");
    fclose(file);
    free(configs);
    return NULL;
  }

  fscanf_return = fscanf(
    file,
    "%d %d %d",
    &configs->VENTO_LINHA, &configs->VENTO_COLUNA, &configs->V
  );

  if (fscanf_return != 3) {
    perror("Erro em leitura de parâmetros de arquivo de entrada. Configurações da linha 2\n");
    fclose(file);
    free(configs);
    return NULL;
  }

  fscanf_return = fscanf(
    file,
    "%d %d",
    &configs->F, &configs->Z
  );

  if (fscanf_return != 2) {
    perror("Erro em leitura de parâmetros de arquivo de entrada. Configurações da linha 3\n");
    fclose(file);
    free(configs);
    return NULL;
  }

  if (!is_valid_F_Z(configs)) {
    fclose(file);
    free(configs);
    return NULL;
  }

  configs->FOCOS_INCENDIO = malloc(configs->F * sizeof(FocoIncendio*));
  for (int i = 0; i < configs->F; i++) {
    configs->FOCOS_INCENDIO[i] = malloc(sizeof(FocoIncendio));
  }
  
  for (int i = 0; i < configs->F; i++) {
    fscanf_return = fscanf(
      file, 
      "%d %d",
      &configs->FOCOS_INCENDIO[i]->L,
      &configs->FOCOS_INCENDIO[i]->C
    );

    if (fscanf_return != 2) {
      perror("Erro em leitura de parâmetros de arquivo de entrada. Configurações da coordenadas de focos de incêndio\n");
      fclose(file);
      free_input_configs(configs);
      return NULL;
    }
  }

  configs->ZONAS_CONTENCAO = malloc(configs->Z * sizeof(ZonaContencao*));
  for (int i = 0; i < configs->Z; i++) {
    configs->ZONAS_CONTENCAO[i] = malloc(sizeof(ZonaContencao));
  }

  for (int i = 0; i < configs->Z; i++) { 
    fscanf_return = fscanf(
      file,
      "%d %d %d %d %d",
      &configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO,
      &configs->ZONAS_CONTENCAO[i]->LI,
      &configs->ZONAS_CONTENCAO[i]->CI,
      &configs->ZONAS_CONTENCAO[i]->LF,
      &configs->ZONAS_CONTENCAO[i]->CF
    );

    if (fscanf_return != 5) {
      perror("Erro em leitura de parâmetros de arquivo de entrada. Configurações da coordenadas de zonas de contenção\n");
      fclose(file);
      free_input_configs(configs);
      return NULL;
    }
  }

  fclose(file);
  return configs;
}

void free_input_configs(InputConfigs* configs) {
  if (!configs) return;

  if (configs->FOCOS_INCENDIO) {
    for (int i = 0; i < configs->F; i++) {
      free(configs->FOCOS_INCENDIO[i]);
    }
    free(configs->FOCOS_INCENDIO);
  }

  if (configs->ZONAS_CONTENCAO) {
    for (int i = 0; i < configs->Z; i++) {
      free(configs->ZONAS_CONTENCAO[i]);
    }
    free(configs->ZONAS_CONTENCAO);
  }

  free(configs);
}

void print_loaded_input_configs(InputConfigs* configs) {
  printf("=== Input Configuration Loaded ===\n");
  printf("Matrix (LxC) | Steps (P) | Threads (T) | Seed | Limiar\n");
  printf("%dx%d | %d | %d | %d | %d\n",
          configs->L, configs->C, configs->P, configs->T, configs->SEED, configs->LIMIAR);
  printf("Vento (direção L, C) | V\n");
  printf("%d,%d | %d\n",
          configs->VENTO_LINHA, configs->VENTO_COLUNA, configs->V);
  printf("Focos (F): %d:\n", configs->F);
  for (int i = 0; i < configs->F; i++) {
    printf("  - %d: %d, %d\n", i + 1, configs->FOCOS_INCENDIO[i]->L, configs->FOCOS_INCENDIO[i]->C);
  }
  printf("Zonas (Z): %d:\n", configs->Z);
  for (int i = 0; i < configs->Z; i++) {
      printf("  - %d: %d, [%d, %d] [%d, %d]\n",
        i + 1,
        configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO,
        configs->ZONAS_CONTENCAO[i]->LI, configs->ZONAS_CONTENCAO[i]->CI,
        configs->ZONAS_CONTENCAO[i]->LF, configs->ZONAS_CONTENCAO[i]->CF
      );
  }
}

bool is_valid_F_Z(InputConfigs* configs) {
  if (configs->F < 0) {
    printf("F >= 0. Informado: F = %d\n", configs->F);
    return false;
  }
  if (configs->Z < 0) {
    printf("Z >= 0. Informado: Z = %d\n", configs->Z);
    return false;
  }
  return true;
}

bool is_valid_matrix_focos_incendio(InputConfigs* configs) {
  if (configs->F == 0) return true;

  int L = configs->FOCOS_INCENDIO[0]->L;
  int C = configs->FOCOS_INCENDIO[0]->C;

  for (int i = 0; i < configs->F; i++) {
    if (
      (configs->FOCOS_INCENDIO[i]->L < 0 || configs->FOCOS_INCENDIO[i]->C < 0) ||
      (configs->FOCOS_INCENDIO[i]->L >= configs->L || configs->FOCOS_INCENDIO[i]->C >= configs->C)
    ) {
      printf(
        "Focos de incêndio devem estar dentro da matriz. Informado: (%d, %d)\n", 
        configs->FOCOS_INCENDIO[i]->L, configs->FOCOS_INCENDIO[i]->C
      );
      return false;
    }

    if (i != 0 && (configs->FOCOS_INCENDIO[i]->L == L && configs->FOCOS_INCENDIO[i]->C == C)) {
      printf(
        "Focos não devem estar duplicados. Informado: (%d, %d)\n", 
        configs->FOCOS_INCENDIO[i]->L, configs->FOCOS_INCENDIO[i]->C
      );
      return false;
    }

    L = configs->FOCOS_INCENDIO[i]->L;
    C = configs->FOCOS_INCENDIO[i]->C;
  }

  return true;
}

bool is_valid_matrix_zonas_contencao(InputConfigs* configs) {

  if (configs->Z == 0) return true;

  for (int i = 0; i < configs->Z; i++) {
    if (
      (configs->ZONAS_CONTENCAO[i]->LI < 0 || configs->ZONAS_CONTENCAO[i]->CI < 0) ||
      (configs->ZONAS_CONTENCAO[i]->LI >= configs->L || configs->ZONAS_CONTENCAO[i]->CI >= configs->C)
    ) {
      printf(
        "Zonas de contenção devem estar dentro da matriz. Informado: (%d, %d)\n", 
        configs->ZONAS_CONTENCAO[i]->LI, configs->ZONAS_CONTENCAO[i]->CI
      );
      return false;
    }

    if (
      (configs->ZONAS_CONTENCAO[i]->LF < 0 || configs->ZONAS_CONTENCAO[i]->CF < 0) ||
      (configs->ZONAS_CONTENCAO[i]->LF >= configs->L || configs->ZONAS_CONTENCAO[i]->CF >= configs->C)
    ) {
      printf(
        "Zonas de contenção devem estar dentro da matriz. Informado: (%d, %d)\n", 
        configs->ZONAS_CONTENCAO[i]->LF, configs->ZONAS_CONTENCAO[i]->CF
      );
      return false;
    }

    if (
      (configs->ZONAS_CONTENCAO[i]->LI > configs->ZONAS_CONTENCAO[i]->LF) || 
      (configs->ZONAS_CONTENCAO[i]->CI > configs->ZONAS_CONTENCAO[i]->CF)
    ) {
      printf(
        "(LI, CI) < (LF, CF). Informado: (%d, %d), (%d, %d)\n", 
        configs->ZONAS_CONTENCAO[i]->LI, configs->ZONAS_CONTENCAO[i]->CI,
        configs->ZONAS_CONTENCAO[i]->LF, configs->ZONAS_CONTENCAO[i]->CF
      );
      return false;
    }

    if (configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO < 0 || configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO >= configs->P) {
      printf(
        "0 <= PASSO_ATIVACAO < P . Informado: %d\n", 
        configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO
      );
      return false;
    }
  }

  return true;
}

bool is_valid_configs_vento(InputConfigs* configs) {
  if (
      (configs->VENTO_LINHA < -1 || configs->VENTO_LINHA > 1) || 
      (configs->VENTO_COLUNA < -1 || configs->VENTO_COLUNA > 1)
    ) { 
      printf("-1 <= VENTO_LINHA <= 1 e -1 <= VENTO_COLUNA <= 1. Informado: (L,C) = (%d,%d)\n", configs->VENTO_LINHA, configs->VENTO_COLUNA);
      return false;
  }
  
  if (configs->VENTO_LINHA == 0 && configs->VENTO_COLUNA == 0){ 
    printf("(VENTO_LINHA, VENTO_COLUNA) != (0,0). Informado: (L,C) = (%d,%d)\n", configs->VENTO_LINHA, configs->VENTO_COLUNA);
    return false;
  }

  if (configs->V < 0 || configs->V > 5) {
    printf("0 <= V <= 5. Informado: V = %d\n", configs->V);
    return false;
  }

  return true;
}

bool is_valid_input_configs(InputConfigs* configs) {
  return is_valid_configs_first_line(configs) &&
    is_valid_configs_vento(configs) &&
    is_valid_matrix_focos_incendio(configs) && 
    is_valid_matrix_zonas_contencao(configs);
}

bool is_valid_single_input_argument(int argc) {
  return argc == 2 ? true : false;
}

int generate_cobertura(InputConfigs* configs) {
  int valor = rand_r(&configs->SEED) % 100;
  if (valor <= 9) return 0;
  if (valor <= 19) return 1;
  if (valor <= 54) return 2;
  return 3;
}

int generate_fator_incendio(int id_cobertura) {
  if (id_cobertura == 0 || id_cobertura == 1) return 0;
  if (id_cobertura == 2) return 8;
  return 12;
}

int generate_umidade(InputConfigs* configs) {
  return rand_r(&configs->SEED) % 101;
}

int generate_estado(int id_cobertura) {
  return (id_cobertura == 0 || id_cobertura == 1) ? 0 : 1;
}

unsigned long long calculate_linear_matrix_index(int row, int col, int C) {
  return (unsigned long long) row * C + col;
}

bool is_valid_foco_incendio_sobre_celula_combustivel(InputConfigs* configs, int id_cobertura, unsigned long long idx) {
  if (id_cobertura == 2 || id_cobertura == 3) return true;

  int i = idx / configs->C;
  int j = idx % configs->C;
  for (int k = 0; k < configs->F; k++) {
    if (configs->FOCOS_INCENDIO[k]->L == i && configs->FOCOS_INCENDIO[k]->C == j) {
      printf("Focos posicionados sobre células combustíveis. Informado: (L,C) = (%d,%d)\n", i, j);
      return false;
    }
  }

  return true;
}

bool populate_matrix(InputConfigs* configs, Celula *matrix) {
  if (!configs || !matrix) return false;

  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    int id_cobertura = generate_cobertura(configs);
    
    matrix[i].ID_COBERTURA = id_cobertura;
    matrix[i].FATOR_INCENDIO = generate_fator_incendio(id_cobertura);
    matrix[i].UMIDADE = generate_umidade(configs);
    matrix[i].ID_ESTADO = generate_estado(id_cobertura);
    matrix[i].TEMPO_QUEIMA = 0;

    if (!is_valid_foco_incendio_sobre_celula_combustivel(configs, id_cobertura, i)) return false;
  }

  return true;
}

Celula *copy_matrix(InputConfigs* configs, Celula *matrix) {
  if (!configs || !matrix) return false;
  Celula *cp_matrix = build_linear_state_matrix(configs);

  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    cp_matrix[i].ID_COBERTURA = matrix[i].ID_COBERTURA;
    cp_matrix[i].FATOR_INCENDIO = matrix[i].FATOR_INCENDIO;
    cp_matrix[i].UMIDADE = matrix[i].UMIDADE;
    cp_matrix[i].ID_ESTADO = matrix[i].ID_ESTADO;
    cp_matrix[i].TEMPO_QUEIMA = matrix[i].TEMPO_QUEIMA;
  }

  return cp_matrix;
}

void print_state_matrix(InputConfigs* configs, Celula* matrix, int metric) {
  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    if (i % configs->C  == 0) printf("\n");
    switch (metric){
      case 0:
        printf("%d ", matrix[i].ID_ESTADO);
        break;
      case 1:
        printf("%d ", matrix[i].ID_COBERTURA);
        break;
      case 2:
        printf("%d ", matrix[i].FATOR_INCENDIO);
        break;
      case 3:
        printf("%d ", matrix[i].UMIDADE);
        break;
      case 4:
        printf("%d ", matrix[i].TEMPO_QUEIMA);
        break;
      
      default:
        break;
    }
  }
  printf("\n");
}

void free_simulation_matrix(InputConfigs* configs, Celula* matrix) {
  if (!matrix) return;
  free(matrix);
}

Celula *build_linear_state_matrix(InputConfigs* configs) {
  Celula *matrix = malloc((size_t) configs->L * configs->C * sizeof(Celula));
  if (!matrix) return NULL;

  return matrix;
}


Metrics *build_metrics_vector(InputConfigs* configs) {
  Metrics *vector = malloc(configs->L * configs->C * sizeof(Metrics));
  if (!vector) return NULL;

  for (unsigned long long i = 0; i < (configs->L * configs->C); i++) {
    vector[i].CONTENCAO = 0;
    vector[i].EM_CHAMAS = 0;
    vector[i].INTACTAS = 0;
    vector[i].NAO_COMBUSTIVEIS = 0;
    vector[i].COMBUSTIVEIS = 0;
    vector[i].PASSO = 0;
    vector[i].PERCENTUAL_PROTEGIDO = 0;
    vector[i].PERCENTUAL_QUEIMADO = 0;
    vector[i].QUEIMADAS = 0;
    vector[i].TOTAL_IGNICOES = 0;
  }
  return vector;
}

void free_metrics_vector(Metrics *vector) {
  if (!vector) return;
  free(vector);
}

void free_mapa_contencao_vector(int *vector) {
  if (!vector) return;
  free(vector);
}

void apply_focos_iniciais_incendio(InputConfigs* configs, Celula* matrix) {
  for (int i = 0; i < configs->F; i++) {
    unsigned long long idx = calculate_linear_matrix_index(configs->FOCOS_INCENDIO[i]->L, configs->FOCOS_INCENDIO[i]->C, configs->C);
    matrix[idx].ID_ESTADO = 2;

    if (matrix[idx].ID_COBERTURA == 2) {
      matrix[idx].TEMPO_QUEIMA = 2;
      continue;
    }

    matrix[idx].TEMPO_QUEIMA = 4;
  }
}

int get_passo_ativacao_if_cell_in_zona_contencao(int row, int col, InputConfigs* configs) {
  int min_passo_ativacao = configs->P;
  for (int i = 0; i < configs->Z; i++) {
    if (
      (row >= configs->ZONAS_CONTENCAO[i]->LI && row <= configs->ZONAS_CONTENCAO[i]->LF) &&
      (col >= configs->ZONAS_CONTENCAO[i]->CI && col <= configs->ZONAS_CONTENCAO[i]->CF)
    ) {
      min_passo_ativacao = configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO < min_passo_ativacao ? configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO : min_passo_ativacao;
    }
  }
  return min_passo_ativacao == configs->P ? -1 : min_passo_ativacao;
}

int *build_mapa_contencao(InputConfigs* configs, Celula* matrix) {
  int *ativacao = (int*) malloc((size_t) configs->L * configs->C * sizeof(int));
  if (!ativacao) return NULL;

  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    int rowi = i / configs->C;
    int coli = i % configs->C;
    ativacao[i] = get_passo_ativacao_if_cell_in_zona_contencao(rowi, coli, configs);
  }
  return ativacao;
}

/**
 * ATENÇÃO: função NÃO tocada por mim — não é a minha parte. Continua
 * sequencial, igual estava. Fica pra quem ficou responsável por ela.
 */
void activate_zonas_contencao(InputConfigs* configs, Celula* matrix, int *vetor_ativacao, int p) {
  if (!configs || !matrix || !vetor_ativacao) return;

  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    if (vetor_ativacao[i] != p) continue;
    if (matrix[i].ID_ESTADO != 1) continue;
    matrix[i].ID_ESTADO = 4;
  }
}

bool check_in_matrix_boundaries(
  InputConfigs* configs,
  int neighbor_row,
  int neighbor_col
) {
  return (
    (neighbor_row >= 0 && neighbor_row < configs->L) &&
    (neighbor_col >= 0 && neighbor_col < configs->C)
  );
}

/**
 * >>> MINHA PARTE (implementada/corrigida) <<<
 *
 * CORRIGIDO em relação à versão anterior: o cálculo original obtinha o
 * vizinho por OFFSET LINEAR (idx_atual + vizinhos_moore[i]) e só depois
 * decodificava linha/coluna via divisão/módulo. Isso faz com que, para
 * células na coluna 0 ou C-1, o "vizinho oeste"/"vizinho leste" (e os
 * diagonais correspondentes) vazem para a linha anterior/seguinte da
 * matriz (ex.: coluna 0 com offset -1 cai na última coluna da linha de
 * cima) — e check_in_matrix_boundaries não detecta isso porque o índice
 * decodificado PARECE válido. A correção calcula linha/coluna do vizinho a
 * partir de deltas (di,dj) e valida ANTES de transformar em índice linear.
 */
int calculate_potencial_ignicao(InputConfigs* configs, int idx_atual, Celula* matrix_atual) {
  int C = configs->C;

  int linha_celula = idx_atual / C;
  int coluna_celula = idx_atual % C;

  static const int DELTA_LINHA[8]  = {-1, -1, -1,  0, 0,  1, 1, 1};
  static const int DELTA_COLUNA[8] = {-1,  0,  1, -1, 1, -1, 0, 1};

  int S = 0;

  for (int k = 0; k < 8; k++) {
    int linha_vizinho = linha_celula + DELTA_LINHA[k];
    int coluna_vizinho = coluna_celula + DELTA_COLUNA[k];

    if (!check_in_matrix_boundaries(configs, linha_vizinho, coluna_vizinho)) continue;

    int idx_vizinho = linha_vizinho * C + coluna_vizinho;

    if (matrix_atual[idx_vizinho].ID_ESTADO != 2) continue;

    int prop_linha = linha_celula - linha_vizinho;
    int prop_coluna = coluna_celula - coluna_vizinho;

    int peso_basico = (abs(prop_linha) + abs(prop_coluna) == 1) ? 10 : 7;

    int A = prop_linha*configs->VENTO_LINHA + prop_coluna*configs->VENTO_COLUNA;

    int peso_calculado_int_a = peso_basico + (configs->V * A);
    int peso_vizinho = peso_calculado_int_a > 1 ? peso_calculado_int_a : 1;

    S += peso_vizinho;
  }

  return (S * matrix_atual[idx_atual].FATOR_INCENDIO * (100 - matrix_atual[idx_atual].UMIDADE)) / 100 ;
}

/**
 * >>> MINHA PARTE (implementada/corrigida + paralelizada) <<<
 *
 * CORRIGIDO: a versão original testava/decrementava
 * matrix_proximo[i].TEMPO_QUEIMA, mas matrix_proximo é o buffer de DUAS
 * iterações atrás (reaproveitado como scratch após a troca de ponteiros no
 * laço principal da simulação) e nunca era resetado — ou seja, o tempo de
 * queima "decrementava" a partir de lixo, não do tempo real da célula no
 * passo atual. A correção decrementa sempre a partir de
 * matrix_atual[i].TEMPO_QUEIMA (o valor correto e atual) e escreve o
 * resultado em matrix_proximo. Também passamos a zerar TEMPO_QUEIMA
 * explicitamente nos estados que não queimam, para o buffer não carregar
 * lixo residual (importante para o checksum final, que soma
 * estado*31+tempo de TODA célula).
 *
 * PARALELIZADO: cada iteração 'i' só lê matrix_atual (somente leitura,
 * seguro para acesso concorrente) e só escreve na sua própria posição de
 * matrix_proximo — não há dependência entre iterações, então dá pra usar
 * "omp for" direto, sem reduções. O schedule é escolhido em tempo de
 * execução ("schedule(runtime)"), então dá pra comparar static/dynamic/
 * guided via variável de ambiente OMP_SCHEDULE sem recompilar (útil pro
 * relatório).
 *
 * IMPORTANTE pro grupo: o "#pragma omp for" abaixo é uma construção ÓRFÃ —
 * ele só divide iterações entre threads se update_matrix for chamada de
 * dentro de uma região "#pragma omp parallel" já aberta (isso é
 * responsabilidade de quem for paralelizar run_simulation). Sem essa
 * região em volta, o código roda igual — como se fosse 1 thread só — e o
 * RESULTADO continua correto, só não ganha o paralelismo de fato ainda.
 */
void update_matrix(InputConfigs* configs, Celula* matrix_atual, Celula* matrix_proximo) {
  #pragma omp for schedule(runtime)
  for (long long i = 0; i < (long long)configs->L * configs->C; i++) {
    matrix_proximo[i].ID_ESTADO = matrix_atual[i].ID_ESTADO;
    if (
      matrix_atual[i].ID_ESTADO == 0 ||
      matrix_atual[i].ID_ESTADO == 3 ||
      matrix_atual[i].ID_ESTADO == 4
    ) {
      matrix_proximo[i].TEMPO_QUEIMA = 0;
      continue;
    }

    if (matrix_atual[i].ID_ESTADO == 2) {
      int novo_tempo = matrix_atual[i].TEMPO_QUEIMA - 1;

      if (novo_tempo <= 0) {
        matrix_proximo[i].ID_ESTADO = 3;
        matrix_proximo[i].TEMPO_QUEIMA = 0;
      } else {
        matrix_proximo[i].TEMPO_QUEIMA = novo_tempo;
      }
      continue;
    }

    int potencial_ignicao = calculate_potencial_ignicao(configs, (int)i, matrix_atual);

    if (potencial_ignicao < configs->LIMIAR) {
      matrix_proximo[i].TEMPO_QUEIMA = 0;
      continue;
    }

    matrix_proximo[i].ID_ESTADO = 2;
    matrix_proximo[i].TEMPO_QUEIMA = matrix_atual[i].ID_COBERTURA == 2 ? 2 : 4;
  }
}

/**
 * ATENÇÃO: função NÃO tocada por mim — não é a minha parte.
 */
bool check_stop_condition(InputConfigs* configs, Metrics item_vetor_tempo) {
  return item_vetor_tempo.EM_CHAMAS != 0 && item_vetor_tempo.PASSO < configs->P;
}

void print_metrics_csv_header() {
  printf("PASSO,COMBUSTIVEIS,NAO_COMBUSTIVEIS,INTACTAS,EM_CHAMAS,QUEIMADAS,CONTENCAO,TOTAL_IGNICOES,PERCENTUAL_QUEIMADO,PERCENTUAL_PROTEGIDO\n");
}

void print_metrics(Metrics item_vetor_tempo) {
  printf("%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f\n", 
      item_vetor_tempo.PASSO,
      item_vetor_tempo.COMBUSTIVEIS,
      item_vetor_tempo.NAO_COMBUSTIVEIS,
      item_vetor_tempo.INTACTAS,
      item_vetor_tempo.EM_CHAMAS,
      item_vetor_tempo.QUEIMADAS,
      item_vetor_tempo.CONTENCAO,
      item_vetor_tempo.TOTAL_IGNICOES,
      item_vetor_tempo.PERCENTUAL_QUEIMADO,
      item_vetor_tempo.PERCENTUAL_PROTEGIDO
    );
}

/**
 * ATENÇÃO: função NÃO tocada por mim — não é a minha parte. Continua
 * sequencial (sem reduction), igual estava.
 */
void calculate_metrics_resultados(
  int p,
  InputConfigs* configs,
  Metrics* vetor_tempo_atual,
  Celula* matrix_atual,
  Celula* matrix_proximo
) {
  vetor_tempo_atual[p].PASSO = p;
  int combustiveis_iniciais = 0;
  
  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    if (matrix_atual[i].ID_COBERTURA == 2 || matrix_atual[i].ID_COBERTURA == 3) combustiveis_iniciais++;
    
    switch (matrix_atual[i].ID_ESTADO) {
      case 0:
        vetor_tempo_atual[p].NAO_COMBUSTIVEIS += 1;
        break;
      case 1:
        vetor_tempo_atual[p].INTACTAS += 1;
        if (matrix_proximo[i].ID_ESTADO == 2) vetor_tempo_atual[p].TOTAL_IGNICOES++;
        break;
      case 2:
        vetor_tempo_atual[p].EM_CHAMAS += 1;
        break;
      case 3:
        vetor_tempo_atual[p].QUEIMADAS += 1;
        break;
      case 4:
        vetor_tempo_atual[p].CONTENCAO += 1;
        break;
      default:
        break;
    }
  }
  vetor_tempo_atual[p].COMBUSTIVEIS = combustiveis_iniciais;
  
  vetor_tempo_atual[p].PERCENTUAL_QUEIMADO = combustiveis_iniciais == 0 
    ? 0 
    : 100 * ((float)(vetor_tempo_atual[p].QUEIMADAS + vetor_tempo_atual[p].EM_CHAMAS) / combustiveis_iniciais);

  vetor_tempo_atual[p].PERCENTUAL_PROTEGIDO = combustiveis_iniciais == 0
    ? 0
    : 100 * ((float)(vetor_tempo_atual[p].CONTENCAO) / combustiveis_iniciais);
}

/**
 * ATENÇÃO: função NÃO tocada por mim — não é a minha parte. Continua
 * exatamente como estava (inclusive sem a região "#pragma omp parallel"
 * em volta do laço — isso é do responsável por essa orquestração).
 */
void run_simulation(
  InputConfigs* configs,
  Celula *matrix_estado_atual,
  Celula *matrix_proximo_estado,
  int *vetor_ativacao,
  Metrics *vetor_tempo_atual,
  Metrics *vetor_proximo_tempo
) {
  print_metrics_csv_header();

  int p = 0;
  do {
    activate_zonas_contencao(configs, matrix_estado_atual, vetor_ativacao, p);
    
    update_matrix(configs, matrix_estado_atual, matrix_proximo_estado);
    
    calculate_metrics_resultados(p, configs, vetor_tempo_atual, matrix_estado_atual, matrix_proximo_estado);
    
    print_metrics(vetor_tempo_atual[p]);

    Celula* matrix_tmp = matrix_estado_atual;
    matrix_estado_atual = matrix_proximo_estado;
    matrix_proximo_estado = matrix_tmp; 

    p++;
  } while (check_stop_condition(configs, vetor_tempo_atual[p-1]));
}

/**
 * ATENÇÃO: função NÃO tocada por mim — não é a minha parte.
 */
unsigned long long calculate_checksum(InputConfigs* configs, Celula* matrix_estado_atual, Metrics* vetor_tempo_atual) {
  unsigned long long checksum = 0;
  for (long long i = 0; i < configs->L * configs->C; i++) {
    checksum = checksum*31ULL + (unsigned long long)matrix_estado_atual[i].ID_ESTADO;
    checksum = checksum*31ULL + (unsigned long long)vetor_tempo_atual[i].PASSO;
  }
  return checksum;
}


void cleanup_simulation_setup(InputConfigs *configs, Celula *matrix_estado_atual, Metrics *vetor_tempo_atual, Metrics *vetor_proximo_tempo, int *vetor_ativacao) {
  free_simulation_matrix(configs, matrix_estado_atual);
  free_metrics_vector(vetor_tempo_atual);
  free_metrics_vector(vetor_proximo_tempo);
  free_mapa_contencao_vector(vetor_ativacao);
  free_input_configs(configs);
}

int main(int argc, char* argv[]) {
  if (!is_valid_single_input_argument(argc)) {
    perror("Entrada inválida. A execução do programa requer um argumento: ./program <input_configs_filename>\n");
    return EXIT_FAILURE;
  }

  InputConfigs* configs = load_input_configs(argv[1]);
  if (!configs) {
    perror("Arquivo de configurações inválido.\n");
    free_input_configs(configs);
    return EXIT_FAILURE;
  }

  if (!is_valid_input_configs(configs)) {
    perror("Arquivo de configurações inválido.\n");
    free_input_configs(configs);
    return EXIT_FAILURE;
  }

  Celula *matrix_estado_atual = build_linear_state_matrix(configs);
  Metrics *vetor_tempo_atual = build_metrics_vector(configs);
  Metrics *vetor_proximo_tempo = build_metrics_vector(configs);
  int *vetor_ativacao = build_mapa_contencao(configs, matrix_estado_atual);

  if (!matrix_estado_atual || !vetor_tempo_atual || !vetor_proximo_tempo || !vetor_ativacao) {
    perror("Falha ao alocar memória para matriz");

    cleanup_simulation_setup(configs, matrix_estado_atual, vetor_tempo_atual, vetor_proximo_tempo, vetor_ativacao);
    return EXIT_FAILURE;
  }

  if (!populate_matrix(configs, matrix_estado_atual)) {
    perror("Falha ao popular matriz.\n");

    cleanup_simulation_setup(configs, matrix_estado_atual, vetor_tempo_atual, vetor_proximo_tempo, vetor_ativacao);
    return EXIT_FAILURE;
  }

  Celula *matrix_proximo_estado = copy_matrix(configs, matrix_estado_atual);

  apply_focos_iniciais_incendio(configs, matrix_estado_atual);
  apply_focos_iniciais_incendio(configs, matrix_proximo_estado);

  run_simulation(configs, matrix_estado_atual, matrix_proximo_estado, vetor_ativacao, vetor_tempo_atual, vetor_proximo_tempo);

  cleanup_simulation_setup(configs, matrix_estado_atual, vetor_tempo_atual, vetor_proximo_tempo, vetor_ativacao);
  free_simulation_matrix(configs, matrix_proximo_estado);
  return EXIT_SUCCESS;
}