#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#define ordem 4
#define tam_registro (41 + (ordem + 1) * 26) // mudar
#define TAMANHO_REGISTRO 192
int prox_RRN = 0;
int raiz = 0;

// -----------------------------------------------------------------------
// Declaração de structs

typedef struct film
{
    char codigo[6];
    char titulo_original[56];
    char titulo_pt[58];
    char diretor[40];
    char ano[5];
    char pais[21];
    int nota;
    char padding[2]; // Para o que sobrou, preenche com @, pois não dava 192 bytes
} filme;

typedef struct node
{
    int RRN;               // RNN do próprio nó no arquivo de indice (arvore B+)
    int folha;             // Para ver se é folha
    char chaves[ordem][6]; // vetor de chaves
    int no_RRN[ordem];     // RRNs de dados associados a cada chave (somente em folha)
    int filhos[ordem + 1]; // Ponteiros** (RNNs dos nós no arquivo) para os filhos
    int numKeys;           // número de chaves inseridas na página
    int parent;            // referência para o pai
    int next_node;         // referência para o próximo nó/página da lista (se o nó é folha)
} node;

typedef struct sec
{
    char titulo_original[56];
    int proximo;
    int posicao_primario;
} indice_sec;

// Struct de primarios
typedef struct prim
{
    char id_prim[6];
    int prox;
} primarios_lista;

// Funções árvore B+

void promove_chave(node **no_esq, node **no_dir, char *chave_promovida, FILE *arvore);
void split(node **no, FILE *arvore);

void inicializar_no(node **no) // Malloc no nó da árvore e inicializa oss dados
{
    *no = malloc(sizeof(node));
    (*no)->RRN = 0;
    (*no)->folha = 1;
    (*no)->numKeys = 0;
    (*no)->parent = -1;
    (*no)->next_node = -1;

    for (int i = 0; i < ordem; i++)
    {
        (*no)->chaves[i][0] = '#';
        (*no)->no_RRN[i] = -1;
    }
    for (int i = 0; i < ordem + 1; i++)
        (*no)->filhos[i] = -1;
}

void ler_pagina(FILE *arquivo, node **no, int RRN) // Leitua do nó no arquivo, levando em conta o header
{
    char buffer[10];
    char teste, teste2;
    fseek(arquivo, 0, SEEK_SET);
    fgets(buffer, sizeof(buffer), arquivo);
    sscanf(buffer, "%d,%d", &raiz, &prox_RRN);

    fgetc(arquivo);

    fseek(arquivo, RRN * tam_registro, SEEK_CUR);
    fscanf(arquivo, "%d", &(*no)->RRN);
    fgetc(arquivo);
    fscanf(arquivo, "%d", &(*no)->folha);
    fgetc(arquivo);

    fgetc(arquivo);

    for (int i = 0; i < ordem; i++)
    {
        fscanf(arquivo, "%[^,()]", (*no)->chaves[i]);
        fgetc(arquivo);
    }
    fgetc(arquivo);
    fgetc(arquivo);
    for (int i = 0; i < ordem; i++)
    {
        fscanf(arquivo, "%d", &(*no)->no_RRN[i]);
        fgetc(arquivo);
    }
    fgetc(arquivo);
    fgetc(arquivo);

    for (int i = 0; i < ordem + 1; i++)
    {
        fscanf(arquivo, "%d", &(*no)->filhos[i]);
        fgetc(arquivo);
    }
    fgetc(arquivo);

    fscanf(arquivo, "%d", &(*no)->numKeys);
    fgetc(arquivo);

    fscanf(arquivo, "%d", &(*no)->parent);
    fgetc(arquivo);

    fscanf(arquivo, "%d", &(*no)->next_node);
}

// Escrever quando o nó não existe na árvore
void escrever_arquivo_arvore(node **no, FILE *arquivo, int RRN) // Isso aqui não funciona
{
    int count = 0, i, j, k;
    fseek(arquivo, 0, SEEK_SET);
    fprintf(arquivo, "%d,", raiz);
    fprintf(arquivo, "%d\n", prox_RRN);
    fseek(arquivo, (*no)->RRN * tam_registro, SEEK_CUR);
    i = ftell(arquivo);
    fprintf(arquivo, "(%d,", (*no)->RRN);
    fprintf(arquivo, "%d,", (*no)->folha);
    fprintf(arquivo, "(");
    for (int i = 0; i < ordem; i++)
    {
        if ((*no)->chaves[i][0] == '\0')
            fprintf(arquivo, "#");
        else
            fprintf(arquivo, "%s", (*no)->chaves[i]);
        if (count < 3)
        {
            fprintf(arquivo, ",");
        }
        count++;
    }
    count = 0;
    fprintf(arquivo, "),");
    fprintf(arquivo, "(");
    for (int i = 0; i < ordem; i++)
    {
        fprintf(arquivo, "%d", (*no)->no_RRN[i]);
        if (count < 3)
        {
            fprintf(arquivo, ",");
        }
        count++;
    }
    fprintf(arquivo, "),");
    fprintf(arquivo, "(");
    count = 0;
    for (int i = 0; i < ordem + 1; i++)
    {
        fprintf(arquivo, "%d", (*no)->filhos[i]);
        if (count < 4)
        {
            fprintf(arquivo, ",");
        }
        count++;
    }
    fprintf(arquivo, "),");
    fprintf(arquivo, "%d,", (*no)->numKeys);
    fprintf(arquivo, "%d,", (*no)->parent);
    fprintf(arquivo, "%d)", (*no)->next_node);

    j = ftell(arquivo);

    k = j - i;
    while (k < tam_registro)
    {
        fprintf(arquivo, "$");
        k++;
    }
}

FILE *ler_arquivo_arvore(char *nome, node **no, bool *flag_1)
{
    FILE *arquivo = fopen(nome, "r+");
    if (arquivo == NULL) // caso seja a primeira vez do arquivo
    {
        arquivo = fopen(nome, "w+");
        inicializar_no(no);
        *flag_1 = true;
    }
    else
    {
        *no = calloc(1, sizeof(node));
        fscanf(arquivo, "%d", &raiz);
        ler_pagina(arquivo, no, raiz);
    }
    return arquivo;
}

// BUsca a folha verificando seu valor em função do pai e dirigindo pelos ponteiros
int buscar_folha(node **no, char *chave, FILE *arquivo, int RRN)
{
    ler_pagina(arquivo, no, RRN);

    int i = 1;

    if ((*no)->folha)
    {
        return (*no)->RRN;
    }
    if (strcmp(chave, (*no)->chaves[0]) < 0)
    {
        return buscar_folha(no, chave, arquivo, (*no)->filhos[0]);
    }
    while (i < (*no)->numKeys && strcmp(chave, (*no)->chaves[i]) >= 0)
    {
        i++;
    }
    return buscar_folha(no, chave, arquivo, (*no)->filhos[i]);
}

// Tentativa de redistribuição na remoção
void redistribuir(node **no, node **no_aux, node **pai, FILE *arquivo, int posicao, int pai_filho_posicao)
{
    if (!posicao)
    {
        strcpy((*no)->chaves[(*no)->numKeys], (*no_aux)->chaves[(*no_aux)->numKeys - 1]);
        (*no_aux)->chaves[(*no_aux)->numKeys - 1][0] = '\0';
        (*no_aux)->numKeys--;
        (*no)->numKeys++;
        strcpy((*pai)->chaves[pai_filho_posicao], (*no)->chaves[0]);
    }
    else
    {
        strcpy((*no)->chaves[(*no)->numKeys], (*no_aux)->chaves[0]);
        (*no_aux)->chaves[0][0] = '\0';
        for (int i = 0; i < (*no)->numKeys - 1; i++)
        {
            strcpy((*no_aux)->chaves[i], (*no_aux)->chaves[i+1]);
        }


        (*no_aux)->numKeys--;
        (*no)->numKeys++;
        strcpy((*pai)->chaves[pai_filho_posicao], (*no_aux)->chaves[0]);

       
    }
     escrever_arquivo_arvore(no, arquivo, (*no)->RRN);
    escrever_arquivo_arvore(no_aux, arquivo, (*no_aux)->RRN);
    escrever_arquivo_arvore(pai, arquivo, (*pai)->RRN);
}

//void concatenar() {

//}

// Tentativa de remoção no nó folha, faltou fazer a concatenação e achar uma forma de usar isso para o nó pai e recursividade para underflows
void remover_na_folha(char *chave, node **no, FILE *arquivo) 
{
    int RRN = buscar_folha(no, chave, arquivo, raiz);
    ler_pagina(arquivo, no, RRN);
    int flag = 0;
    char aux[6];

    for (int i = 0; i < (*no)->numKeys; i++)
    {
        if (!strcmp(chave, (*no)->chaves[i]))
        {
            (*no)->chaves[i][0] = '\0';
            (*no)->numKeys--;
            flag = 1;
        }
        if (flag)
        {
            strcpy(aux, (*no)->chaves[i]);
            strcpy((*no)->chaves[i], (*no)->chaves[i + 1]);
        }
    }

    if ((*no)->numKeys < ordem / 2)
    {
        node *no_aux, *no_aux_2, *pai;
        int posicao;
        inicializar_no(&pai);
        inicializar_no(&no_aux);
        ler_pagina(arquivo, &pai, (*no)->parent);
        for (int i = 0; i < pai->numKeys; i++)
        {
            if (strcmp((*no)->chaves[0], pai->chaves[i]) < 0)
            {
                posicao = i;
                ler_pagina(arquivo, &no_aux, pai->filhos[i]);
                ler_pagina(arquivo, &no_aux_2, pai->filhos[i + 1]);
            }
            else if (strcmp((*no)->chaves[0], pai->chaves[i]) > 0 && i == pai->numKeys - 1)
            {
                posicao = i;
                ler_pagina(arquivo, &no_aux_2, pai->filhos[i + 1]);
                ler_pagina(arquivo, &no_aux, pai->filhos[i]);
            }
        }

        if (no_aux->numKeys > ordem / 2)
        {
            redistribuir(no, &no_aux, &pai, arquivo, 0, posicao);
        }
        else if (no_aux_2->numKeys > ordem / 2)
        {
            redistribuir(no, &no_aux_2, &pai, arquivo, 1, posicao);
        }
        //else
        //{
        //    concatenar();
        //}
    }
}

// Vai para a primeira folha e segue na lista imprimindo os respectivos filmes
void listar_todos_os_filmes(FILE *arvore, node *no, FILE *arquivo_filmes)
{
    filme f;
    char *campo;
    char *delim = "@";
    int rrn;
    char buffer[TAMANHO_REGISTRO + 1];
    printf("\n\033[95m******************\033[0m Lista de filmes \033[95m******************\033[0m\n");
    while (!no->folha)
    {
        ler_pagina(arvore, &no, no->filhos[0]);
        rrn = no->RRN;
    }

    while (rrn != -1)
    {
        ler_pagina(arvore, &no, rrn);

        for (int i = 0; i < no->numKeys; i++)
        {
            fseek(arquivo_filmes, TAMANHO_REGISTRO * no->no_RRN[i], SEEK_SET);
            // Inicializar os campos do struct
            fgets(buffer, TAMANHO_REGISTRO + 1, arquivo_filmes);
            memset(&f, 0, sizeof(filme));

            campo = strtok(buffer, delim); // Separa os campos a partir do @

            strcpy(f.codigo, campo);
            campo = strtok(NULL, delim);
            strcpy(f.titulo_pt, campo);
            campo = strtok(NULL, delim);
            strcpy(f.titulo_original, campo);
            campo = strtok(NULL, delim);
            strcpy(f.diretor, campo);
            campo = strtok(NULL, delim);
            strcpy(f.ano, campo);
            campo = strtok(NULL, delim);
            strcpy(f.pais, campo);
            campo = strtok(NULL, delim);
            f.nota = atoi(campo); // Transforma em int

            printf("\033[95mCodigo: \033[0m%s\n", f.codigo);
            printf("\033[95mTitulo original: \033[0m%s\n", f.titulo_original);
            printf("\033[95mTitulo portugues: \033[0m%s\n", f.titulo_pt);
            printf("\033[95mDiretor: \033[0m%s\n", f.diretor);
            printf("\033[95mAno: \033[0m%s\n", f.ano);
            printf("\033[95mPais: \033[0m%s\n", f.pais);
            printf("\033[95mNota: \033[0m%d\n", f.nota);
            printf("\033[95m*****************************************************\33[0m\n");
        }
        rrn = no->next_node;
    }
}

/*
    Função que busca a chave na árvore B+ e busca a folha correspondente para listar adiante pela lista das folhas.
    Caso a chave não exista, coloca uma mensagem na tela.
*/

void listar_adiante(FILE *arquivo, FILE *arquivo_filmes, node *no, char *chave) 
{
    int rrn = buscar_folha(&no, chave, arquivo, raiz);
    // Chamar de printar
    filme f;
    char *campo;
    char *delim = "@";
    int i;
    char buffer[TAMANHO_REGISTRO + 1];
    printf("\n\033[95m******************\033[0m Lista de filmes \033[95m******************\033[0m\n");

    ler_pagina(arquivo, &no, rrn);
    for (int j = 0; j < no->numKeys; j++)
    {
        if (!strcmp(chave, no->chaves[j]))
        {
            i = j;
            break;
        }
        if (j == no->numKeys - 1)
        {
            printf("NÃO EXISTE ESSA CHAVE \n");
            return;
        }
    }
    while (rrn != -1)
    {
        ler_pagina(arquivo, &no, rrn);

        for (; i < no->numKeys; i++)
        {
            fseek(arquivo_filmes, TAMANHO_REGISTRO * no->no_RRN[i], SEEK_SET);
            // Inicializar os campos do struct
            fgets(buffer, TAMANHO_REGISTRO + 1, arquivo_filmes);
            memset(&f, 0, sizeof(filme));

            campo = strtok(buffer, delim); // Separa os campos a partir do @

            strcpy(f.codigo, campo);
            campo = strtok(NULL, delim);
            strcpy(f.titulo_pt, campo);
            campo = strtok(NULL, delim);
            strcpy(f.titulo_original, campo);
            campo = strtok(NULL, delim);
            strcpy(f.diretor, campo);
            campo = strtok(NULL, delim);
            strcpy(f.ano, campo);
            campo = strtok(NULL, delim);
            strcpy(f.pais, campo);
            campo = strtok(NULL, delim);
            f.nota = atoi(campo); // Transforma em int

            printf("\033[95mCodigo: \033[0m%s\n", f.codigo);
            printf("\033[95mTitulo original: \033[0m%s\n", f.titulo_original);
            printf("\033[95mTitulo portugues: \033[0m%s\n", f.titulo_pt);
            printf("\033[95mDiretor: \033[0m%s\n", f.diretor);
            printf("\033[95mAno: \033[0m%s\n", f.ano);
            printf("\033[95mPais: \033[0m%s\n", f.pais);
            printf("\033[95mNota: \033[0m%d\n", f.nota);
            printf("\033[95m*****************************************************\33[0m\n");
        }
        rrn = no->next_node;
        i = 0;
    }
}

/*
 Função que cria nova raiz quando dá split na raiz.
 Atualiza os pais dos nós filhos.
*/
void criar_nova_raiz(int RRN_filho_esq, int RRN_filho_dir, FILE *arquivo, char *chave_promovida)
{
    node *no;

    inicializar_no(&no);
    strcpy(no->chaves[0], chave_promovida);
    no->filhos[0] = RRN_filho_esq;
    no->filhos[1] = RRN_filho_dir;
    no->numKeys++;
    no->folha = false;
    no->RRN = prox_RRN;
    prox_RRN++;
    raiz = no->RRN;
    escrever_arquivo_arvore(&no, arquivo, no->RRN);
}


/*
    Função para o split, copia metade das chaves e dados de um nó e atualiza.
    Verifica se é na folha ou não para tratamento correto.
    Chama a função de promoção.
*/
void split(node **no, FILE *arvore)
{
    node *novo_no;
    int i;
    char promovido[6];

    inicializar_no(&novo_no);
    i = (*no)->numKeys / 2;

    for (int j = 0; j < i; j++)
    {
        strcpy(novo_no->chaves[j], (*no)->chaves[(*no)->numKeys - i + j]);
        (*no)->chaves[j + i][0] = '\0';
        novo_no->no_RRN[j] = (*no)->no_RRN[j + i];
        (*no)->no_RRN[j + i] = -1;
        novo_no->numKeys++;
        if (!(*no)->folha)
        {
            novo_no->filhos[j] = (*no)->filhos[j + i + 1];
            (*no)->filhos[j + i + 1] = -1;
        }
    }

    (*no)->numKeys = (*no)->numKeys - i;
    novo_no->RRN = prox_RRN;
    prox_RRN++;
    novo_no->parent = (*no)->parent;
    strcpy(promovido, novo_no->chaves[0]);
    if (!(*no)->folha)
    {
        novo_no->next_node = -1;
        (*no)->next_node = -1;
        novo_no->folha = 0;
        novo_no->numKeys--;
        strcpy(novo_no->chaves[0], novo_no->chaves[1]);
        novo_no->chaves[1][0] = '\0';
    }
    else
    {
        novo_no->next_node = (*no)->next_node;
        (*no)->next_node = novo_no->RRN;
    }
    escrever_arquivo_arvore(no, arvore, (*no)->RRN);
    escrever_arquivo_arvore(&novo_no, arvore, novo_no->RRN);
    if (!(*no)->folha)
    {
        node *filho;
        inicializar_no(&filho);
        for (int h = 0; h <= novo_no->numKeys; h++)
        {
            ler_pagina(arvore, &filho, novo_no->filhos[h]);
            filho->parent = novo_no->RRN;
            escrever_arquivo_arvore(&filho, arvore, filho->RRN);
        }
    }
    promove_chave(no, &novo_no, promovido, arvore);
}

/*
    Faz a promoção da chave pegando a primeira chave do nó direito.
    Atualiza os nós pai também.
*/
void promove_chave(node **no_esq, node **no_dir, char *chave_promovida, FILE *arvore)
{
    if ((*no_esq)->RRN == raiz)
    {
        (*no_esq)->parent = (*no_dir)->parent = prox_RRN;
        escrever_arquivo_arvore(no_esq, arvore, (*no_esq)->RRN);
        escrever_arquivo_arvore(no_dir, arvore, (*no_dir)->RRN);

        criar_nova_raiz((*no_esq)->RRN, (*no_dir)->RRN, arvore, chave_promovida);
    }
    else
    {
        node *pai;
        inicializar_no(&pai);
        ler_pagina(arvore, &pai, (*no_esq)->parent);
        int i;
        i = pai->numKeys - 1;

        while (i >= 0)
        {
            if (strcmp(chave_promovida, pai->chaves[i]) < 0)
            {
                strcpy(pai->chaves[i + 1], pai->chaves[i]);
                strcpy(pai->chaves[i], chave_promovida);
                pai->filhos[i + 2] = pai->filhos[i + 1];

                if (pai->numKeys == 1 || i == 0)
                {
                    pai->numKeys++;
                    pai->filhos[i + 1] = (*no_dir)->RRN;
                }

                pai->no_RRN[i] = -1;
            }
            else if (strcmp(chave_promovida, pai->chaves[i]) > 0)
            {
                strcpy(pai->chaves[i + 1], chave_promovida);
                pai->numKeys++;
                pai->filhos[i + 2] = (*no_dir)->RRN;
                break;
            }
            else
            {
                printf("Não é possível inserir essa chave!!! CHAVE NÃO PERMITIDA \n");
                return;
            }
            i--;
        }
        escrever_arquivo_arvore(&pai, arvore, pai->RRN);

        if (pai->numKeys == ordem)
        {
            split(&pai, arvore);
        }
    }
}

/*
    Função de inserção na folha, apenas insere e verifica se precisa ou não de split.
*/
void inserir_no_folha(char *chave, node **no, int RRN_data, FILE *arquivo, int RRN)
{
    if (RRN < 0)
        RRN = buscar_folha(no, chave, arquivo, raiz);
    char promovido[6];
    int i;
    i = (*no)->numKeys - 1;
    if (RRN >= 0)
    {
        if (i == -1)
        {
            strcpy((*no)->chaves[i + 1], chave);
            (*no)->no_RRN[i + 1] = RRN_data;
            (*no)->numKeys++;
            prox_RRN++;
            escrever_arquivo_arvore(no, arquivo, (*no)->RRN);
            return;
        }

        while (i >= 0)
        {
            if (strcmp(chave, (*no)->chaves[i]) < 0)
            {
                strcpy((*no)->chaves[i + 1], (*no)->chaves[i]);
                strcpy((*no)->chaves[i], chave);
                (*no)->no_RRN[i + 1] = (*no)->no_RRN[i];

                if ((*no)->numKeys == 1 || i == 0)
                {
                    (*no)->no_RRN[i] = RRN_data;
                    (*no)->numKeys++;
                }
                escrever_arquivo_arvore(no, arquivo, (*no)->RRN);
            }
            else if (strcmp(chave, (*no)->chaves[i]) > 0)
            {
                strcpy((*no)->chaves[i + 1], chave);
                (*no)->no_RRN[i + 1] = RRN_data;
                (*no)->numKeys++;
                escrever_arquivo_arvore(no, arquivo, (*no)->RRN);
                break;
            }
            else
            {
                printf("Não é possível inserir essa chave!!! CHAVE NÃO PERMITIDA \n");
                return;
            }
            i--;
        }
    }

    if ((*no)->numKeys == ordem)
    {
        split(no, arquivo);
    }
}

// ==================================================================================================
// Lista invertida

/**
 * Funcao para inserção na lista secundaria por ordem de nome
 * Passa os valores de posicoes em ponteiros para modificar o valor fora do escopo da funcao
 */
void inserir_secundario(int *head, int *ppl, indice_sec *lista, char *titulo, int posicao_prim)
{
    strcpy(lista[*ppl].titulo_original, titulo);
    lista[*ppl].posicao_primario = posicao_prim; // Copia na primeira posicao livre

    if (*head == -1) // Se for o primeiro elemento, tem que verificar o head (modificar seu valor)
    {
        (*head) = *ppl;
        lista[*ppl].proximo = -1;
        (*ppl)++;

        return;
    }

    if (strcmp(titulo, lista[*head].titulo_original) < 0) // Verifica se e menor que o head, pois nesse caso, o valor de head modifica
    {
        lista[*ppl].proximo = *head;
        *head = *ppl;
        (*ppl)++;
        return;
    }

    int atual = *head;
    int anterior = -1;

    while (atual != -1 && strcmp(titulo, lista[atual].titulo_original) > 0) // Percorre a lista ate achar a posicao
    {
        anterior = atual;
        atual = lista[atual].proximo;
    }

    lista[*ppl].proximo = atual; // Aqui encontrou sua posicao

    if (anterior != -1)
    {
        lista[anterior].proximo = *ppl; // Ultimo elemento da lista
    }
    else
    {
        *head = *ppl;
    }

    (*ppl)++;
}

/**
 * Busca na lista secundaria o titulo e retorna a posicao dele na lista
 */
int buscar_lista_sec(int head, indice_sec *lista_sec, char *titulo)
{
    int i = head;
    if (head == -1) // Se a lista esta vazia
        return -1;

    do
    {
        if (strcmp(lista_sec[i].titulo_original, titulo) == 0) // Busca o lemento e retorna a posicao se achar
            return i;

        i = lista_sec[i].proximo;
    } while (i != -1);

    return -1;
}

/**
 * Busca o nome na lista secundaira, para verificar se ele ja existe  ou  nao
 */
int buscar_lista_secundaria_para_operacao(int head, indice_sec *lista_sec, primarios_lista *lista_invertida, char *titulo)
{
    int i = head;
    if (head == -1) // Caso a lista esteja vazia
        return -1;

    do
    {
        if (strcmp(lista_sec[i].titulo_original, titulo) == 0 && lista_invertida[lista_sec[i].posicao_primario].id_prim[0] != '\0')
            return lista_sec[i].posicao_primario; // Porcura o elemennto valido da lsita secundaria (busca por nome na funcaao)

        i = lista_sec[i].proximo;

    } while (i != -1);

    return -1;
}

/*
 * Busca pela posicao do primario na lsita invertida
 */
int busca_sec_cod_prim(int head, indice_sec *lista_sec, int index)
{
    int i = head;

    if (head == -1)
    {
        return -1;
    }

    do
    {
        if (lista_sec[i].posicao_primario == index) // Busca o index
            return i;
        i = lista_sec[i].proximo;
    } while (lista_sec[i].proximo != -1);

    return -1;
}

// ==================================================================================================

// Verificações:

/**
 * Gera codigo primario a partir do nome do diretor e o ano. (3 ultimos caracteres do nome e 2 ultimos do ano)
 * Recebe tambem a string que ele vai modificar (por tipos incompativeis, ele  nao retorna char*)
 */
void gerar_codigo(char *ano, char *nome_dir, char *cod)
{
    int i;
    int tam = strlen(nome_dir);
    int k = 0;

    for (i = 0; i < 3; i++)
    {
        cod[k] = toupper(nome_dir[i]);
        k++;
    }

    cod[3] = ano[2];
    cod[4] = ano[3];
    cod[5] = '\0';
}

/**
 * Verifica a integridade do arquivo se ele existir -> os arquivos de indice
 * Verifica se tem 0 ou 1 no cabeçalho
 */
bool verifica_integridade(FILE *arquivo)
{
    char linha[2];
    if (fgets(linha, sizeof(linha), arquivo))
    {
        int num = atoi(linha); // Transforma o char lido em int
        if (num)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        printf("\033[31m\t\tNAO INTEGRO\033[0m\n");
        return false;
    }
}
// ================================================================================================

// Funções de pegar dados do usuário:

/**
 * Funcao que pega os dados do usuario para inserir no arquivo e chama a funcao para gerar codigo
 * Devolve (return) a posicao para colocar no offset
 * recebe o char*indice_prim para modifica-lo dentro da funcao
 */
int escreve_arq_filme(FILE *filmes, char *indice_prim, char *titulo)
{
    filme f;
    printf("\n\033[95m*****************************************************\33[0m");
    printf("\n\t\t\033[32mInsira os dados: \n");
    printf("\033[31mEVITE ACENTUACAO\n");
    printf("\33[95mTitulo pt: \33[0m");
    scanf(" %58[^\n]s", f.titulo_pt);
    while (getchar() != '\n')
        ; // Limpar buffer de entrada
    printf("\33[95mTitulo original: \33[0m");
    scanf(" %56[^\n]s", f.titulo_original);
    while (getchar() != '\n')
        ; // Limpar buffer de entrada
    strcpy(titulo, f.titulo_original);
    printf("\33[95mDiretor (sobrenome, nome): \33[0m");
    scanf(" %40[^\n]s", f.diretor);
    while (getchar() != '\n')
        ; // Limpar buffer de entrada
    printf("\33[95mDigite o ano: \33[0m");
    scanf(" %s", f.ano);
    printf("\33[95mDigite o país: \33[0m");
    scanf(" %20[^\n]s", f.pais);
    while (getchar() != '\n')
        ; // Limpar buffer de entrada
    printf("\33[95mDigite a nota: \33[0m");
    scanf("%d", &f.nota);
    if (f.nota < 0 || f.nota > 9)
    {
        do
        {
            printf("\33[95mDigite uma nota valida: \33[0m");
            scanf("%d", &f.nota);
        } while (f.nota < 0 || f.nota > 9);
    }
    gerar_codigo(f.ano, f.diretor, f.codigo);
    strcpy(indice_prim, f.codigo);
    fseek(filmes, 0, SEEK_END);
    int posicao = (int)ftell(filmes) / 192;
    fprintf(filmes, "%s@%s@%s@%s@%s@%s@%d@",
            f.codigo, f.titulo_pt, f.titulo_original, f.diretor, f.ano, f.pais, f.nota); // coloca os dados no  arquivo

    int tamanho_campos_texto = strlen(f.codigo) + strlen(f.titulo_pt) + strlen(f.titulo_original) +
                               strlen(f.diretor) + strlen(f.pais) + 7 + strlen(f.ano); // 7 caracteres @

    // int tamanho_fixo =  sizeof(f.nota);

    int tamanho_total = tamanho_campos_texto + 1;

    int qtd_hashtag = TAMANHO_REGISTRO - tamanho_total; // Achar quantos # precisam ser colocadas

    for (int i = 0; i < qtd_hashtag; i++)
    {
        fprintf(filmes, "#");
    }

    return posicao;
}

// ==================================================================================

// =====================================================================================================
// Função de menu
/*
1. Inserir um novo filme no cat ́alogo.
2. Remover um filme a partir da chave prim ́aria.
3. Modificar o campo nota de um filme a partir da chave prim ́aria.
4. Buscar filmes a partir da (a) chave prim ́aria (Arvore  ́ B+) ou do (b) t ́ıtulo em
portuguˆes ( ́ındice secund ́ario).
5. Listar todos os filmes no cat ́alogo ordenados pela chave prim ́aria usando o conjunto
de sequˆencias (lista de folhas) da Arvore  ́ B+.
*/
/**
 * Funcao que exibe o menu na tela e devolve a opcao que o usuario escolher
 */
int menu()
{
    int op;

    do
    {
        printf("\n\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[0m   LOCADORA   \033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\033[95m*\033[36m-\n");
        printf("\033[95m-------------------------------------------------------\n\n");
        printf("\033[35m1)\033[0m  Inserir novo filme no catalogo\n");
        // printf("\033[35m2)\033[0m  Remover filme a partir de chave primaria\n");
        printf("\033[35m2)\033[0m  Modificar a nota de um filme a partir da chave primaria\n");
        printf("\033[35m3)\033[0m  Buscar filme por chave primaria\n");
        printf("\033[35m4)\033[0m  Buscar filme por chave secundaria\n");
        printf("\033[35m5)\033[0m  Listar todos os filmes do catalogo\n");
        printf("\033[35m6)\033[0m  Listar todos os filmes adiante\n");

        // printf("\033[35m7)\033[0m  Compactar\n");
        printf("\033[35m0)\033[0m  Encerrar\n");
        printf("\033[32mDigite sua opcao: \33[0m");
        scanf("%d", &op);

        if (op < 0 || op > 6)
        {
            printf("\t\t  \033[31mOpcao invalida\033[0m\n");
        }
    } while (op < 0 || op > 6);

    return op;
}

filme *buscar_prim(node *no, char *indice_prim, FILE *arquivo_filme, FILE *arvore)
{
    int rrn_primario;
    int i;
    rrn_primario = buscar_folha(&no, indice_prim, arvore, raiz);

    filme *registro_filme = malloc(sizeof(filme));

    if (rrn_primario == -1)
    {
        printf("Erro\n");
        return NULL;
    }

    ler_pagina(arvore, &no, rrn_primario);

    for (i = 0; i < no->numKeys; i++)
    {
        if (!strcmp(indice_prim, no->chaves[i]))
        {
            break;
        }
        if (i == no->numKeys - 1)
        {
            printf("NÃO EXISTE ESSA CHAVE \n");
            return NULL;
        }
    }

    fseek(arquivo_filme, no->no_RRN[i] * TAMANHO_REGISTRO, SEEK_SET);

    if (!feof(arquivo_filme))
    {
        fscanf(arquivo_filme, " %[^@^#]s", registro_filme->codigo);
        fscanf(arquivo_filme, "%c", &registro_filme->padding[0]);
        fscanf(arquivo_filme, " %[^@^#]s", registro_filme->titulo_pt);
        fscanf(arquivo_filme, "%c", &registro_filme->padding[0]);
        fscanf(arquivo_filme, " %[^@^#]s", registro_filme->titulo_original);
        fscanf(arquivo_filme, "%c", &registro_filme->padding[0]);
        fscanf(arquivo_filme, " %[^@^#]s", registro_filme->diretor);
        fscanf(arquivo_filme, "%c", &registro_filme->padding[0]);
        fscanf(arquivo_filme, "%[^@^#]s", registro_filme->ano);
        fscanf(arquivo_filme, "%c", &registro_filme->padding[0]);
        fscanf(arquivo_filme, " %[^@^#]s", registro_filme->pais);
        fscanf(arquivo_filme, "%c", &registro_filme->padding[0]);
        fscanf(arquivo_filme, "%d", &registro_filme->nota);

        char c;
        while (c = fgetc(arquivo_filme) == '#') // remover #
            ;

        return registro_filme;
    }
}

FILE *abrir_arquivo_filme(char *nome, bool *flag)
{
    FILE *arquivo;
    arquivo = fopen(nome, "r+a");

    if (!arquivo)
    {
        arquivo = fopen(nome, "w+");
        fclose(arquivo);
        arquivo = fopen(nome, "r+");
        return arquivo;
    }
    *flag = true;
    return arquivo;
}

// Modificar a nota por chave primaria

/**
 * Modifica a nota do registro correspondente, buscando por chave primaria na AVL
 * Com offset, encontra o registro, conta 6 @ e acha a notta e muda
 */
bool modificar_nota(FILE *arquivo_filmes, char *primario, int nova_nota, node *no, FILE *arvore)
{
    int rrn = buscar_folha(&no, primario, arvore, raiz);
    filme f;
    int i;
    int count = 0;
    char caractere;

    if (rrn == -1)
    {
        printf("NÃO EXISTE\n");
        return false;
    }

    ler_pagina(arvore, &no, rrn);

    for (i = 0; i < no->numKeys; i++)
    {
        if (!strcmp(primario, no->chaves[i]))
        {
            break;
        }
        if (i == no->numKeys - 1)
        {
            printf("NÃO EXISTE ESSA CHAVE\n");
            return false;
        }
    }

    if (rrn != -1)
    {
        fseek(arquivo_filmes, no->no_RRN[i] * TAMANHO_REGISTRO, SEEK_SET);
        while ((caractere = fgetc(arquivo_filmes)) != EOF && count < 6)
        {
            if (caractere == '@')
            {
                count++;

                if (count == 6)
                {
                    fprintf(arquivo_filmes, "%d", nova_nota);
                    fflush(arquivo_filmes);
                    return true;
                }
            }
        }
    }
    else
    {
        printf("Índice não encontrado\n");
        return false;
    }
}

// Funções arquivo

/**
 * Abertura dos arquivos de indices, verificando se ja existem ou nao para definir o tipo de abertura
 */
FILE *abrir_arquivo_indices(char *nome, bool *flag)
{
    FILE *arquivo = fopen(nome, "r+a");

    if (arquivo == NULL) // caso seja a primeira vez do arquivo
    {
        arquivo = fopen(nome, "w+");
    }
    else
    {
        *flag = verifica_integridade(arquivo);
    }
    return arquivo;
}

/**
 * Refaz a lista secundaria e invertida caso esteja integra, salvando os dados na estrutura
 */
void salva_lista_secundaria(indice_sec *lista_sec, primarios_lista *lista_invertida, int *ppl, int *head, FILE *sec_index)
{
    char nome[56];
    char titulo[56];
    char buffer[TAMANHO_REGISTRO + 1];
    char *cod;
    char prim[6];
    int pos;
    char *delim = "@";
    fgetc(sec_index);

    while (fgets(buffer, TAMANHO_REGISTRO + 1, sec_index) != NULL)
    {

        if (strcmp(buffer, "\n") != 0)
        {
            char *posicao_arroba = strchr(buffer, '@');

            cod = strtok(buffer, delim);
            char *titulo = strdup(cod);
            cod = strtok(NULL, delim);
            while (cod)
            {
                strcpy(prim, cod);
                if (prim[strlen(prim) - 1] == '\n')
                    prim[strlen(prim) - 1] = '\0';
                int i;
                for (i = 0; lista_invertida[i].id_prim[0] != '\0'; i++)
                    ;
                strcpy(lista_invertida[i].id_prim, prim);

                int posicao_sec = buscar_lista_sec(*head, lista_sec, titulo);

                if (posicao_sec == -1)
                {
                    inserir_secundario(head, ppl, lista_sec, titulo, i);
                    lista_invertida[i].prox = -1;
                }
                else
                {
                    lista_invertida[i].prox = lista_sec[posicao_sec].posicao_primario;
                    lista_sec[posicao_sec].posicao_primario = i;
                }
                cod = strtok(NULL, delim);
            }
        }
    }
}

/**
 * Chama a funcao para escrever no arquivo de filmes e insere os dados na AVL e lista invertida
 */
void cadastro_filmes(node **raiz, indice_sec *lista_sec, int *head, int *ppl, FILE *filmes, primarios_lista *lista, FILE *arvore)
{
    char indice_prim[6];
    char titulo[58];
    int posicao = escreve_arq_filme(filmes, indice_prim, titulo);
    inserir_no_folha(indice_prim, raiz, posicao, arvore, -1);
    int i;
    for (i = 0; lista[i].id_prim[0] != '\0'; i++)
        ;
    strcpy(lista[i].id_prim, indice_prim);

    int posicao_sec = buscar_lista_sec(*head, lista_sec, titulo); // Para verificar se ja existe esse indice secundario ou nao

    // Insere na lista invertida de acordo com o resultado da posicao_sec
    if (posicao_sec == -1)
    {
        inserir_secundario(head, ppl, lista_sec, titulo, i);
        lista[i].prox = -1;
    }
    else
    {
        lista[i].prox = lista_sec[posicao_sec].posicao_primario;
        lista_sec[posicao_sec].posicao_primario = i;
    }
}

/**
 * Se o arquivo de indices secundarios ja existir e nao estiver integro, salvar na lista invertida seus dados a partir do arquivo de filmes
 */
void grava_indice_sec(indice_sec *lista_sec, primarios_lista *lista_invertida, FILE *indice_sec, FILE *filmes, int *head, int *ppl)
{
    filme f;
    char *campo;
    char *delim = "@";
    char buffer[TAMANHO_REGISTRO + 1];

    fseek(filmes, 0, SEEK_SET);
    fprintf(indice_sec, "1\n");

    while (!feof(filmes))
    {
        if (fgets(buffer, TAMANHO_REGISTRO + 1, filmes) == NULL)
        {
            printf("\t\t       \033[31mFIM DO ARQUIVO");
            return;
        }

        if (buffer[0] == '*' && buffer[1] == '|')
        {
            continue;
        }

        // Inicializar os campos do struct
        memset(&f, 0, sizeof(filme));

        campo = strtok(buffer, delim);

        strcpy(f.codigo, campo);
        campo = strtok(NULL, delim);
        strcpy(f.titulo_pt, campo);
        campo = strtok(NULL, delim);
        strcpy(f.titulo_original, campo);
        campo = strtok(NULL, delim);
        strcpy(f.diretor, campo);
        campo = strtok(NULL, delim);
        strcpy(f.ano, campo);
        campo = strtok(NULL, delim);
        strcpy(f.pais, campo);
        campo = strtok(NULL, delim);
        f.nota = atoi(campo);
        int i;
        for (i = 0; lista_invertida[i].id_prim[0] != '\0'; i++)
            ;
        strcpy(lista_invertida[i].id_prim, f.codigo);

        int posicao_sec = buscar_lista_sec(*head, lista_sec, f.titulo_original);

        if (posicao_sec == -1)
        {
            inserir_secundario(head, ppl, lista_sec, f.titulo_original, i);
            lista_invertida[i].prox = -1;
        }
        else
        {
            lista_invertida[i].prox = lista_sec[posicao_sec].posicao_primario;
            lista_sec[posicao_sec].posicao_primario = i;
        }
    }

    int j = *head;
    int k = lista_sec[j].posicao_primario;

    do
    {
        fprintf(indice_sec, "%s|", lista_sec[j].titulo_original);
        k = lista_sec[j].posicao_primario;
        do
        {
            fprintf(indice_sec, "%s|", lista_invertida[k].id_prim);
            k = lista_invertida[k].prox;
        } while (lista_invertida[k].prox != -1);
        fprintf(indice_sec, "\n");
        j = lista_sec[j].proximo;
    } while (lista_sec[j].proximo != -1);
}

/**
 * Busca pelo indice secundario na lista secundario e acha seu link com a lista invertida, encontrando o offset e buscando por ele no arquivo
 */
void buscar_sec(indice_sec *lista_sec, primarios_lista *lista_invertida, int head, char *titulo, FILE *arquivo_filme, node *raiz, FILE *arvore)
{
    int posicao = buscar_lista_secundaria_para_operacao(head, lista_sec, lista_invertida, titulo);
    filme *f;
    int k;

    if (posicao != -1)
    {
        int i = posicao;

        do
        {
            f = buscar_prim(raiz, lista_invertida[i].id_prim, arquivo_filme, arvore);
            if (!f)
            {
                printf("lista invertida: %s \n", lista_invertida[i].id_prim);
                printf("\033[31m\t\tNão encontrado ou erro\n");
                return;
            }
            printf("\033[95m*************** Dados do filme: *************\n");
            printf("\033[95mCodigo: \033[0m%s\n", f->codigo);
            printf("\033[95mTitulo original: \033[0m%s\n", f->titulo_original);
            printf("\033[95mTitulo português:\033[0m %s\n", f->titulo_pt);
            printf("\033[95mDiretor: \033[0m%s\n", f->diretor);
            printf("\033[95mAno: \033[0m%s\n", f->ano);
            printf("\033[95mPais:\033[0m %s\n", f->pais);
            printf("\033[95mnota: \033[0m%d\n", f->nota);
            printf("\033[95m****************************************************************************\033[0m\n\n");
            i = lista_invertida[i].prox;
        } while (i != -1);
    }
    else
    {
        printf("Titulo invalido\n");
    }
}

/**
 * Encerra os arquivos primario e secundario salvando dados a AVL e lista invertida
 */
void encerrar_programa(FILE *primario, FILE *secundario, indice_sec *lista_sec, int *head, primarios_lista *lista_invertida)
{
    fclose(secundario);
    fclose(primario);
    secundario = fopen("ititle.idx", "w");
    fprintf(secundario, "1\n");
    int secundario_impresso = 0;

    int j = *head;
    int k;
    if (*head == -1)
    {
        return;
    }

    do
    {
        k = lista_sec[j].posicao_primario;
        if (lista_invertida[k].id_prim[0] != '\0')
        {
            fprintf(secundario, "%s", lista_sec[j].titulo_original);
            secundario_impresso = 1;
        }
        do
        {
            if (lista_invertida[k].id_prim[0] == '\0')
            {
                break;
            }

            fprintf(secundario, "@%s", lista_invertida[k].id_prim);
            k = lista_invertida[k].prox;
        } while (k != -1);
        if (secundario_impresso)
            fprintf(secundario, "\n");
        j = lista_sec[j].proximo;
        secundario_impresso = 0;
    } while (j != -1);
}

void percorrer_arquivo_filme(FILE *arquivo_filmes, FILE *arvore, node **raiz)
{
    filme f;
    char c;
    char *campo;
    char *delim = "@";
    int posicao;
    rewind(arquivo_filmes);
    char buffer[TAMANHO_REGISTRO + 1];
    while (!feof(arquivo_filmes))
    {
        if (fgets(buffer, TAMANHO_REGISTRO + 1, arquivo_filmes) == NULL)
        {
            printf("\t\t       \033[31mFIM DO ARQUIVO");
            return;
        }

        posicao = (int)ftell(arquivo_filmes) / 192 - 1;

        // Inicializar os campos do struct
        memset(&f, 0, sizeof(filme));

        campo = strtok(buffer, delim); // Separa os campos a partir do @

        strcpy(f.codigo, campo);
        campo = strtok(NULL, delim);
        strcpy(f.titulo_pt, campo);
        campo = strtok(NULL, delim);
        strcpy(f.titulo_original, campo);
        campo = strtok(NULL, delim);
        strcpy(f.diretor, campo);
        campo = strtok(NULL, delim);
        strcpy(f.ano, campo);
        campo = strtok(NULL, delim);
        strcpy(f.pais, campo);
        campo = strtok(NULL, delim);
        f.nota = atoi(campo); // Transforma em int

        inserir_no_folha(f.codigo, raiz, posicao, arvore, -1);
    }
}

int main()
{
    bool flag2 = false, flag_filmes = false, flag_1 = false; // Verificam se os arquivos estavam integros, ou se o arquivo de filmes ja existia
    filme *f;                                                // Registro de filmes
    char chave_prim[6], chave_sec[56];                       // Nossas chaves primarias e secundarias para buscas
    int ppl = 0;                                             // primeira posicao livre da lista invertida -> vai sendo atualizado
    int head = -1;                                           // Cabeca da lista invertida
    node *no;
    primarios_lista lista_invertida[1000]; // Declaracao da lista invertida
    indice_sec lista_sec[900];             // Declaracao da lista secundaria que linka com a invertida                                               // no da arvore de huffman

    FILE *filmes = abrir_arquivo_filme("movies.dat", &flag_filmes);
    FILE *sec_index = abrir_arquivo_indices("ititle.idx", &flag2);
    FILE *pri_index = ler_arquivo_arvore("ibtree.idx", &no, &flag_1);
    int op;
    if (!filmes)
    {
        printf("\t\t\033[31mErro ao abrir o arquivo de filmes!\033[0m\n");
        return 1;
    }

    if (!pri_index)
    {
        printf("\t\t\033[31mErro ao abrir o arquivo de indice primario!\033[0m\n");
        return 1;
    }

    if (!sec_index)
    {
        printf("\t\t\033[31mErro ao abrir o arquivo de indice secundario!\033[0m\n");
        return 1;
    }

    if (flag2) // Se existir e estiver integro, apenas passa os dados para lista invertida
    {
        salva_lista_secundaria(lista_sec, lista_invertida, &ppl, &head, sec_index);
    }
    else if (!flag2 && flag_filmes) // Caso esteja nao integro e exista arquivo de filmes previo
    {
        grava_indice_sec(lista_sec, lista_invertida, sec_index, filmes, &head, &ppl);
    }

    if (flag_filmes && flag_1)
    {
        percorrer_arquivo_filme(filmes, pri_index, &no);
    }

    do
    {
        op = menu();
        switch (op)
        {

        case 1:
        {
            if (!raiz)
            {
                fseek(sec_index, 0, SEEK_SET);
                fprintf(sec_index, "0");
            }
            cadastro_filmes(&no, lista_sec, &head, &ppl, filmes, lista_invertida, pri_index);
            fseek(sec_index, 0, SEEK_SET);

            int flag_pri, flag_sec;
            fscanf(pri_index, "%d", &flag_pri);
            fscanf(sec_index, "%d", &flag_sec);

            if (flag_sec == 1)
            {
                fseek(sec_index, 0, SEEK_SET);
                fprintf(sec_index, "0");
            }
            break;
        }

        case 2:
        {
            printf("\033[33mDigite a chave primaria: \033[0m");
            scanf(" %s", chave_prim);
            for (int i = 0; i < 3; i++)
                chave_prim[i] = toupper(chave_prim[i]);
            printf("\033[35m-------------------------------------\n");
            int nota;
            do
            {
                printf("\033[33mDigite a nova nota: \033[0m");
                scanf(" %d", &nota);
                if (nota < 0 || nota > 9)
                {
                    printf("\033[31mDIGITE NOTA VALIDA\033[0m");
                }
            } while (nota < 0 || nota > 9);

            if (modificar_nota(filmes, chave_prim, nota, no, pri_index))
            {
                printf("\033[33m ------->  Nota modificada com sucesso! <-------\033[0m\n");
            }
            else
            {
                printf("\033[31m\t\tErro ao modificar a nota\033[0m\n");
            }
            break;
        }

        case 3:
        {
            printf("\033[33mDigite a chave primaria: \033[0m");
            scanf("%s", chave_prim);
            for (int i = 0; i < 3; i++)
                chave_prim[i] = toupper(chave_prim[i]);
            f = buscar_prim(no, chave_prim, filmes, pri_index);
            if (!f)
            {
                break;
            }
            printf("\033[95m*************** Dados do filme: *************\n");
            printf("\033[95mCodigo: \033[0m%s\n", f->codigo);
            printf("\033[95mTitulo original: \033[0m%s\n", f->titulo_original);
            printf("\033[95mTitulo português: \033[0m%s\n", f->titulo_pt);
            printf("\033[95mDiretor: \033[0m%s\n", f->diretor);
            printf("\033[95mAno: \033[0m%s\n", f->ano);
            printf("\033[95mPais: \033[0m%s\n", f->pais);
            printf("\033[95mNota:\033[0m %d\n", f->nota);
            printf("\033[95m*******************************************************\033[0m\n\n");

            break;
        }

        case 4:
        {
            printf("\033[33mDigite o titulo: \033[0m");
            scanf(" %[^\n]s", chave_sec);
            buscar_sec(lista_sec, lista_invertida, head, chave_sec, filmes, no, pri_index);
            break;
        }

        case 5:
        {
            listar_todos_os_filmes(pri_index, no, filmes);
            break;
        }

        case 6:
        {
            printf("\033[33mDigite a chave: \033[0m");
            scanf(" %[^\n]s", chave_prim);
            listar_adiante(pri_index, filmes, no, chave_prim);
            break;
        }

        case 0:
        {
            encerrar_programa(pri_index, sec_index, lista_sec, &head, lista_invertida);
            printf("\t\t\033[95mAte a proxima\n\n");
            printf(" \033[33m\t\t    /\\_/\\  \n");
            printf("\t\t   ( o.o ) \n");
            printf("\t\t    > ^ <  \n");

            break;
        }
        }
    } while (op != 0);

    fclose(filmes);
    return 0;
}
