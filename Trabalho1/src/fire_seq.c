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
struct input_configs {
  unsigned int L, C, P, T, SEED, LIMIAR;
  int VENTO_LINHA, VENTO_COLUNA, V;
  int F, Z;
  FocoIncendio** FOCOS_INCENDIO;
  ZonaContencao** ZONAS_CONTENCAO;
};


// Definições de funções
// =====================


struct input_configs* load_input_configs(const char* filepath);
void free_input_configs(struct input_configs* configs);

// Impressão de configurações (debug)
void print_loaded_input_configs(struct input_configs* configs);

bool is_valid_input_configs(struct input_configs* configs);
bool is_valid_single_input_argument(int argc);

bool is_valid_configs_first_line(struct input_configs* configs);
bool is_valid_configs_vento(struct input_configs* configs);
bool is_valid_F_Z(struct input_configs* configs);
bool is_valid_matrix_focos_incendio(struct input_configs* configs);
bool is_valid_matrix_zonas_contencao(struct input_configs* configs);
bool is_valid_foco_incendio_sobre_celula_combustivel(struct input_configs* configs, int id_cobertura, unsigned long long);

// Geração de cobertura
Celula *build_linear_state_matrix(struct input_configs* configs);


/**
 * 5. Validação da entrada
 * 
 * 5.3: L > 0, C > 0, P ≥ 0, T > 0, LIMIAR > 0
 */
bool is_valid_configs_first_line(struct input_configs* configs) {
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
struct input_configs* load_input_configs(const char* filepath) {
  // 4. Formato do arquivo de entrada
  FILE* file = fopen(filepath, "r");

  // 5. Validação da entrada
  // 5.2: Abertura de arquivo de entrada
  if (!file) {
    perror("Erro ao abrir arquivo\n");
    return NULL;
  }

  struct input_configs* configs = (struct input_configs*) malloc(sizeof(struct input_configs));
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
  configs->FOCOS_INCENDIO = (FocoIncendio**) malloc(configs->F * sizeof(FocoIncendio*));
  for (int i = 0; i < configs->F; i++) {
    configs->FOCOS_INCENDIO[i] = (FocoIncendio*) malloc(sizeof(FocoIncendio));
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

  configs->ZONAS_CONTENCAO = (ZonaContencao**) malloc(configs->Z * sizeof(ZonaContencao*));
  for (int i = 0; i < configs->Z; i++) {
    configs->ZONAS_CONTENCAO[i] =  (ZonaContencao*) malloc(sizeof(ZonaContencao));
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

void free_input_configs(struct input_configs* configs) {
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

void print_loaded_input_configs(struct input_configs* configs) {
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

bool is_valid_F_Z(struct input_configs* configs) {
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
bool is_valid_matrix_focos_incendio(struct input_configs* configs) {
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
bool is_valid_matrix_zonas_contencao(struct input_configs* configs) {

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
bool is_valid_configs_vento(struct input_configs* configs) {
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
bool is_valid_input_configs(struct input_configs* configs) {
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
int generate_cobertura(struct input_configs* configs) {
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
int generate_umidade(struct input_configs* configs) {
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
bool is_valid_foco_incendio_sobre_celula_combustivel(struct input_configs* configs, int id_cobertura, unsigned long long idx) {
  // Validação 6.3: Focos sobre células de combustivels

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
bool populate_matrix(struct input_configs* configs, Celula *matrix) {
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

Celula *copy_matrix(struct input_configs* configs, Celula *matrix) {
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

void print_state_matrix(struct input_configs* configs, Celula* matrix, int metric) {
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

void free_simulation_matrix(struct input_configs* configs, Celula* matrix) {
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
Celula *build_linear_state_matrix(struct input_configs* configs) {
  Celula *matrix = (Celula*) malloc((size_t) configs->L * configs->C * sizeof(Celula));
  if (!matrix) return NULL;

  return matrix;
}


Metrics *build_metrics_vector(struct input_configs* configs) {
  Metrics *vector = (Metrics*) malloc(configs->L * configs->C * sizeof(Metrics));
  if (!vector) return NULL;

  // Inicializar com 0
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

/**
 * 6. Descrição do uso das entradas na preparação da simulação
 * 
 * 6.5: Aplicação dos focos iniciais de incêndio
 *  - O tempo inicial de queima será de 02 passos para Vegetação Rasteira e de 04 
 *    passos para Floresta
 */
void apply_focos_iniciais_incendio(struct input_configs* configs, Celula* matrix) {
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
int get_passo_ativacao_if_cell_in_zona_contencao(int row, int col, struct input_configs* configs) {
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
int *build_mapa_contencao(struct input_configs* configs, Celula* matrix) {
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
 * 7. Funcionamento da simulação
 * 
 * 7.2: Ativação de zonas de contenção
 *  - Mudança de estado: Se instante p, intacta -> Contenção
 */
void activate_zonas_contencao(struct input_configs* configs, Celula* matrix, int *vetor_ativacao, int p) {
  if (!configs || !matrix || !vetor_ativacao) return;

  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    if (vetor_ativacao[i] != p) continue;

    // Se diferente de intacta (= 1), mantém estado atual
    if (matrix[i].ID_ESTADO != 1) continue;

    // Se intacta (= 1) transicionar para contenção (= 4): 1 -> 4
    matrix[i].ID_ESTADO = 4;
  }
}

bool check_in_matrix_boundaries(
  struct input_configs* configs, 
  int neighbor_row,
  int neighbor_col
) {
  return (
    (neighbor_row >= 0 && neighbor_row < configs->L) &&
    (neighbor_col >= 0 && neighbor_col < configs->C)
  );
}

/**
 * 8. Cálculo do potencial de ignição
 * 
 * 8.1: Sentido de propagação do vento
 * 
 */
int calculate_potencial_ignicao(struct input_configs* configs, int idx_atual, Celula* matrix_atual) {
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

  // Transformação para matrix linear: idx = x_i * C + x_j;
  //  b = idx - C
  //  g = idx + C
  //  d = idx - 1
  //  e = idx + 1
  
  //  a = idx - C - 1
  //  f = idx - C + 1
  //  c = idx + C - 1
  //  h = idx + C + 1

  // Casos de borda: vizinhos fora da matriz serão ignorados
  
  int C = configs->C;

  int vizinhos_moore[8] = {
    -C, C, -1, 1,         // vizinhos ortogonais
    -C-1, -C+1, C-1, C+1  // vizinhos diagonais
  };

  int linha_celula = idx_atual / C;
  int coluna_celula = idx_atual % C;

  // S = sum(𝑃𝑣)
  int S = 0;
  
  for (int i = 0; i < 8; i++) {
    int idx_vizinho = idx_atual + vizinhos_moore[i];

    int linha_vizinho = idx_vizinho / C;
    int coluna_vizinho = idx_vizinho % C;
    
    int prop_linha = linha_celula - linha_vizinho;
    int prop_coluna = coluna_celula - coluna_vizinho;
    
    // Caso fora dos limites, evitar calculo
    if (!check_in_matrix_boundaries(configs, linha_vizinho, coluna_vizinho)) continue;
    
    // Considerar apernas se vizinho em estado 'em chamas'
    if (matrix_atual[idx_vizinho].ID_ESTADO != 2) continue;

    // Casos peso_basico:
    //  - Vizinho ortogonal: 10
    //  - Vizinho diagonal: 7
    int peso_basico = (abs(prop_linha) + abs(prop_coluna) == 1) ? 10 : 7;

    // 𝐴 = 𝑝𝑟𝑜𝑝_𝑙𝑖𝑛ℎ𝑎 × 𝑣𝑒𝑛𝑡𝑜_𝑙𝑖𝑛ℎ𝑎 + 𝑝𝑟𝑜𝑝_𝑐𝑜𝑙𝑢𝑛𝑎 × 𝑣𝑒𝑛𝑡𝑜_𝑐𝑜𝑙𝑢𝑛a
    int A = prop_linha*configs->VENTO_LINHA + prop_coluna*configs->VENTO_COLUNA;

    // 𝑃𝑣 = max(1, 𝑃básico + 𝑖𝑛𝑡𝑒𝑛𝑠𝑖𝑑𝑎𝑑𝑒 × 𝐴)
    int peso_calculado_int_a = peso_basico + (configs->V * A);
    int peso_vizinho = peso_calculado_int_a > 1 ? peso_calculado_int_a : 1;

    S += peso_vizinho;
  }

  // 𝐼 = floor(𝑆 × 𝑓𝑎𝑡𝑜𝑟_𝑐𝑜𝑚𝑏𝑢𝑠𝑡𝚤𝑣𝑒𝑙 × (100 − 𝑢𝑚𝑖𝑑𝑎𝑑𝑒) / 100)
  return (S * matrix_atual[idx_atual].FATOR_INCENDIO * (100 - matrix_atual[idx_atual].UMIDADE)) / 100 ;
}

/**
 * 7. Funcionamento da simulação
 * 
 * 7.3: Atualização das células
 *  - célula combustível:   permanece não combustível
 *  - célula intacta:       calcular potencial de ignição se potencia > LIMIAR, entrará em chamas 
 *  - célula em chamas:     tempo de queima - 1 se noto tempo = 0, mudança para estado queimada
 *  - célula queimada:      permanece queimada
 *  - célula contenção:     permanece em contenção
 * 
 * 8. Cálculo do potencial de ignição
 */
void update_matrix(struct input_configs* configs, Celula* matrix_atual, Celula* matrix_proximo) {
  for (unsigned long long i = 0; i < configs->L * configs->C; i++) {
    matrix_proximo[i].ID_ESTADO = matrix_atual[i].ID_ESTADO;
    // Célula que permanecem
    // - não combustível (= 0)
    // - queimada (= 3)
    // - contenção (= 4)
    if (
      matrix_atual[i].ID_ESTADO == 0 ||
      matrix_atual[i].ID_ESTADO == 3 ||
      matrix_atual[i].ID_ESTADO == 4
    ) {
      continue;
    }

    // Se célula em chamas (= 2)
    if (matrix_atual[i].ID_ESTADO == 2) {
      // Se tempo_queima = 0, transitar de em chamas para queimada (2 -> 3)
      if (matrix_proximo[i].TEMPO_QUEIMA == 0) {
        matrix_proximo[i].ID_ESTADO = 3;
        continue;
      }

      matrix_proximo[i].TEMPO_QUEIMA--;
      continue;
    }

    // Caso contrário: Célula intacta (= 1), calcular potencial de ignicao
    int potencial_ignicao = calculate_potencial_ignicao(configs, i, matrix_atual);

    // Se potencial_ignicao < LIMIAR, manter intacta (= 1)
    if (potencial_ignicao < configs->LIMIAR) continue;
    
    // Caso contrário, transitar para em chamas (= 2)
    matrix_proximo[i].ID_ESTADO = 2;
    
    // Atualizar tempo de queima
    // Se vegetação rasteira (= 2), tempo de quima = 2
    // Se floresta (= 3), tempo de queima = 4
    matrix_proximo[i].TEMPO_QUEIMA = matrix_atual[i].ID_COBERTURA == 2 ? 2 : 4;
  }
}

/**
 * 9. Condição de parada
 *  - Após P passos: p > P
 *  - Na ausência de células em chamas
 */
bool check_stop_condition(struct input_configs* configs, Metrics item_vetor_tempo) {
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
 * 10. Calculo dos resultados
 * 
 * 10.1: Total de ignições
 *  - incluso total de estados por tempo
 *    importante para pico de ignições e outras análises
 * 
 * 10.2: Percentual queimado
 * 
 * 10.3: Percentual protegido
 */
void calculate_metrics_resultados(
  int p, 
  struct input_configs* configs, 
  Metrics* vetor_tempo_atual, 
  Metrics* vetor_proximo_tempo, 
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
  
  // 𝑝𝑒𝑟𝑐𝑒𝑛𝑡𝑢𝑎𝑙_𝑞𝑢𝑒𝑖𝑚𝑎𝑑𝑜 = 100 × (𝑞𝑢𝑒𝑖𝑚𝑎𝑑𝑎𝑠 + 𝑒𝑚_𝑐ℎ𝑎𝑚𝑎𝑠 / 𝑐𝑜𝑚𝑏𝑢𝑠𝑡𝚤𝑣𝑒𝑖𝑠_𝑖𝑛𝑖𝑐𝑖𝑎𝑖s)
  vetor_tempo_atual[p].PERCENTUAL_QUEIMADO = combustiveis_iniciais == 0 
    ? 0 
    : 100 * ((float)(vetor_tempo_atual[p].QUEIMADAS + vetor_tempo_atual[p].EM_CHAMAS) / combustiveis_iniciais);

  // 𝑝𝑒𝑟𝑐𝑒𝑛𝑡𝑢𝑎𝑙_𝑝𝑟𝑜𝑡𝑒𝑔𝑖𝑑𝑜 = 100 × (𝑐𝑜𝑛𝑡𝑒𝑛𝑐𝑎𝑜 / 𝑐𝑜𝑚𝑏𝑢𝑠𝑡𝑖𝑣𝑒𝑖𝑠_𝑖𝑛𝑖𝑐𝑖𝑎𝑖𝑠)
  vetor_tempo_atual[p].PERCENTUAL_PROTEGIDO = combustiveis_iniciais == 0
    ? 0
    : 100 * ((float)(vetor_tempo_atual[p].CONTENCAO) / combustiveis_iniciais);
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
void run_simulation(
  struct input_configs* configs,
  Celula *matrix_estado_atual,
  Celula *matrix_proximo_estado,
  int *vetor_ativacao,
  Metrics *vetor_tempo_atual,
  Metrics *vetor_proximo_tempo
) {
    /**
   * ========================================
   * PRINT DATA - csv header
   * Relevante para visualização: 
   *  - time_series
   * ========================================
   */
  print_metrics_csv_header();

  int p = 0;
  do {
    // Para cada passo p da simulação
    
    // 1. Ativar as zonas programadas para p
    activate_zonas_contencao(configs, matrix_estado_atual, vetor_ativacao, p);
    
    // 2. Calcular o próximo estado de todas as células
    update_matrix(configs, matrix_estado_atual, matrix_proximo_estado);
    
    // 3. Calcular estatísticas do próximo estado
    calculate_metrics_resultados(p, configs, vetor_tempo_atual, vetor_proximo_tempo, matrix_estado_atual, matrix_proximo_estado);
    
    /**
     * ========================================
     * PRINT DATA - csv data
     * Relevante para visualização: 
     *  - time_series
     * ========================================
     */
    print_metrics(vetor_tempo_atual[p]);
    
    /**
     * ========================================
     * PRINT DATA
     * Relevante para visualização: 
     *  - grid_states
     * ========================================
     */
    // printf("%d %d %d", configs->L, configs->C, p);
    // print_state_matrix(configs, matrix_estado_atual, 0);

    // 4. Trocar as matrizes
    Celula* matrix_tmp = matrix_estado_atual;
    matrix_estado_atual = matrix_proximo_estado;
    matrix_proximo_estado = matrix_tmp; 

    p++;
    // 5. Verificar condição de parada
  } while (check_stop_condition(configs, vetor_tempo_atual[p-1]));
}

/**
 * 10. Calculo dos resultados
 * 
 * 10.4. Checksum
 */
unsigned long long calculate_checksum(struct input_configs* configs, Celula* matrix_estado_atual, Metrics* vetor_tempo_atual) {
  unsigned long long checksum = 0;
  for (long long i = 0; i < configs->L * configs->C; i++) {
    checksum = checksum*31ULL + (unsigned long long)matrix_estado_atual[i].ID_ESTADO;
    checksum = checksum*31ULL + (unsigned long long)vetor_tempo_atual[i].PASSO;
  }
  return checksum;
}


int main(int argc, char* argv[]) {
  if (!is_valid_single_input_argument(argc)) {
    perror("Entrada inválida. A execução do programa requer um argumento: ./program <input_configs_filename>\n");
    return EXIT_FAILURE;
  }

  struct input_configs* configs = load_input_configs(argv[1]);
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

  // print_loaded_input_configs(configs);

  Celula *matrix_estado_atual = build_linear_state_matrix(configs);
  Metrics *vetor_tempo_atual = build_metrics_vector(configs);
  Metrics *vetor_proximo_tempo = build_metrics_vector(configs);
  int *vetor_ativacao = build_mapa_contencao(configs, matrix_estado_atual);

  if (!matrix_estado_atual || !vetor_tempo_atual || !vetor_proximo_tempo || !vetor_ativacao) {
    perror("Falha ao alocar memória para matriz");
    free_simulation_matrix(configs, matrix_estado_atual);
    free_metrics_vector(vetor_tempo_atual);
    free_metrics_vector(vetor_proximo_tempo);
    free_mapa_contencao_vector(vetor_ativacao);
    free_input_configs(configs);
    return EXIT_FAILURE;
  }

  if (!populate_matrix(configs, matrix_estado_atual)) {
    perror("Falha ao popular matriz.\n");
    free_simulation_matrix(configs, matrix_estado_atual);
    free_metrics_vector(vetor_tempo_atual);
    free_metrics_vector(vetor_proximo_tempo);
    free_mapa_contencao_vector(vetor_ativacao);
    free_input_configs(configs);
    return EXIT_FAILURE;
  }

  Celula *matrix_proximo_estado = copy_matrix(configs, matrix_estado_atual);

  apply_focos_iniciais_incendio(configs, matrix_estado_atual);
  apply_focos_iniciais_incendio(configs, matrix_proximo_estado);

  run_simulation(configs, matrix_estado_atual, matrix_proximo_estado, vetor_ativacao, vetor_tempo_atual, vetor_proximo_tempo);

  // unsigned long long checksum = calculate_checksum(configs, matrix_estado_atual, vetor_tempo_atual);
  // printf("Checksum: %llu\n", checksum);

  free_simulation_matrix(configs, matrix_estado_atual);
  free_simulation_matrix(configs, matrix_proximo_estado);
  free_metrics_vector(vetor_tempo_atual);
  free_metrics_vector(vetor_proximo_tempo);
  free_mapa_contencao_vector(vetor_ativacao);
  free_input_configs(configs);
  return EXIT_SUCCESS;
}