// Modificações com suporte para execução MPI/OMP

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <mpi.h>
#include <string.h>

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
 * TEMPO: tempo de execução instantânea do passo corrente
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
  double TEMPO;
} Metrics;

/**
 * Definições de entrada
 * 
 * ============================
 * L: número de linhas (matrix)
 * C: número de colunas (matrix)
 * P: número máximo de passos para a simulação
 * T: número de threads (caso paralelo)
 * SEED: semente pseudoaleatoria
 * LIMIAR: potencial mínimo de ignição
 * 
 * ============================
 * VENTO_LINHA: componente vertical do vento
 * VENTO_COLUNA: componente horizontal do vento
 * VENTO: intensidade do vento
 * 
 * ============================
 * F: focos inuiciais de incêndio
 * Z: quantidade de zonas de contenção
 * FOCOS_INCENDIO: focos de incêndio
 * ZONAS_CONTENCAO: zonas de contenção
 */
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

// Impressão de configurações (debug)
void print_loaded_input_configs(InputConfigs* configs);

bool is_valid_input_configs(InputConfigs* configs);
bool is_valid_single_input_argument(int argc);

bool is_valid_configs_first_line(InputConfigs* configs);
bool is_valid_configs_vento(InputConfigs* configs);
bool is_valid_F_Z(InputConfigs* configs);
bool is_valid_matrix_focos_incendio(InputConfigs* configs);
bool is_valid_matrix_zonas_contencao(InputConfigs* configs);
bool is_valid_foco_incendio_sobre_celula_combustivel(InputConfigs* configs, int id_cobertura, int row, int col);

// Geração de cobertura
Celula *build_linear_state_matrix(InputConfigs* configs);


/**
 * 5. Validação da entrada
 * 
 * 5.3: L > 0, C > 0, P ≥ 0, T > 0, LIMIAR > 0
 */
bool is_valid_configs_first_line(InputConfigs* configs) {
  // Validação 3: Argumentos de primeira linha

  // Validação 3.1: L > 0
  if (configs->L <= 0) {
    printf("L > 0. Informado: L = %d\n", configs->L);
    return false;
  }

  // Validação 3.2: C > 0
  if (configs->C <= 0) {
    printf("C > 0. Informado: C = %d\n", configs->C);
    return false;
  }

  // Validação 3.3: P >= 0
  if (configs->P < 0) {
    printf("P >= 0. Informado: P = %d\n", configs->P);
    return false;
  }

  // Validação 3.4: T >= 0
  if (configs->T < 0) {
    printf("T >= 0. Informado: T = %d\n", configs->T);
    return false;
  }

  // Validação 3.5: LIMIAR > 0
  if (configs->LIMIAR <= 0) {
    printf("LIMIAR > 0. Informado: LIMIAR = %d\n", configs->LIMIAR);
    return false;
  }

  return true;
}

/**
 * 4. Carregamento de valores de configurações
 * 
 * 4.1. Configuração geral da simulação
 * Linha 1: L, C, P, T, SEED, LIMIAR
 * 
 * 4.2. Configuração do vento para a floresta
 * Linha 2: VENTO_LINHA, VENTO_COLUNA, V
 * 
 * 4.3. Quantidade de focos de incêndio e zonas de contenção
 * Linha 3: F, Z
 * 
 * Linha 4+: Linhas de focos de incêndio 
 * Coordenadas de Focos: L, C
 * Coordenadas de Zonas: PASSO_ATIVACAO, LI, CI, LF, CF
 */
InputConfigs* load_input_configs(const char* filepath) {
  // 4. Formato do arquivo de entrada
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
  // 4.1. Configuração geral da simulação
  // Linha 1: L, C, P, T, SEED, LIMIAR
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

  // 4.2. Configuração do vento para a floresta
  // Linha 2: VENTO_LINHA, VENTO_COLUNA, V
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

  // 4.3. Quantidade de focos de incêndio e zonas de contenção
  // Linha 3: F, Z
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

  // Validação prévia à alocação de memória
  if (!is_valid_F_Z(configs)) {
    fclose(file);
    free(configs);
    return NULL;
  }

  // Linha 4+: Linhas de focos de incêndio 
  configs->FOCOS_INCENDIO = malloc(configs->F * sizeof(FocoIncendio*));
  for (int i = 0; i < configs->F; i++) {
    configs->FOCOS_INCENDIO[i] = malloc(sizeof(FocoIncendio));
  }
  
  for (int i = 0; i < configs->F; i++) {
    // Coordenadas de Focos: L, C
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
    // Coordenadas de Zonas: PASSO_ATIVACAO, LI, CI, LF, CF
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
  // Validação 5: Focos e Zonas

  // Validação 5.1: F >= 0
  if (configs->F < 0) {
    printf("F >= 0. Informado: F = %d\n", configs->F);
    return false;
  }

  // Validação 5.2: Z >= 0
  if (configs->Z < 0) {
    printf("Z >= 0. Informado: Z = %d\n", configs->Z);
    return false;
  }

  return true;
}

/**
 * 5. Validação da entrada
 * 
 * 5.5: Focos de incêndio
 */
bool is_valid_matrix_focos_incendio(InputConfigs* configs) {
  if (configs->F == 0) return true;

  int L = configs->FOCOS_INCENDIO[0]->L;
  int C = configs->FOCOS_INCENDIO[0]->C;

  for (int i = 0; i < configs->F; i++) {
    // Validação 5.1: Focos dentro da matriz
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

    // Validação 5.2: Ausência de focos repetidos
    if (i != 0 && (configs->FOCOS_INCENDIO[i]->L == L && configs->FOCOS_INCENDIO[i]->C == C)) {
      printf(
        "Focos não devem estar duplicados. Informado: (%d, %d)\n", 
        configs->FOCOS_INCENDIO[i]->L, configs->FOCOS_INCENDIO[i]->C
      );
      return false;
    }

    // Validação 5.3: Focos sobre células de combustivels
    // Realizada após gerar matriz

    L = configs->FOCOS_INCENDIO[i]->L;
    C = configs->FOCOS_INCENDIO[i]->C;
  }

  return true;
}

/**
 * 5. Validação da entrada
 * 
 * 5.6: Zonas de contenção
 */
bool is_valid_matrix_zonas_contencao(InputConfigs* configs) {

  if (configs->Z == 0) return true;

  for (int i = 0; i < configs->Z; i++) {
    // Validação 6.1: Zonas dentro da matriz
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

    // Validação 6.2: Limites iniciais < Limites finais
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

    // Validação 6.3: Passo de ativação
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

/**
 * 5. Validação da entrada
 * 
 * 5.4: componentes do vento entre -1 e 1, direção diferente de (0,0), intensidade entre 0 e 5
 */
bool is_valid_configs_vento(InputConfigs* configs) {
  // Validação 4: Vento

  // Validação 4.1: -1 <= VENTO_LINHA <= 1 e -1 <= VENTO_COLUNA <= 1
  if (
      (configs->VENTO_LINHA < -1 || configs->VENTO_LINHA > 1) || 
      (configs->VENTO_COLUNA < -1 || configs->VENTO_COLUNA > 1)
    ) { 
      printf("-1 <= VENTO_LINHA <= 1 e -1 <= VENTO_COLUNA <= 1. Informado: (L,C) = (%d,%d)\n", configs->VENTO_LINHA, configs->VENTO_COLUNA);
      return false;
  }
  
  // Validação 4.2: (VENTO_LINHA, VENTO_COLUNA) != (0,0)
  if (configs->VENTO_LINHA == 0 && configs->VENTO_COLUNA == 0){ 
    printf("(VENTO_LINHA, VENTO_COLUNA) != (0,0). Informado: (L,C) = (%d,%d)\n", configs->VENTO_LINHA, configs->VENTO_COLUNA);
    return false;
  }

  // Validação 4.3: 0 <= V <= 5
  if (configs->V < 0 || configs->V > 5) {
    printf("0 <= V <= 5. Informado: V = %d\n", configs->V);
    return false;
  }

  return true;
}

/**
 * 5. Validação da entrada
 * 
 * 5.3: Configurações gerais
 *  - L > 0, C > 0, P ≥ 0, T > 0, LIMIAR > 0
 * 
 * 5.4: Componentes do vento
 *  - componentes do vento entre -1 e 1, 
 *  - direção diferente de (0,0), 
 *  - intensidade entre 0 e 5
 * 
 * 5.5: Focos de incendio
 *  - F ≥ 0, Z ≥ 0
 *  - focos dentro da matriz
 *  - ausência de focos repetidos
 *  - focos posicionados sobre células combustíveis
 * 
 * 5.6: Zonas de contenção
 *  - zonas completamente internas à matriz
 *  - limites iniciais não superiores aos finais
 *  - 0 ≤ passo_ativacao < P
 */
bool is_valid_input_configs(InputConfigs* configs) {
  return is_valid_configs_first_line(configs) &&
    is_valid_configs_vento(configs) &&
    is_valid_matrix_focos_incendio(configs) && 
    is_valid_matrix_zonas_contencao(configs);
}

/**
 * 5. Validação da entrada
 * 
 * 5.1: Presença de uym único argumento
 */
bool is_valid_single_input_argument(int argc) {
  return argc == 2 ? true : false;
}

/**
 * 6. Descrição do uso das entradas na prepaçara~o da simulação
 * 
 * 6.2: Geração da cobertura
 *  - valor = rand_r(&seed) % 100
 *       0 a  9 0 Água (10%)
 *      10 a 19 1 solo exposto (10%)
 *      20 a 54 2 vegetação rasteira (35%)
 *      55 a 99 3 Floresta (45%)
 */
int generate_cobertura(InputConfigs* configs) {
  int valor = rand_r(&configs->SEED) % 100;
  if (valor <= 9) return 0;
  if (valor <= 19) return 1;
  if (valor <= 54) return 2;
  return 3;
}

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.2: Geração da cobertura
 *  - fatores de combustível aplicados para cada tipo de cobertura
 *      Água                0
 *      Solo exposto        0
 *      Vegetação rasteira  8
 *      Floresta            12
 */
int generate_fator_incendio(int id_cobertura) {
  if (id_cobertura == 0 || id_cobertura == 1) return 0;
  if (id_cobertura == 2) return 8;
  return 12;
}

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.3: Geração da umidade
 *  - umidade = rand_r(&seed) % 101
 */
int generate_umidade(InputConfigs* configs) {
  return rand_r(&configs->SEED) % 101;
}

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.4: Estados das células
 *  - As coberturas Água e Solo Exposto são do tipo “não combustível”. Todas as células 
 *    com coberturas Vegetação Rasteira e Floresta são iniciadas como “intactas”, 
 *    até a aplicação dos focos iniciais de incêndio. 
 *      0 não combustível
 *      1 intacta
 *      2 em chamas
 *      3 queimada
 *      4 contenção
 */
int generate_estado(int id_cobertura) {
  return (id_cobertura == 0 || id_cobertura == 1) ? 0 : 1;
}

// TODO: manter row*C, increment col nos laços for
unsigned long long calculate_linear_matrix_index(int row, int col, int C) {
  return (unsigned long long) row * C + col;
}

/**
 * 5. Validação da entrada
 * 
 * 5.6: Focos de incêndio
 */
bool is_valid_foco_incendio_sobre_celula_combustivel(InputConfigs* configs, int id_cobertura, int row, int col) {
  // Validação 6.3: Focos sobre células de combustivels

  if (id_cobertura == 2 || id_cobertura == 3) return true;

  for (int k = 0; k < configs->F; k++) {
    if (configs->FOCOS_INCENDIO[k]->L == row && configs->FOCOS_INCENDIO[k]->C == col) {
      printf("Focos posicionados sobre células combustíveis. Informado: (L,C) = (%d,%d)\n", row, col);
      return false;
    }
  }

  return true;
}

/**
 * 6. Descrição do uso das entradas na prepaçara~o da simulação
 * 
 * 6.2: Geração da cobertura
 *  - valor = rand_r(&seed) % 100
 *       0 a  9 0 Água (10%)
 *      10 a 19 1 solo exposto (10%)
 *      20 a 54 2 vegetação rasteira (35%)
 *      55 a 99 3 Floresta (45%)
 *  - fatores de combustível aplicados para cada tipo de cobertura
 *      Água                0
 *      Solo exposto        0
 *      Vegetação rasteira  8
 *      Floresta            12
 * 
 * 6.3: Geração da umidade
 *  - umidade = rand_r(&seed) % 101
 * 
 * 6.4: Estados das células
 *  - As coberturas Água e Solo Exposto são do tipo “não combustível”. Todas as células 
 *    com coberturas Vegetação Rasteira e Floresta são iniciadas como “intactas”, 
 *    até a aplicação dos focos iniciais de incêndio. 
 *      0 não combustível
 *      1 intacta
 *      2 em chamas
 *      3 queimada
 *      4 contenção
 * 
 */
bool populate_matrix(InputConfigs* configs, Celula *matrix) {
  if (!configs || !matrix) return false;

  for (int i = 0; i < configs->L; i++) {
    for (int j = 0; j < configs->C; j++) {
      unsigned long long idx = calculate_linear_matrix_index(i, j, configs->C); // row * C + col;
      
      int id_cobertura = generate_cobertura(configs);
      matrix[idx].ID_COBERTURA = id_cobertura;
      matrix[idx].FATOR_INCENDIO = generate_fator_incendio(id_cobertura);
      matrix[idx].UMIDADE = generate_umidade(configs);
      matrix[idx].ID_ESTADO = generate_estado(id_cobertura);
      matrix[idx].TEMPO_QUEIMA = 0;
      
      if (!is_valid_foco_incendio_sobre_celula_combustivel(configs, id_cobertura, i, j)) return false;
    }
  }
  return true;
}

/**
 * 8. Cálculo do potencial de ignição
 * 
 * 8.1: Sentido de propagação do vento
 * 
 */
int calculate_potencial_ignicao(const InputConfigs* restrict configs, int row, int col, const Celula* restrict matrix_atual) {
  // Vizinhos de Moore para matriz linear, sendo 
  //  - x a posição da célula corrente, dado por (x_i, x_j)
  //  - L: número de linhas na matriz
  //  - C: número de colunas na matriz
  //      a b c
  //      d X e
  //      f g h
  // 
  //  b = (x_i - C,     x_j - L - 1)
  //  g = (x_i - C,     x_j - L + 1)
  //  d = (x_i - C - 1, x_j - L)
  //  e = (x_i - C + 1, x_j - L)
  
  //  a = (x_i - C - 1, x_j - L - 1)
  //  f = (x_i - C - 1, x_j - L + 1)
  //  c = (x_i - C + 1, x_j - L - 1)
  //  h = (x_i - C + 1, x_j - L + 1)

  // op linha: x_i - C + { 0,  0, -1, +1, -1, -1, +1, +1}
  // op colun: x_j - L + {-1, +1,  0,  0, -1, +1, -1, +1}

  static const int vet_linha[8] = { 0,  0, -1, 1, -1, -1, 1, 1};
  static const int vet_coluna[8] = {-1, 1,  0,  0, -1, 1, -1, 1};

  // Casos de borda: vizinhos fora da matriz serão ignorados

  // Já calcular peso basico. prim4: ortogonais. last4: diagonais
  // Casos peso_basico:
  //  - Vizinho ortogonal: 10
  //  - Vizinho diagonal: 7
  static const int peso_basico[8] = {10, 10, 10, 10, 7, 7, 7, 7};

  
  int C = configs->C;
  const int V_LINHA = configs->VENTO_LINHA;
  const int V_COLUNA = configs->VENTO_COLUNA;
  const int V_INTENS = configs->V;

  // S = sum(𝑃𝑣)
  int S = 0;
  
  for (int i = 0; i < 8; i++) { 
    const int linha_vizinho = row + vet_linha[i];
    const int coluna_vizinho = col + vet_coluna[i];
    
    const int prop_linha = row - linha_vizinho;
    const int prop_coluna = col - coluna_vizinho;
    
    // Caso fora dos limites
    // evitar chamada de função
    if (
      (linha_vizinho >= 0 && linha_vizinho < configs->L) &&
      (coluna_vizinho >= 0 && coluna_vizinho < configs->C)
    ) {
      const int idx_vizinho = linha_vizinho * C + coluna_vizinho;
      
      // Considerar apernas se vizinho em estado 'em chamas'
      if (matrix_atual[idx_vizinho].ID_ESTADO == 2) {
  
        // 𝐴 = 𝑝𝑟𝑜𝑝_𝑙𝑖𝑛ℎ𝑎 × 𝑣𝑒𝑛𝑡𝑜_𝑙𝑖𝑛ℎ𝑎 + 𝑝𝑟𝑜𝑝_𝑐𝑜𝑙𝑢𝑛𝑎 × 𝑣𝑒𝑛𝑡𝑜_𝑐𝑜𝑙𝑢𝑛a
        const int A = prop_linha*V_LINHA + prop_coluna*V_COLUNA;
  
        // 𝑃𝑣 = max(1, 𝑃básico + 𝑖𝑛𝑡𝑒𝑛𝑠𝑖𝑑𝑎𝑑𝑒 × 𝐴)
        const int peso_calculado_int_a = peso_basico[i] + (V_INTENS * A);
  
        S += peso_calculado_int_a > 1 ? peso_calculado_int_a : 1;
      }
    }
  }

  const unsigned long long idx_atual = row * C + col;

  // 𝐼 = floor(𝑆 × 𝑓𝑎𝑡𝑜𝑟_𝑐𝑜𝑚𝑏𝑢𝑠𝑡𝚤𝑣𝑒𝑙 × (100 − 𝑢𝑚𝑖𝑑𝑎𝑑𝑒) / 100)
  return (S * matrix_atual[idx_atual].FATOR_INCENDIO * (100 - matrix_atual[idx_atual].UMIDADE)) / 100 ;
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

/**
 * 6. Descrição de uso das entradas na preparação da simulação
 * 
 * 6.1 Construão da matriz
 *  - armazenamento linear
 *  - cada célula apresenta cobertura, umidade, estado e tempo de queima
 */
Celula *build_linear_state_matrix(InputConfigs* configs) {
  Celula *matrix = malloc((size_t) configs->L * configs->C * sizeof(Celula));
  if (!matrix) return NULL;

  return matrix;
}


Metrics *build_metrics_vector(InputConfigs* configs) {
  Metrics *vector = malloc(configs->L * configs->C * sizeof(Metrics));
  if (!vector) return NULL;

  // Inicializar com 0
  for (unsigned long long i = 0; i < (configs->L * configs->C); i++) {
    vector[i].CONTENCAO = 0;
    vector[i].EM_CHAMAS = 0;
    vector[i].INTACTAS = 0;
    vector[i].NAO_COMBUSTIVEIS = 0;
    vector[i].COMBUSTIVEIS = 0;
    vector[i].PASSO = 0;
    vector[i].QUEIMADAS = 0;
    vector[i].TOTAL_IGNICOES = 0;
    vector[i].TEMPO = 0;
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

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.5: Aplicação dos focos iniciais de incêndio
 *  - O tempo inicial de queima será de 02 passos para Vegetação Rasteira e de 04 
 *    passos para Floresta
 */
void apply_focos_iniciais_incendio(InputConfigs* configs, Celula* matrix) {
  for (int i = 0; i < configs->F; i++) {
    unsigned long long idx = calculate_linear_matrix_index(configs->FOCOS_INCENDIO[i]->L, configs->FOCOS_INCENDIO[i]->C, configs->C);
    matrix[idx].ID_ESTADO = 2;

    // Caso vegetação rasteira
    if (matrix[idx].ID_COBERTURA == 2) {
      matrix[idx].TEMPO_QUEIMA = 2;
      continue;
    }

    // Caso floresta
    matrix[idx].TEMPO_QUEIMA = 4;
  }
}

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.6: Construção do mapa de contenção
 *  - Retorno para -1 ou min(passo_ativação) caso zonas sobrepostas
 */
int get_passo_ativacao_if_cell_in_zona_contencao(int row, int col, InputConfigs* configs) {
  // Obter mínimo caso célula pertencer a zonas de contenção sobrepostas
  // max(passo) = P-1
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

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.6: Construção do mapa de contenção
 *  - ativacao[indice], contendo o valor -1 ou o número do passo de ativação da zona
 */
int *build_mapa_contencao(InputConfigs* configs, Celula* matrix) {
    int *ativacao = (int*) malloc((size_t) configs->L * configs->C * sizeof(int));
    if (!ativacao) return NULL;

    for (int rowi = 0; rowi < configs->L; rowi++) {
        for (int coli = 0; coli < configs->C; coli++) {
            unsigned long long i = rowi * configs->C + coli;
            ativacao[i] = get_passo_ativacao_if_cell_in_zona_contencao(rowi, coli, configs);
        }
    }

    return ativacao;
}

void print_metrics(Metrics item_vetor_tempo) {
  printf("%d,%d,%d,%d,%d,%d,%d,%d\n", 
    item_vetor_tempo.PASSO,
    item_vetor_tempo.COMBUSTIVEIS,
    item_vetor_tempo.NAO_COMBUSTIVEIS,
    item_vetor_tempo.INTACTAS,
    item_vetor_tempo.EM_CHAMAS,
    item_vetor_tempo.QUEIMADAS,
    item_vetor_tempo.CONTENCAO,
    item_vetor_tempo.TOTAL_IGNICOES
  );
}

float calculate_percentual_queimado(int passo, Metrics* vetor_tempo) {
  // 𝑝𝑒𝑟𝑐𝑒𝑛𝑡𝑢𝑎𝑙_𝑞𝑢𝑒𝑖𝑚𝑎𝑑𝑜 = 100 × (𝑞𝑢𝑒𝑖𝑚𝑎𝑑𝑎𝑠 + 𝑒𝑚_𝑐ℎ𝑎𝑚𝑎𝑠 / 𝑐𝑜𝑚𝑏𝑢𝑠𝑡𝚤𝑣𝑒𝑖𝑠_𝑖𝑛𝑖𝑐𝑖𝑎𝑖s)
  return vetor_tempo[passo].COMBUSTIVEIS == 0 
    ? 0 
    : 100 * ((float)(vetor_tempo[passo].QUEIMADAS + vetor_tempo[passo].EM_CHAMAS) / vetor_tempo[passo].COMBUSTIVEIS);
}

float calculate_percentual_protegido(int passo, Metrics* vetor_tempo) {
  // 𝑝𝑒𝑟𝑐𝑒𝑛𝑡𝑢𝑎𝑙_𝑝𝑟𝑜𝑡𝑒𝑔𝑖𝑑𝑜 = 100 × (𝑐𝑜𝑛𝑡𝑒𝑛𝑐𝑎𝑜 / 𝑐𝑜𝑚𝑏𝑢𝑠𝑡𝑖𝑣𝑒𝑖𝑠_𝑖𝑛𝑖𝑐𝑖𝑎𝑖𝑠)
  return vetor_tempo[passo].COMBUSTIVEIS == 0
    ? 0
    : 100 * ((float)(vetor_tempo[passo].CONTENCAO) / vetor_tempo[passo].COMBUSTIVEIS);
}

int calculate_max_ignicoes(int passo, Metrics* vetor_tempo, int* passo_max_ign) {
  int max_ignicoes = vetor_tempo[0].TOTAL_IGNICOES;
  int passo_max = 0;
  *passo_max_ign = 0;
  for (int i = 0; i < passo; i++) {
    if (vetor_tempo[i].TOTAL_IGNICOES > max_ignicoes) {
      max_ignicoes = vetor_tempo[i].TOTAL_IGNICOES;
      passo_max = i;
    }
  }

  *passo_max_ign = passo_max;

  return max_ignicoes;
}

static inline void exchange_halo_rows(
  Celula *matrix_atual,
  int local_rows,
  int C,
  int pos_vizinhanca_top,
  int pos_vizinhanca_bottom
) {
  const int row_bytes = C * (int)sizeof(Celula);

  MPI_Sendrecv(
    &matrix_atual[1 * C], row_bytes, MPI_BYTE, pos_vizinhanca_top, 0,
    &matrix_atual[(local_rows + 1) * C], row_bytes, MPI_BYTE, pos_vizinhanca_bottom, 0,
    MPI_COMM_WORLD, MPI_STATUS_IGNORE
  );

  MPI_Sendrecv(
    &matrix_atual[local_rows * C], row_bytes, MPI_BYTE, pos_vizinhanca_bottom, 1,
    &matrix_atual[0], row_bytes, MPI_BYTE, pos_vizinhanca_top, 1,
    MPI_COMM_WORLD, MPI_STATUS_IGNORE
  );
}

static inline void reduce_global_metrics(const int local_metrics[7], int global_metrics[7]) {
  MPI_Allreduce(local_metrics, global_metrics, 7, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
}

bool broadcast_input_validity(InputConfigs** configs_ptr, int rank) {
  bool is_valid = true;

  if (rank == 0) {
    if (!(*configs_ptr)) is_valid = false;
  } else {
    *configs_ptr = (InputConfigs*) malloc(sizeof(InputConfigs));
    if (!(*configs_ptr)) is_valid = false;
  }

  MPI_Bcast(&is_valid, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
  if (!is_valid) {
    if (rank != 0) {
      free(*configs_ptr);
      *configs_ptr = NULL;
    }
    return is_valid;
  }

  return is_valid;
}

bool broadcast_vetor_focos_incendio(InputConfigs* configs, int rank) {
  // Broadcast vetores
  // - focos de incendio
  if (configs->F > 0) {
    int* focos_int_linear = (int*) malloc((size_t)configs->F * 2 * sizeof(int));
    if (!focos_int_linear) {
      if (rank != 0) free_input_configs(configs);
      return false;
    }

    if (rank == 0) {
      for (int i = 0; i < configs->F; i++) {
        focos_int_linear[i * 2 + 0] = configs->FOCOS_INCENDIO[i]->L;
        focos_int_linear[i * 2 + 1] = configs->FOCOS_INCENDIO[i]->C;
      }
    }

    MPI_Bcast(focos_int_linear, configs->F * 2, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
      configs->FOCOS_INCENDIO = (FocoIncendio**) malloc((size_t)configs->F * sizeof(FocoIncendio*));
      for (int i = 0; i < configs->F; i++) {
        configs->FOCOS_INCENDIO[i] = (FocoIncendio*) malloc(sizeof(FocoIncendio));
        configs->FOCOS_INCENDIO[i]->L = focos_int_linear[i * 2 + 0];
        configs->FOCOS_INCENDIO[i]->C = focos_int_linear[i * 2 + 1];
      }
    }

    free(focos_int_linear);
  }

  return true;
}

bool broadcast_vetor_zonas_contencao(InputConfigs* configs, int rank) {
  if (configs->Z > 0) {
    int* zonas_int_linear = (int*) malloc((size_t)configs->Z * 5 * sizeof(int));
    if (!zonas_int_linear) {
      if (rank != 0) free_input_configs(configs);
      return false;
    }

    if (rank == 0) {
      for (int i = 0; i < configs->Z; i++) {
        zonas_int_linear[i * 5 + 0] = configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO;
        zonas_int_linear[i * 5 + 1] = configs->ZONAS_CONTENCAO[i]->LI;
        zonas_int_linear[i * 5 + 2] = configs->ZONAS_CONTENCAO[i]->CI;
        zonas_int_linear[i * 5 + 3] = configs->ZONAS_CONTENCAO[i]->LF;
        zonas_int_linear[i * 5 + 4] = configs->ZONAS_CONTENCAO[i]->CF;
      }
    }

    MPI_Bcast(zonas_int_linear, configs->Z * 5, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank != 0) {
      configs->ZONAS_CONTENCAO = (ZonaContencao**) malloc((size_t)configs->Z * sizeof(ZonaContencao*));
      for (int i = 0; i < configs->Z; i++) {
        configs->ZONAS_CONTENCAO[i] = (ZonaContencao*) malloc(sizeof(ZonaContencao));
        configs->ZONAS_CONTENCAO[i]->PASSO_ATIVACAO = zonas_int_linear[i * 5 + 0];
        configs->ZONAS_CONTENCAO[i]->LI = zonas_int_linear[i * 5 + 1];
        configs->ZONAS_CONTENCAO[i]->CI = zonas_int_linear[i * 5 + 2];
        configs->ZONAS_CONTENCAO[i]->LF = zonas_int_linear[i * 5 + 3];
        configs->ZONAS_CONTENCAO[i]->CF = zonas_int_linear[i * 5 + 4];
      }
    }

    free(zonas_int_linear);
  }

  return true;
}

/**
 * Broadcast de configurações de entrada
 * - Pós validação pela master (rank 0)
 */
bool broadcast_input_configs(InputConfigs** configs_ptr, int rank) {
  bool bcast_validity_success = broadcast_input_validity(configs_ptr, rank);
  if (!bcast_validity_success) return bcast_validity_success;

  InputConfigs* configs = *configs_ptr;

  // Brodcast valores na raiz de configs (ints)
  MPI_Bcast(configs, sizeof(InputConfigs), MPI_BYTE, 0, MPI_COMM_WORLD);

  if (rank != 0) {
    configs->FOCOS_INCENDIO = NULL;
    configs->ZONAS_CONTENCAO = NULL;
  }

  // Broadcast valores em vetores
  // - focos de incendio | zonas de contencao

  // Broadcst focos de incendio
  bool bcast_focos_success = broadcast_vetor_focos_incendio(configs, rank);

  // Broadcast zonas contencao
  bool bcast_zonas_success = broadcast_vetor_zonas_contencao(configs, rank);

  return bcast_focos_success && bcast_zonas_success;
}

/**
 * 7. Funcionamento da simulação
 * 
 * 7.1: Ordem de execução de cada passo
 *  - Para cada passo p, a ordem será:
 *      1. ativar as zonas programadas para p;
 *      2. calcular o próximo estado de todas as células;
 *      3. calcular as estatísticas do próximo estado;
 *      4. trocar as matrizes;
 *      5. verificar a condição de parada.
 * 
 * 7.2: Ativação das zonas
 *  - Se ativacao[indice] == p
 *      Estado atual    Estado após a ativação
 *      Intacta         Contenção
 *      Em chamas       Em chamas
 *      Queimada        Queimada
 *      Não combustível Não combustível
 *      Contenção       Contenção
 * 
 */
int run_simulation(
  InputConfigs* configs,
  Celula *matrix_estado_atual,
  Celula *matrix_proximo_estado,
  int *vetor_ativacao,
  Metrics *vetor_tempo_atual,
  Metrics *vetor_proximo_tempo,
  int local_rows,
  int rank,
  int size
) {
  Celula* const matrices[2] = {matrix_estado_atual, matrix_proximo_estado};

  int p = 0;
  bool manter_simulacao = true;
  const int P_TOTAL = configs->P;

  const int C = configs->C;
  const int LIMIAR = configs->LIMIAR;

  const unsigned long long START_IDX = (unsigned long long) C;
  const unsigned long long END_IDX   = (unsigned long long) (local_rows + 1) * C;

  const int pos_vizinhanca_top    = (rank > 0) ? (rank - 1) : MPI_PROC_NULL;
  const int pos_vizinhanca_bottom = (rank < size - 1) ? (rank + 1) : MPI_PROC_NULL;

  int combustiveis = 0;
  int nao_combustiveis = 0;
  int intactas = 0;
  int total_ignicoes = 0;
  int em_chamas = 0;
  int queimadas = 0;
  int contencao = 0;

  // pulo de 7 posicoes de int
  // -- uma por metrica
  int local_metrics[7]  = {0};
  int global_metrics[7] = {0};

  #pragma omp parallel default(none) \
      num_threads(configs->T) \
      shared(matrices, configs, vetor_ativacao, vetor_tempo_atual, \
        p, manter_simulacao, P_TOTAL, START_IDX, END_IDX, \
        local_rows, C, LIMIAR, \
        pos_vizinhanca_top, pos_vizinhanca_bottom, rank, \
        combustiveis, nao_combustiveis, intactas, total_ignicoes, \
        em_chamas, queimadas, contencao, local_metrics, global_metrics \
      )
  {
    do {
      Celula* const matrix_atual = matrices[p & 1];
      Celula* const matrix_proximo = matrices[(p + 1) & 1];

      // Tempo inicial. Set 0 para m[étricas]
      #pragma omp single
      {
        if (rank == 0) {
          vetor_tempo_atual[p].TEMPO = MPI_Wtime();
          vetor_tempo_atual[p].PASSO = p;
        }

        combustiveis = 0;
        nao_combustiveis = 0;
        intactas = 0;
        total_ignicoes = 0;
        em_chamas = 0;
        queimadas = 0;
        contencao = 0;
      }

      // Ativação de zonas de contenção
      // Remoção de função activate_zonas_contencao (presente no T1)
      #pragma omp for simd schedule(static)
      for (unsigned long long i = START_IDX; i < END_IDX; i++) {
        if (vetor_ativacao[i] == p && matrix_atual[i].ID_ESTADO == 1) {
          matrix_atual[i].ID_ESTADO = 4;
        }
      }

      // Compartilhamento de células de borda (halo)
      #pragma omp master
      {
        exchange_halo_rows(matrix_atual, local_rows, C, pos_vizinhanca_top, pos_vizinhanca_bottom);
      }
      #pragma omp barrier

      // Remoção de função update_matrix (presente no T1)
      #pragma omp for collapse(2) schedule(static)
      for (int i = 1; i <= local_rows; i++) {
        for (int j = 0; j < C; j++) {
          const int idx = i * C + j;
          const int estado_atual = matrix_atual[idx].ID_ESTADO;
          matrix_proximo[idx].ID_ESTADO = estado_atual;

          if (estado_atual == 2) {
            const int tempo = matrix_proximo[idx].TEMPO_QUEIMA;
            if (tempo == 0) {
              matrix_proximo[idx].ID_ESTADO = 3;
            } else {
              matrix_proximo[idx].TEMPO_QUEIMA = tempo - 1;
            }
          } else 
          if (estado_atual == 1) {
            const int potencial_ignicao = calculate_potencial_ignicao(configs, i, j, matrix_atual);

            // Se potencial_ignicao < LIMIAR, manter intacta (= 1)
            if (potencial_ignicao >= LIMIAR) {
              // Caso contrário, transitar para em chamas (= 2)
              matrix_proximo[idx].ID_ESTADO = 2;
              
              // Atualizar tempo de queima
              // Se vegetação rasteira (= 2), tempo de quima = 2
              // Se floresta (= 3), tempo de queima = 4
              matrix_proximo[idx].TEMPO_QUEIMA = matrix_atual[idx].ID_COBERTURA == 2 ? 2 : 4;
            };
          }
        }
      }

      // 5. Local OpenMP metrics reduction over owned interior cells [START_IDX, END_IDX)
      #pragma omp for simd schedule(static) reduction(+: \
          combustiveis, \
          nao_combustiveis, \
          intactas, \
          total_ignicoes, \
          em_chamas, \
          queimadas, \
          contencao)
      for (unsigned long long i = START_IDX; i < END_IDX; i++) {
        const int cob = matrix_atual[i].ID_COBERTURA;
        if (cob == 2 || cob == 3) {
          combustiveis++;
        }

        const int estado = matrix_atual[i].ID_ESTADO;
        switch (estado) {
          case 0:
            nao_combustiveis++;
            break;
          case 1:
            intactas++;
            if (matrix_proximo[i].ID_ESTADO == 2) {
              total_ignicoes++;
            }
            break;
          case 2:
            em_chamas++;
            break;
          case 3:
            queimadas++;
            break;
          case 4:
            contencao++;
            break;
          default:
            break;
        }
      }

      // Calculo interno de metricas
      // Remoção de função calculate_metrics_resultados (presente no T1)
      #pragma omp master
      {
        local_metrics[0] = combustiveis;
        local_metrics[1] = nao_combustiveis;
        local_metrics[2] = intactas;
        local_metrics[3] = total_ignicoes;
        local_metrics[4] = em_chamas;
        local_metrics[5] = queimadas;
        local_metrics[6] = contencao;

        reduce_global_metrics(local_metrics, global_metrics);

        if (rank == 0) {
          vetor_tempo_atual[p].COMBUSTIVEIS = global_metrics[0];
          vetor_tempo_atual[p].NAO_COMBUSTIVEIS = global_metrics[1];
          vetor_tempo_atual[p].INTACTAS = global_metrics[2];
          vetor_tempo_atual[p].TOTAL_IGNICOES = global_metrics[3];
          vetor_tempo_atual[p].EM_CHAMAS = global_metrics[4];
          vetor_tempo_atual[p].QUEIMADAS = global_metrics[5];
          vetor_tempo_atual[p].CONTENCAO = global_metrics[6];

          vetor_tempo_atual[p].TEMPO = MPI_Wtime() - vetor_tempo_atual[p].TEMPO;
        }

        manter_simulacao = (global_metrics[4] != 0) && (p < P_TOTAL);
        p++;
      }
      #pragma omp barrier

    } while (manter_simulacao);
  }

  return p - 1;
}

/**
 * 10. Calculo dos resultados
 * 
 * 10.4. Checksum
 */
unsigned long long calculate_checksum(InputConfigs* configs, Celula* matrix_estado_atual, Metrics* vetor_tempo_atual) {
  unsigned long long checksum = 0;
  for (long long i = 0; i < configs->L * configs->C; i++) {
    checksum = checksum*31ULL + (unsigned long long)matrix_estado_atual[i].ID_ESTADO;
    checksum = checksum*31ULL + (unsigned long long)vetor_tempo_atual[i].PASSO;
  }
  return checksum;
}


void cleanup_simulation_setup(InputConfigs *configs, Celula *matrix_estado_atual, Celula *matrix_proximo_estado, Metrics *vetor_tempo_atual, Metrics *vetor_proximo_tempo, int *vetor_ativacao) {
  free_simulation_matrix(configs, matrix_estado_atual);
  free_simulation_matrix(configs, matrix_proximo_estado);
  free_metrics_vector(vetor_tempo_atual);
  free_metrics_vector(vetor_proximo_tempo);
  free_mapa_contencao_vector(vetor_ativacao);
  free_input_configs(configs);
  omp_pause_resource_all(omp_pause_hard);
}

void print_final_report(int passo, unsigned long long checksum, Metrics *vetor_tempo) {
  int passo_max_ignicao = 0;
  int max_ignicoes = calculate_max_ignicoes(passo, vetor_tempo, &passo_max_ignicao);
  printf("passos: %d\n", passo);
  printf("nao_combustiveis: %d\n", vetor_tempo[passo].NAO_COMBUSTIVEIS);
  printf("intactas: %d\n", vetor_tempo[passo].INTACTAS);
  printf("em_chamas: %d\n", vetor_tempo[passo].EM_CHAMAS);
  printf("queimadas: %d\n", vetor_tempo[passo].QUEIMADAS);
  printf("contencao: %d\n", vetor_tempo[passo].CONTENCAO);
  printf("total_ignicoes: %d\n", vetor_tempo[passo].TOTAL_IGNICOES);
  printf("pico_ignicoes: %d %d\n", passo_max_ignicao, max_ignicoes);
  printf("percentual_queimado: %.2f\n", calculate_percentual_queimado(passo, vetor_tempo));
  printf("percentual_protegido: %.2f\n", calculate_percentual_protegido(passo, vetor_tempo));
  printf("checksum: %llu\n", checksum);
  printf("tempo: %.6f\n", vetor_tempo[passo].TEMPO - vetor_tempo[0].TEMPO);
}

/**
 * Cálculo de faixas de execução
 * - E.g.
 *    C = 8 | L = 9
 *    size = 4
 * 
 * tamanho_faixa_de_celulas_base = 9 / 4 = 2
 * remaider = 1
 * local_rows = 2 + 1
 * 
 * -------------- C/L _____             start | offset
 * o x x x x x x x  |      rank = 0     0       23
 * x x x x x x x x  | 
 * x x x x x x x o  | _____                         <- mantém remainder junto com rank 0, tentando manter localidade espacial
 * o x x x x x x x  |      rank = 1     24      39
 * x x x x x x x o  | _____
 * o x x x x x x x  |      rank = 2     40      55
 * x x x x x x x o  | _____
 * o x x x x x x x  |      rank = 3     60      71
 * x x x x x x x x  | _____
 * 
 */
void calculate_faixas_de_celulas(
  InputConfigs* configs,
  int size, int rank, int tamanho_faixa_de_celulas_base, int remainder,
  int *sendv_posicao_celulas, int *offset_posicao_celulas, 
  int *sendv_posicao_vet_ativacao, int *offset_posicao_vet_ativacao
) {
  const int C = configs->C;

  int offset_rows = 0;
  for (int r = 0; r < size; r++) {
    int r_rows = r < remainder 
      ? tamanho_faixa_de_celulas_base + 1 
      : tamanho_faixa_de_celulas_base;

    sendv_posicao_celulas[r] = r_rows * C * (int)sizeof(Celula);
    offset_posicao_celulas[r] = offset_rows * C * (int)sizeof(Celula);
    sendv_posicao_vet_ativacao[r] = r_rows * C;
    offset_posicao_vet_ativacao[r] = offset_rows * C;
    offset_rows += r_rows;
  }
}

/**
 * Transmissão de mensagem dos índices da matrix trabalhados por cada processo
 */
int mpi_run_simulation(InputConfigs* configs, int rank, int size) {
  const int C = configs->C;
  const int L = configs->L;
  
  // vetor de posicao das celulas
  //  - necessario que offset considere tamanho do objeto Celula para deslocamento
  int *sendv_posicao_celulas = NULL;
  int *offset_posicao_celulas = NULL;

  // vetor de posicao no vetor de ativaoca
  //  - offset considera posicoes de int
  int *sendv_posicao_vet_ativacao = NULL;
  int *offset_posicao_vet_ativacao = NULL;

  // Divisão inteira
  // - total de celulas na faixa disponibilizada ao processo de rank r
  const int tamanho_faixa_de_celulas_base = L / size;

  // Resto
  // - caso divisão inexata, aumento de 1 linha no tamanho da faixa
  const int remainder  = L % size;
  // - cada rank r ganha uma linha, enquanto resto existir
  const int tamanho_faixa = rank < remainder 
    ? tamanho_faixa_de_celulas_base + 1 
    : tamanho_faixa_de_celulas_base;

  // Alocação de vetores de posição e deslocamento apenas por rank 0
  if (rank == 0) {
    sendv_posicao_celulas = (int*) malloc(size * sizeof(int));
    offset_posicao_celulas = (int*) malloc(size * sizeof(int));

    sendv_posicao_vet_ativacao = (int*) malloc(size * sizeof(int));
    offset_posicao_vet_ativacao = (int*) malloc(size * sizeof(int));

    if (!sendv_posicao_celulas || !offset_posicao_celulas || !sendv_posicao_vet_ativacao || !offset_posicao_vet_ativacao) {
      fprintf(stderr, "Falha ao alocar vetores de deslocamento.\n");
      MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    calculate_faixas_de_celulas(
      configs, size, rank, tamanho_faixa_de_celulas_base, remainder,
      sendv_posicao_celulas, offset_posicao_celulas, 
      sendv_posicao_vet_ativacao, offset_posicao_vet_ativacao
    );
  }

  // Matrizes e vetores globais
  Celula  *global_matrix_atual = NULL;
  int *global_vetor_ativacao = NULL;

  Metrics *vetor_tempo_atual = NULL;
  Metrics *vetor_proximo_tempo = NULL;
  bool is_setup_ok = true;

  // Apenas rank 0 constroi mapas e vetores
  if (rank == 0) {
    global_matrix_atual = build_linear_state_matrix(configs);
    vetor_tempo_atual = build_metrics_vector(configs);
    vetor_proximo_tempo = build_metrics_vector(configs);
    global_vetor_ativacao = build_mapa_contencao(configs, global_matrix_atual);

    if (!global_matrix_atual || !vetor_tempo_atual || !vetor_proximo_tempo || !global_vetor_ativacao) {
      fprintf(stderr, "[Rank 0] Falha em alocar estruturas globais.\n");
      is_setup_ok = false;
    }

    if (!populate_matrix(configs, global_matrix_atual)) {
      fprintf(stderr, "[Rank 0] Falha em popular matriz global.\n");
      is_setup_ok = false;
    } else {
      apply_focos_iniciais_incendio(configs, global_matrix_atual);
    }
  }

  // Broadcast check setup
  MPI_Bcast(&is_setup_ok, 1, MPI_C_BOOL, 0, MPI_COMM_WORLD);
  
  if (!is_setup_ok) {
    // Rank 0 print
    if (rank == 0) {
      cleanup_simulation_setup(configs, global_matrix_atual, NULL, vetor_tempo_atual, vetor_proximo_tempo, global_vetor_ativacao);
      free(sendv_posicao_celulas);
      free(offset_posicao_celulas); 
      free(sendv_posicao_vet_ativacao); 
      free(offset_posicao_vet_ativacao);
    } else {
      // Todos processos limpam sua propria configuração de entrada
      free_input_configs(configs);
    }
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  // Vetores locais para scatter de dados

  /**
   * Faixa original: retangulo (tamanho_faixa x C)
   * Faixa com halo: retangulo ((tamanho_faixa+2) x C)
   */
  const int tamanho_faixa_com_halo = (tamanho_faixa + 2) * C;

  Celula *local_matrix_atual = (Celula*) calloc(tamanho_faixa_com_halo, sizeof(Celula));
  Celula *local_matrix_proximo = (Celula*) calloc(tamanho_faixa_com_halo, sizeof(Celula));

  int *local_vetor_ativacao = (int*) malloc(tamanho_faixa_com_halo * sizeof(int));

  if (!local_matrix_atual || !local_matrix_proximo || !local_vetor_ativacao) {
    fprintf(stderr, "[Rank %d] Falha em alocar memória da faixa local.\n", rank);
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
  }

  // Scater de dados
  // Índice C inicializa para todos processos, pulando primeira linha de halo 
  MPI_Scatterv(
      global_matrix_atual, sendv_posicao_celulas, offset_posicao_celulas, MPI_BYTE,
      &local_matrix_atual[C], tamanho_faixa * C * (int)sizeof(Celula), MPI_BYTE,
      0, MPI_COMM_WORLD
  );

  MPI_Scatterv(
      global_vetor_ativacao, sendv_posicao_vet_ativacao, offset_posicao_vet_ativacao, MPI_INT,
      &local_vetor_ativacao[C], tamanho_faixa * C, MPI_INT,
      0, MPI_COMM_WORLD
  );

  // Remoção de função copy_matrix (presente no T1)
  memcpy(local_matrix_proximo, local_matrix_atual, tamanho_faixa_com_halo * sizeof(Celula));

  int passo_final = run_simulation(
      configs,
      local_matrix_atual,
      local_matrix_proximo,
      local_vetor_ativacao,
      vetor_tempo_atual,
      vetor_proximo_tempo,
      tamanho_faixa,
      rank,
      size
  );

  // ponteiro para matriz de gatherv, com passo atual ou proximo passo
  Celula *gatherv_matrix = (passo_final % 2 == 0) 
    ? local_matrix_atual 
    : local_matrix_proximo;

  // Consolidação de celulas locais para gatherv_matrix
  MPI_Gatherv(
    &gatherv_matrix[C], tamanho_faixa * C * (int)sizeof(Celula), MPI_BYTE,
    global_matrix_atual, sendv_posicao_celulas, offset_posicao_celulas, MPI_BYTE,
    0, MPI_COMM_WORLD
  );

  // Apenas rank 0 calcula checksum e limpa simulação
  if (rank == 0) {
    unsigned long long checksum = calculate_checksum(configs, global_matrix_atual, vetor_tempo_atual);
    print_final_report(passo_final, checksum, vetor_tempo_atual);

    cleanup_simulation_setup(configs, global_matrix_atual, NULL, vetor_tempo_atual, vetor_proximo_tempo, global_vetor_ativacao);
    free(sendv_posicao_celulas);
    free(offset_posicao_celulas);
    free(sendv_posicao_vet_ativacao);
    free(offset_posicao_vet_ativacao);
  } else {
    // Todas limpam configurações locais alocadas
    free_input_configs(configs);
  }

  // Liberar matrizes locais
  free(local_matrix_atual);
  free(local_matrix_proximo);
  free(local_vetor_ativacao);

  return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {

  // Inicialização de execução com MPI
  // ===========================
  int provided = 0;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

  if (provided < MPI_THREAD_FUNNELED) {
    fprintf(stderr, "Erro: MPI não suporta MPI_THREAD_FUNNELED.\n");
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    return EXIT_FAILURE;
  }

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  // ===========================
  
  // A partir de agora, os fluxos precisam ser pensados entre
  // rank == 0: main
  // rank != 0: demais

  if (!is_valid_single_input_argument(argc)) {
    // Apenas processo UID 0 (rank=0) imprime mensagem de erro
    if (rank == 0) perror("Entrada inválida. A execução do programa requer um argumento: ./program <input_configs_filename>\n");

    // Todas finalizam
    MPI_Finalize();
    return EXIT_FAILURE;
  }
  
  InputConfigs* configs = NULL;
  
  // Apenas rank 0 lê e valida configurações
  if (rank == 0) {
    // Processamento para rank 0
    configs = load_input_configs(argv[1]);
    if (!configs || !is_valid_input_configs(configs)) {
      perror("Arquivo de configurações inválido.\n");
      free_input_configs(configs);
      configs = NULL;
    }
  }

  // Caso input configs válido, brodcast para demais processos de rank != 0
  if (!broadcast_input_configs(&configs, rank)) {
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  // Todos processos printam
  // print_loaded_input_configs(configs);

  mpi_run_simulation(configs, rank, size);
  
  // Todos finalizam
  MPI_Finalize();
  return EXIT_SUCCESS;
}
