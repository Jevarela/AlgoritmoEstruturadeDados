#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>

#define BLOCO_TAMANHO 10
#define NUM_REPETICOES 10000

//paths --------------------------------------------------------------

char pathDataset[] = "jewelry.csv";
char pathOrder[] = "orders.bin";
char pathProducts[] = "products.bin";
char pathIndexOrderId[] = "index_order_id.bin";
char pathIndexProductId[] = "index_product_id.bin";

//structs --------------------------------------------------------------
struct Index {
    int64_t chave;   
    long posicao;    
};

struct IndexHeader {
    int totalEntradas;
};

struct Header {
    int32_t totalRegistros;
    int32_t registrosAtivos;
};

struct Order {
    int64_t idPedido;
    int64_t idCliente;
    char date[30];
    bool del;
};

struct Products {
    int64_t idProduto;
    char categoria[30];
    float preco;
    bool del;
};
// Protótipos ----------------------------------------------------------
// Funções de cabeçalho
struct Header lerCabecalho(FILE *file);
void atualizarCabecalho(FILE *file, struct Header header);
// Criação de arquivos
FILE *criaFileProducts(char path[], char name[]);
FILE *criaFileOrder(char path[], char name[]);
// Inserção e remoção
void inserirProduct(char path[], struct Products novoProduto);
void removerProduct(char path[], int64_t idProduto);
void inserirOrder(char path[], struct Order novoOrder);
void removerOrder(char path[], int64_t idPedido);
// Índices
void criaIndexProduct(char pathData[], char pathIndex[]);
void atualizarIndiceProduct(char pathData[], char pathIndex[]);
void inserirProductIndice(char pathData[], char pathIndex[], struct Products novoProduto);
void removerProductIndice(char pathData[], char pathIndex[], int64_t idProduto);
void criaIndexOrder(char pathData[], char pathIndex[]);
void atualizarIndiceOrder(char pathData[], char pathIndex[]);
void inserirOrderIndice(char pathData[], char pathIndex[], struct Order novoOrder);
void removerOrderIndice(char pathData[], char pathIndex[], int64_t idPedido);
// Buscas
struct Products buscaProductIndice(char pathData[], char pathIndex[], int64_t chave);
struct Order buscaOrderIndice(char pathData[], char pathIndex[], int64_t chave);
// Leitura
void lerProducts(char path[], int quantidade);
void lerOrder(char path[], int quantidade);

// Menu e utilitários
struct Order pesquisaIndexComSaida(char pathData[], char pathIndex[], int64_t chave);
bool arquivoExiste(const char *path);
void verificarOuCriarArquivos();
void verificarOuCriarIndices();
void atualizarIndices();
void menu();


// criação index ----------------------------------------------------
void criaIndexProduct(char pathData[], char pathIndex[]) {
    FILE *file = fopen(pathData, "rb");
    FILE *idx = fopen(pathIndex, "wb+");
    if (!file || !idx) {
        printf("Erro ao abrir arquivos.\n");
        exit(1);
    }

    struct Header header = lerCabecalho(file);

    struct IndexHeader idxHeader = {0};
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    struct Products produto;
    long long pos;
    for (int i = 0; i < header.totalRegistros; i++) {
        pos = sizeof(struct Header) + (long long)i * sizeof(struct Products);
        fseek(file, pos, SEEK_SET);
        if (fread(&produto, sizeof(struct Products), 1, file) != 1) continue;

        
        if (i % BLOCO_TAMANHO == 0 && !produto.del) {
            struct Index entrada;
            entrada.chave = produto.idProduto;
            entrada.posicao = pos;
            fseek(idx, sizeof(struct IndexHeader) + (long long)idxHeader.totalEntradas * sizeof(struct Index), SEEK_SET);
            fwrite(&entrada, sizeof(struct Index), 1, idx);
            idxHeader.totalEntradas++;
        }
    }

    fseek(idx, 0, SEEK_SET);
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    fclose(file);
    fclose(idx);
}

struct Products buscaProductIndice(char pathData[], char pathIndex[], int64_t chave) {
    FILE *idx = fopen(pathIndex, "rb");
    FILE *file = fopen(pathData, "rb");
    struct Products notFound = {.idProduto = -1};

    if (!idx || !file) return notFound;

    struct IndexHeader idxHeader;
    fread(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    int left = 0, right = idxHeader.totalEntradas - 1;
    struct Index entrada;
    long blocoPos = -1;

   
    while (left <= right) {
        int mid = left + (right - left) / 2;
        fseek(idx, sizeof(struct IndexHeader) + mid * sizeof(struct Index), SEEK_SET);
        fread(&entrada, sizeof(struct Index), 1, idx);

        if (entrada.chave == chave) {
            blocoPos = entrada.posicao;
            break;
        } else if (entrada.chave < chave) {
            blocoPos = entrada.posicao;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (blocoPos == -1) {
        fclose(idx);
        fclose(file);
        return notFound;
    }

    // ler o bloco
    fseek(file, blocoPos, SEEK_SET);
    struct Products produto;
    for (int i = 0; i < BLOCO_TAMANHO; i++) {
        if (fread(&produto, sizeof(struct Products), 1, file) != 1) break;
        if (!produto.del && produto.idProduto == chave) {
            fclose(idx);
            fclose(file);
            return produto;
        }
    }

    fclose(idx);
    fclose(file);
    return notFound;
}
void atualizarIndiceProduct(char pathData[], char pathIndex[]) {
    FILE *file = fopen(pathData, "rb");
    FILE *idx = fopen(pathIndex, "wb+");
    if (!file || !idx) {
        printf("Erro ao abrir arquivos para índice.\n");
        return;
    }

    struct Header header = lerCabecalho(file);
    struct IndexHeader idxHeader = {0};
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx); 

    struct Products produto;
    long pos;
    int ultimoValido = -1;
    long posUltimoValido = -1;

    for (int i = 0; i < header.totalRegistros; i++) {
        pos = sizeof(struct Header) + i * sizeof(struct Products);
        fseek(file, pos, SEEK_SET);
        fread(&produto, sizeof(struct Products), 1, file);

        if (!produto.del) {
            ultimoValido = produto.idProduto;
            posUltimoValido = pos;
        }

        if (i % BLOCO_TAMANHO == 0 && !produto.del) {
            struct Index entrada = {produto.idProduto, pos};
            fseek(idx, sizeof(struct IndexHeader) + idxHeader.totalEntradas * sizeof(struct Index), SEEK_SET);
            fwrite(&entrada, sizeof(struct Index), 1, idx);
            idxHeader.totalEntradas++;
        }
    }

  
    if (idxHeader.totalEntradas == 0 || 
        (ultimoValido != -1 && (header.totalRegistros % BLOCO_TAMANHO != 0))) {
        struct Index entrada = {ultimoValido, posUltimoValido};
        fseek(idx, sizeof(struct IndexHeader) + idxHeader.totalEntradas * sizeof(struct Index), SEEK_SET);
        fwrite(&entrada, sizeof(struct Index), 1, idx);
        idxHeader.totalEntradas++;
    }

   
    fseek(idx, 0, SEEK_SET);
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    fclose(file);
    fclose(idx);
}

void inserirProductIndice(char pathData[], char pathIndex[], struct Products novoProduto) {
    inserirProduct(pathData, novoProduto);      
    atualizarIndiceProduct(pathData, pathIndex);    
}
void removerProductIndice(char pathData[], char pathIndex[], int64_t idProduto) {
    removerProduct(pathData, idProduto);         
    atualizarIndiceProduct(pathData, pathIndex);   
}
void criaIndexOrder(char pathData[], char pathIndex[]) {
    FILE *file = fopen(pathData, "rb");
    FILE *idx = fopen(pathIndex, "wb+");
    if (!file || !idx) {
        printf("Erro ao abrir arquivos.\n");
        exit(1);
    }

    struct Header header = lerCabecalho(file);

    struct IndexHeader idxHeader = {0};
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    struct Order order;
    long long pos;
    for (int i = 0; i < header.totalRegistros; i++) {
        pos = sizeof(struct Header) + (long long)i * sizeof(struct Order);
        fseek(file, pos, SEEK_SET);
        if (fread(&order, sizeof(struct Order), 1, file) != 1) continue;

        if (i % BLOCO_TAMANHO == 0 && !order.del) {
            struct Index entrada;
            entrada.chave = order.idPedido;
            entrada.posicao = pos;
            fseek(idx, sizeof(struct IndexHeader) + (long long)idxHeader.totalEntradas * sizeof(struct Index), SEEK_SET);
            fwrite(&entrada, sizeof(struct Index), 1, idx);
            idxHeader.totalEntradas++;
        }
    }

   
    fseek(idx, 0, SEEK_SET);
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    fclose(file);
    fclose(idx);
}


// busca index ------------------------------------------------------
struct Order buscaOrderIndice(char pathData[], char pathIndex[], int64_t chave) {
    FILE *idx = fopen(pathIndex, "rb");
    FILE *file = fopen(pathData, "rb");
    struct Order notFound = {.idPedido = -1};

    if (!idx || !file) return notFound;

    struct IndexHeader idxHeader;
    fread(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    int left = 0, right = idxHeader.totalEntradas - 1;
    struct Index entrada;
    long blocoPos = -1;

   
    while (left <= right) {
        int mid = left + (right - left) / 2;
        fseek(idx, sizeof(struct IndexHeader) + mid * sizeof(struct Index), SEEK_SET);
        fread(&entrada, sizeof(struct Index), 1, idx);

        if (entrada.chave == chave) {
            blocoPos = entrada.posicao;
            break;
        } else if (entrada.chave < chave) {
            blocoPos = entrada.posicao;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (blocoPos == -1) {
        fclose(idx);
        fclose(file);
        return notFound;
    }

    fseek(file, blocoPos, SEEK_SET);
    struct Order order;
    for (int i = 0; i < BLOCO_TAMANHO; i++) {
        if (fread(&order, sizeof(struct Order), 1, file) != 1) break;
        if (!order.del && order.idPedido == chave) {
            fclose(idx);
            fclose(file);
            return order;
        }
    }

    fclose(idx);
    fclose(file);
    return notFound;
}

// recriação index ------------------------------------------------

void atualizarIndiceOrder(char pathData[], char pathIndex[]) {
    FILE *file = fopen(pathData, "rb");
    FILE *idx = fopen(pathIndex, "wb+");
    if (!file || !idx) {
        printf("Erro ao abrir arquivos para índice.\n");
        return;
    }

    struct Header header = lerCabecalho(file);
    struct IndexHeader idxHeader = {0};
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx); 

    struct Order order;
    long pos;
    int64_t ultimoValido = -1;
    long posUltimoValido = -1;

    for (int i = 0; i < header.totalRegistros; i++) {
        pos = sizeof(struct Header) + i * sizeof(struct Order);
        fseek(file, pos, SEEK_SET);
        fread(&order, sizeof(struct Order), 1, file);

        if (!order.del) {
            ultimoValido = order.idPedido;
            posUltimoValido = pos;
        }

        if (i % BLOCO_TAMANHO == 0 && !order.del) {
            struct Index entrada = {order.idPedido, pos};
            fseek(idx, sizeof(struct IndexHeader) + idxHeader.totalEntradas * sizeof(struct Index), SEEK_SET);
            fwrite(&entrada, sizeof(struct Index), 1, idx);
            idxHeader.totalEntradas++;
        }
    }

 
    if (idxHeader.totalEntradas == 0 || 
        (ultimoValido != -1 && (header.totalRegistros % BLOCO_TAMANHO != 0))) {
        struct Index entrada = {ultimoValido, posUltimoValido};
        fseek(idx, sizeof(struct IndexHeader) + idxHeader.totalEntradas * sizeof(struct Index), SEEK_SET);
        fwrite(&entrada, sizeof(struct Index), 1, idx);
        idxHeader.totalEntradas++;
    }


    fseek(idx, 0, SEEK_SET);
    fwrite(&idxHeader, sizeof(struct IndexHeader), 1, idx);

    fclose(file);
    fclose(idx);
}



void inserirOrderIndice(char pathData[], char pathIndex[], struct Order novoOrder) {
    inserirOrder(pathData, novoOrder);        
    atualizarIndiceOrder(pathData, pathIndex);  
}


void removerOrderIndice(char pathData[], char pathIndex[], int64_t idPedido) {
    removerOrder(pathData, idPedido);             
    atualizarIndiceOrder(pathData, pathIndex);    
}

// Função utilitária: ler cabeçalho -----------------------------------
struct Header lerCabecalho(FILE *file) {
    struct Header header;
    fseek(file, 0, SEEK_SET);
    fread(&header, sizeof(struct Header), 1, file);
    return header;
}

void atualizarCabecalho(FILE *file, struct Header header) {
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(struct Header), 1, file);
}

// criação de arquivos -------------------------------------------------
FILE *criaFileProducts(char path[], char name[]) {
    FILE *csv = fopen(path, "r");
    if (csv == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        exit(1);
    }

    FILE *fileCreate = fopen(name, "wb+");
    if (fileCreate == NULL) {
        printf("Erro ao criar o arquivo %s\n", name);
        fclose(csv);
        exit(1);
    }

    struct Header header = {0, 0};
    fwrite(&header, sizeof(struct Header), 1, fileCreate);

    char line[500];
    struct Products produto;

    while (fgets(line, sizeof(line), csv)) {
        char *campo = strtok(line, ",");
        int collumns = 1;

        while (campo != NULL) {
            if (collumns == 3) 
                produto.idProduto = atoll(campo);
            else if (collumns == 6) { 
                strncpy(produto.categoria, campo, 29);
                produto.categoria[29] = '\0';
            }
            else if (collumns == 8) { 
                produto.preco = atof(campo);
                produto.del = false;
                fwrite(&produto, sizeof(struct Products), 1, fileCreate);
                header.totalRegistros++;
                header.registrosAtivos++;
            }
            collumns++;
            campo = strtok(NULL, ",");
        }
    }

    atualizarCabecalho(fileCreate, header);
    fclose(csv);
    fclose(fileCreate);
    return fileCreate;
}

FILE *criaFileOrder(char path[], char name[]) {
    FILE *csv = fopen(path, "r");
    if (csv == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        exit(1);
    }

    FILE *fileCreate = fopen(name, "wb+");
    if (fileCreate == NULL) {
        printf("Erro ao criar o arquivo %s\n", name);
        fclose(csv);
        exit(1);
    }

    struct Header header = {0, 0};
    fwrite(&header, sizeof(struct Header), 1, fileCreate);

    char line[500];
    struct Order order;

    while (fgets(line, sizeof(line), csv)) {
        char *campo = strtok(line, ",");
        int collumns = 1;
        while (campo != NULL) {
            if (collumns == 1)
                strncpy(order.date, campo, 29);
            else if (collumns == 2)
                order.idPedido = atoll(campo);
            else if (collumns == 9) {
                order.idCliente = atoll(campo);
                order.del = false;
                fwrite(&order, sizeof(struct Order), 1, fileCreate);
                header.totalRegistros++;
                header.registrosAtivos++;
            }
            collumns++;
            campo = strtok(NULL, ",");
        }
    }

    atualizarCabecalho(fileCreate, header);
    fclose(csv);
    fclose(fileCreate);
    return fileCreate;
}

// ordenação -----------------------------------------------------
void ordenarOrders(char path[], int (*compara)(const void *, const void *)) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        exit(1);
    }

    struct Header header = lerCabecalho(file);
    int total = header.totalRegistros;

    struct Order *orders = malloc(total * sizeof(struct Order));
    fseek(file, sizeof(struct Header), SEEK_SET);
    fread(orders, sizeof(struct Order), total, file);
    fclose(file);

    qsort(orders, total, sizeof(struct Order), compara);

    FILE *temp = fopen("temp.bin", "wb+");
    fwrite(&header, sizeof(struct Header), 1, temp);
    fwrite(orders, sizeof(struct Order), total, temp);
    fclose(temp);
    free(orders);

    remove(path);
    rename("temp.bin", path);
}

void ordenarProducts(char path[], int (*compara)(const void *, const void *)) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        exit(1);
    }

    struct Header header = lerCabecalho(file);
    int total = header.totalRegistros;

    struct Products *produtos = malloc(total * sizeof(struct Products));
    fseek(file, sizeof(struct Header), SEEK_SET);
    fread(produtos, sizeof(struct Products), total, file);
    fclose(file);

    qsort(produtos, total, sizeof(struct Products), compara);

    FILE *temp = fopen("temp.bin", "wb+");
    fwrite(&header, sizeof(struct Header), 1, temp);
    fwrite(produtos, sizeof(struct Products), total, temp);
    fclose(temp);
    free(produtos);

    remove(path);
    rename("temp.bin", path);
}

// funções de comparação -----------------------------------------------
int comparaIdPedido(const void *a, const void *b) {
    const struct Order *x = a;
    const struct Order *y = b;
    if (x->idPedido < y->idPedido) return -1;
    if (x->idPedido > y->idPedido) return 1;
    return 0;
}

int comparaIdCliente(const void *a, const void *b) {
    const struct Order *x = a;
    const struct Order *y = b;
    if (x->idCliente < y->idCliente) return -1;
    if (x->idCliente > y->idCliente) return 1;
    return 0;
}

int comparaIdProduto(const void *a, const void *b) {
    const struct Products *x = a;
    const struct Products *y = b;
    if (x->idProduto < y->idProduto) return -1;
    if (x->idProduto > y->idProduto) return 1;
    return 0;
}


// pesquisa binária --------------------------------------
struct Order pesquisaBinariaOrder(char path[], int64_t idPedido) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        exit(1);
    }

    struct Header header = lerCabecalho(file);
    int left = 0, right = header.totalRegistros - 1;
    struct Order order;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        fseek(file, sizeof(struct Header) + mid * sizeof(struct Order), SEEK_SET);
        fread(&order, sizeof(struct Order), 1, file);

        if (order.idPedido == idPedido && !order.del) {
            fclose(file);
            return order;
        } else if (order.idPedido < idPedido)
            left = mid + 1;
        else
            right = mid - 1;
    }

    fclose(file);
    struct Order notFound = {.idPedido = -1};
    return notFound;
}

// pesquisa binária ---------------------------------------
struct Products pesquisaBinariaProducts(char path[], int64_t idProduto) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        exit(1);
    }

    struct Header header = lerCabecalho(file);
    int left = 0, right = header.totalRegistros - 1;
    struct Products prod;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        fseek(file, sizeof(struct Header) + mid * sizeof(struct Products), SEEK_SET);
        fread(&prod, sizeof(struct Products), 1, file);

        if (prod.idProduto == idProduto && !prod.del) {
            fclose(file);
            return prod;
        } else if (prod.idProduto < idProduto)
            left = mid + 1;
        else
            right = mid - 1;
    }

    fclose(file);
    struct Products notFound = {.idProduto = -1};
    return notFound;
}

// leitura e exibição --------------------------------------------------
void lerOrder(char path[], int quantidade) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        return;
    }

    struct Header header = lerCabecalho(file);
    struct Order order;

    printf("Total: %d | Ativos: %d\n", header.totalRegistros, header.registrosAtivos);
    fseek(file, sizeof(struct Header), SEEK_SET);
    int exibidos = 0;
    for (int i = 0; i < header.totalRegistros && exibidos < quantidade; i++) {
        fread(&order, sizeof(struct Order), 1, file);
        if (!order.del) {
            printf("Data: %s | IDCliente: %lld | IDPedido: %lld\n", order.date, order.idCliente, order.idPedido);
            exibidos++;
        }
    }

    printf("Exibidos: %d | Total Ativos: %d\n", exibidos, header.registrosAtivos);
    fclose(file);
}

// inserção --------------------------------------------------------
void inserirProduct(char path[], struct Products novoProduto) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("Erro ao abrir o arquivo %s\n", path);
        return;
    }

    FILE *temp = fopen("temp.bin", "wb+");
    if (!temp) {
        printf("Erro ao criar arquivo temporario\n");
        fclose(file);
        return;
    }

   
    struct Header header = lerCabecalho(file);
    struct Header novoHeader = header;
    fwrite(&novoHeader, sizeof(struct Header), 1, temp);

    struct Products current;
    bool inserido = false;

    for (int i = 0; i < header.totalRegistros; i++) {
        fread(&current, sizeof(struct Products), 1, file);

        if (!inserido && comparaIdProduto(&novoProduto, &current) < 0) {
            fwrite(&novoProduto, sizeof(struct Products), 1, temp);
            novoHeader.totalRegistros++;
            novoHeader.registrosAtivos++;
            inserido = true;
        }

       
        fwrite(&current, sizeof(struct Products), 1, temp);
    }

    
    if (!inserido) {
        fwrite(&novoProduto, sizeof(struct Products), 1, temp);
        novoHeader.totalRegistros++;
        novoHeader.registrosAtivos++;
    }

    
    fseek(temp, 0, SEEK_SET);
    fwrite(&novoHeader, sizeof(struct Header), 1, temp);

    fclose(file);
    fclose(temp);

    remove(path);
    rename("temp.bin", path);
}

void inserirOrder(char path[], struct Order novoOrder) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        printf("Erro ao abrir o arquivo %s\n", path);
        return;
    }

    FILE *temp = fopen("temp.bin", "wb+");
    if (!temp) {
        printf("Erro ao criar arquivo temporario\n");
        fclose(file);
        return;
    }

    
    struct Header header = lerCabecalho(file);
    struct Header novoHeader = header;
    fwrite(&novoHeader, sizeof(struct Header), 1, temp);

    struct Order current;
    bool inserido = false;

    for (int i = 0; i < header.totalRegistros; i++) {
        fread(&current, sizeof(struct Order), 1, file);

        if (!inserido && comparaIdPedido(&novoOrder, &current) < 0) {
            fwrite(&novoOrder, sizeof(struct Order), 1, temp);
            novoHeader.totalRegistros++;
            novoHeader.registrosAtivos++;
            inserido = true;
        }

        
        fwrite(&current, sizeof(struct Order), 1, temp);
    }

   
    if (!inserido) {
        fwrite(&novoOrder, sizeof(struct Order), 1, temp);
        novoHeader.totalRegistros++;
        novoHeader.registrosAtivos++;
    }

    
    fseek(temp, 0, SEEK_SET);
    fwrite(&novoHeader, sizeof(struct Header), 1, temp);

    fclose(file);
    fclose(temp);

    remove(path);
    rename("temp.bin", path);
}

//remove ----------------------------------------------------------

void removerProduct(char path[], int64_t idProduto) {
    FILE *file = fopen(path, "rb+");
    if (!file) {
        printf("Erro ao abrir o arquivo %s\n", path);
        return;
    }

    struct Header header = lerCabecalho(file);
    struct Products produto;
    bool encontrado = false;

  
    for (int i = 0; i < header.totalRegistros; i++) {
        fseek(file, sizeof(struct Header) + i * sizeof(struct Products), SEEK_SET);
        fread(&produto, sizeof(struct Products), 1, file);

        if (!produto.del && produto.idProduto == idProduto) {
            produto.del = true;
            fseek(file, sizeof(struct Header) + i * sizeof(struct Products), SEEK_SET);
            fwrite(&produto, sizeof(struct Products), 1, file);
            header.registrosAtivos--;
            encontrado = true;
            break;
        }
    }

    if (!encontrado) {
        fclose(file);
        printf("Produto com ID %lld não encontrado.\n", idProduto);
        return;
    }

    
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(struct Header), 1, file);
    fflush(file);
    fclose(file);

    
    if (header.registrosAtivos < header.totalRegistros * 0.7) {
        printf("Lixo > 30%%, compactando arquivo de produtos...\n");

        FILE *orig = fopen(path, "rb");
        FILE *temp = fopen("temp.bin", "wb+");
        if (!orig || !temp) {
            printf("Erro ao criar arquivo temporário.\n");
            return;
        }

        struct Header newHeader = header; 
        struct Products tmpProduto;
        int novosAtivos = 0;

        fseek(orig, sizeof(struct Header), SEEK_SET);
        for (int i = 0; i < header.totalRegistros; i++) {
            fread(&tmpProduto, sizeof(struct Products), 1, orig);
            if (!tmpProduto.del) {
                fwrite(&tmpProduto, sizeof(struct Products), 1, temp);
                novosAtivos++;
            }
        }

        newHeader.totalRegistros = novosAtivos;
        newHeader.registrosAtivos = novosAtivos;
        fseek(temp, 0, SEEK_SET);
        fwrite(&newHeader, sizeof(struct Header), 1, temp);

        fclose(orig);
        fclose(temp);

        remove(path);
        rename("temp.bin", path);
    }
}


void removerOrder(char path[], int64_t idPedido) {
    FILE *file = fopen(path, "rb+");
    if (!file) {
        printf("Erro ao abrir o arquivo %s\n", path);
        return;
    }

    struct Header header = lerCabecalho(file);
    struct Order order;
    bool encontrado = false;

    
    for (int i = 0; i < header.totalRegistros; i++) {
        fseek(file, sizeof(struct Header) + i * sizeof(struct Order), SEEK_SET);
        fread(&order, sizeof(struct Order), 1, file);

        if (!order.del && order.idPedido == idPedido) {
            order.del = true;
            fseek(file, sizeof(struct Header) + i * sizeof(struct Order), SEEK_SET);
            fwrite(&order, sizeof(struct Order), 1, file);
            header.registrosAtivos--;
            encontrado = true;
            break;
        }
    }

    if (!encontrado) {
        fclose(file);
        printf("Pedido com ID %lld nao encontrado.\n", idPedido);
        return;
    }

    
    fseek(file, 0, SEEK_SET);
    fwrite(&header, sizeof(struct Header), 1, file);
    fflush(file);
    fclose(file);

    
    if (header.registrosAtivos < header.totalRegistros * 0.7) {
        printf("Lixo > 30%%, compactando arquivo...\n");

        FILE *orig = fopen(path, "rb");
        FILE *temp = fopen("temp.bin", "wb+");
        if (!orig || !temp) {
            printf("Erro ao criar arquivo temporário.\n");
            return;
        }

        struct Header newHeader = header;
        struct Order tmpOrder;
        int novosAtivos = 0;

        
        fseek(orig, sizeof(struct Header), SEEK_SET);
        for (int i = 0; i < header.totalRegistros; i++) {
            fread(&tmpOrder, sizeof(struct Order), 1, orig);
            if (!tmpOrder.del) {
                fwrite(&tmpOrder, sizeof(struct Order), 1, temp);
                novosAtivos++;
            }
        }

        
        newHeader.totalRegistros = novosAtivos;
        newHeader.registrosAtivos = novosAtivos;
        fseek(temp, 0, SEEK_SET);
        fwrite(&newHeader, sizeof(struct Header), 1, temp);

        fclose(orig);
        fclose(temp);

        remove(path);
        rename("temp.bin", path);
    }
}
void lerProducts(char path[], int quantidade) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo %s\n", path);
        return;
    }

    struct Header header = lerCabecalho(file);
    struct Products produto;

    printf("Total: %d | Ativos: %d\n", header.totalRegistros, header.registrosAtivos);
fseek(file, sizeof(struct Header), SEEK_SET);
    int exibidos = 0;
    for (int i = 0; i < header.totalRegistros && exibidos < quantidade; i++) {
        fread(&produto, sizeof(struct Products), 1, file);
        if (!produto.del) {
            printf("IDProduto: %lld | Categoria: %s | Preço: %.2f\n", produto.idProduto, produto.categoria, produto.preco);
            exibidos++;
        }
    }

    printf("Exibidos: %d | Total Ativos: %d\n", exibidos, header.registrosAtivos);
    fclose(file);
}






bool arquivoExiste(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}


void verificarOuCriarArquivos() {
    if (!arquivoExiste(pathProducts)) {
        printf("Criando arquivo de produtos...\n");
        criaFileProducts(pathDataset, pathProducts);
        ordenarProducts(pathProducts, comparaIdProduto);
    }
    if (!arquivoExiste(pathOrder)) {
        printf("Criando arquivo de pedidos...\n");
        criaFileOrder(pathDataset, pathOrder);
        ordenarOrders(pathOrder, comparaIdPedido);
    }
}


void verificarOuCriarIndices() {
    if (!arquivoExiste(pathIndexProductId)) {
        printf("Criando índice de produtos...\n");
        criaIndexProduct(pathProducts, pathIndexProductId);
    } else {
        printf("Atualizando índice de produtos...\n");
        atualizarIndiceProduct(pathProducts, pathIndexProductId);
    }

    if (!arquivoExiste(pathIndexOrderId)) {
        printf("Criando índice de pedidos...\n");
        criaIndexOrder(pathOrder, pathIndexOrderId);
    } else {
        printf("Atualizando índice de pedidos...\n");
        atualizarIndiceOrder(pathOrder, pathIndexOrderId);
    }
}


void atualizarIndices() {
    atualizarIndiceProduct(pathProducts, pathIndexProductId);
    atualizarIndiceOrder(pathOrder, pathIndexOrderId);
}



// NOVO: Structs da Parte 2 (Índices em Memória) -------------------------

struct HashEntry {
    int64_t chave;    
    long posicao;    
    struct HashEntry *proximo;
};

#define TAMANHO_HASH 10007 
struct HashTable {
    struct HashEntry *tabela[TAMANHO_HASH];
};

#define BPLUS_ORDEM 10
#define MAX_CHAVES (BPLUS_ORDEM - 1) 

struct BPlusNode {
    int numChaves;
    bool folha;
    int64_t chaves[MAX_CHAVES];
    long posicoes[MAX_CHAVES];             
    struct BPlusNode *filhos[BPLUS_ORDEM]; 
    struct BPlusNode *proximaFolha;
};

struct BPlusTree {
    struct BPlusNode *raiz;
};

struct BPlusTree *indiceProdMemoria;
struct HashTable *indiceOrderMemoria;


// NOVOS Protótipos (Trabalho II) --------------------------------------------
// Funções de tempo
double medirTempo(clock_t inicio, clock_t fim);
// Árvore B+ (Produtos - idProduto)
struct BPlusTree *criaBPlusTree();
void inserirBPlusTree(struct BPlusTree *tree, int64_t chave, long posicao);
void constroiIndexProductMemoria(char pathData[], struct BPlusTree *tree);
struct Products buscaProductBPlus(char pathData[], struct BPlusTree *tree, int64_t chave);
// Tabela Hash (Pedidos - idCliente)
struct HashTable *criaHashTable();
int hashFunction(int64_t chave);
void inserirHashTable(struct HashTable *hashTable, int64_t chave, long posicao);
void constroiIndexOrderMemoria(char pathData[], struct HashTable *hashTable);
struct Order buscaHashTable(char pathData[], struct HashTable *hashTable, int64_t chave);
// Inicialização e Teste
void carregarIndicesMemoria();
void testeComparativoBusca(char pathProducts[], char pathIndexProducts[], char pathOrders[], char pathIndexOrders[]);
void freeBPlusTree(struct BPlusTree *tree);
void freeHashTable(struct HashTable *hashTable);



double medirTempo(clock_t inicio, clock_t fim) {
    return (double)(fim - inicio) * 1000.0 / CLOCKS_PER_SEC;
}


struct HashTable *criaHashTable() {
    struct HashTable *hashTable = (struct HashTable *)malloc(sizeof(struct HashTable));
    if (!hashTable) exit(1);
    for (int i = 0; i < TAMANHO_HASH; i++) {
        hashTable->tabela[i] = NULL;
    }
    return hashTable;
}

int hashFunction(int64_t chave) {
    return (int)(chave % TAMANHO_HASH);
}

void inserirHashTable(struct HashTable *hashTable, int64_t chave, long posicao) {
    int indice = hashFunction(chave);
    
    struct HashEntry *novaEntrada = (struct HashEntry *)malloc(sizeof(struct HashEntry));
    if (!novaEntrada) exit(1);
    novaEntrada->chave = chave;
    novaEntrada->posicao = posicao;
    
    novaEntrada->proximo = hashTable->tabela[indice];
    hashTable->tabela[indice] = novaEntrada;
}

void constroiIndexOrderMemoria(char pathData[], struct HashTable *hashTable) {
    FILE *file = fopen(pathData, "rb");
    if (!file) {
        printf("Erro ao abrir arquivo de pedidos.\n");
        return;
    }
    
    struct Header header = lerCabecalho(file);
    struct Order order;
    long pos;

    clock_t inicio = clock();

    // Loop de leitura e inserção
    for (int i = 0; i < header.totalRegistros; i++) {
        pos = sizeof(struct Header) + (long)i * sizeof(struct Order);
        fseek(file, pos, SEEK_SET);
        if (fread(&order, sizeof(struct Order), 1, file) != 1) continue;

        if (!order.del) {
            // Chave do índice: idCliente
            inserirHashTable(hashTable, order.idCliente, pos); 
        }
    }

    clock_t fim = clock();
    double tempo_ms = medirTempo(inicio, fim);

    printf("Criacao do Índice HASH (idCliente) em memória concluída em: %.2f ms\n", tempo_ms);
    
    fclose(file);
}

struct Order buscaHashTable(char pathData[], struct HashTable *hashTable, int64_t chave) {
    struct Order notFound = {.idPedido = -1};
    int indice = hashFunction(chave);
    
    struct HashEntry *atual = hashTable->tabela[indice];
    
    // 1. Busca na Tabela Hash em memória
    while (atual != NULL) {
        if (atual->chave == chave) {
            // 2. Acessa o arquivo de dados com o offset encontrado
            FILE *file = fopen(pathData, "rb");
            if (!file) return notFound;

            struct Order order;
            fseek(file, atual->posicao, SEEK_SET);
            if (fread(&order, sizeof(struct Order), 1, file) == 1) {
                fclose(file);
                if (!order.del && order.idCliente == chave) { 
                    return order;
                }
            }
            fclose(file);
        }
        atual = atual->proximo;
    }

    return notFound;
}


struct BPlusNode *criaBPlusNode(bool folha) {
    struct BPlusNode *newNode = (struct BPlusNode *)calloc(1, sizeof(struct BPlusNode));
    if (!newNode) exit(1);
    newNode->folha = folha;
    return newNode;
}

struct BPlusTree *criaBPlusTree() {
    struct BPlusTree *tree = (struct BPlusTree *)malloc(sizeof(struct BPlusTree));
    if (!tree) exit(1);
    tree->raiz = criaBPlusNode(true);
    return tree;
}

void inserirBPlusTree(struct BPlusTree *tree, int64_t chave, long posicao) {
    struct BPlusNode *no = tree->raiz;
    
    if (no == NULL || no->numChaves >= MAX_CHAVES) {
         return; 
    }


    int i = no->numChaves;
    
    while (i > 0 && no->chaves[i - 1] > chave) {
        no->chaves[i] = no->chaves[i - 1];
        no->posicoes[i] = no->posicoes[i - 1];
        i--;
    }

    no->chaves[i] = chave;
    no->posicoes[i] = posicao;
    no->numChaves++;
}

void constroiIndexProductMemoria(char pathData[], struct BPlusTree *tree) {
    FILE *file = fopen(pathData, "rb");
    if (!file) {
        printf("Erro ao abrir arquivo de produtos.\n");
        return;
    }
    

    struct Header header;
    if (fread(&header, sizeof(struct Header), 1, file) != 1) {
        fclose(file);
        printf("Erro ao ler cabeçalho do arquivo de produtos.\n");
        return;
    }
    
    struct Products produto;
    long pos;

    clock_t inicio = clock();

    for (int i = 0; i < header.totalRegistros; i++) {
        

        pos = sizeof(struct Header) + (long)i * sizeof(struct Products);

        fseek(file, pos, SEEK_SET);

        if (fread(&produto, sizeof(struct Products), 1, file) != 1) continue;

        if (!produto.del) {
            inserirBPlusTree(tree, produto.idProduto, pos);
        }
    }
    
    clock_t fim = clock();
    double tempo_ms = medirTempo(inicio, fim);

    printf("Criacao do Índice B+ (idProduto) em memória concluída em: %.2f ms\n", tempo_ms);
    
    fclose(file);
}

struct Products buscaProductBPlus(char pathData[], struct BPlusTree *tree, int64_t chave) {

    struct Products notFound = {.idProduto = -1}; 
    
    if (tree == NULL || tree->raiz == NULL) {
        return notFound;
    }
    
    struct BPlusNode *no = tree->raiz;
    long posicao = -1;
    
    for (int i = 0; i < no->numChaves; i++) {
        if (no->chaves[i] == chave) {
            posicao = no->posicoes[i]; 
            break;
        }
    }
    
    if (posicao == -1) {
        return notFound;
    }
    
    FILE *file = fopen(pathData, "rb");
    if (file == NULL) {
        return notFound;
    }

    struct Products produto;
    
    fseek(file, posicao, SEEK_SET); 
    
    if (fread(&produto, sizeof(struct Products), 1, file) != 1) {
        fclose(file);
        return notFound;
    }

    fclose(file);

    if (!produto.del && produto.idProduto == chave) {
        // 
        return produto;
    }
    
    return notFound;
}



void carregarIndicesMemoria() {
    
    if (indiceProdMemoria != NULL) {
        freeBPlusTree(indiceProdMemoria);
    }
    if (indiceOrderMemoria != NULL) {
        freeHashTable(indiceOrderMemoria);
    }

    indiceProdMemoria = criaBPlusTree();
    indiceOrderMemoria = criaHashTable();
    
    printf("\n--- Carregando Índices em Memória ---\n");

    constroiIndexProductMemoria(pathProducts, indiceProdMemoria); 
    constroiIndexOrderMemoria(pathOrder, indiceOrderMemoria);
    printf("--------------------------------------\n");
}

#define NUM_REPETICOES 10000 
struct Order buscaSequencialOrder(char pathData[], int64_t chave) {
    struct Order notFound = {.idPedido = -1};
    FILE *file = fopen(pathData, "rb");

    if (file == NULL) return notFound;

    fseek(file, sizeof(struct Header), SEEK_SET);

    struct Order order;
    
    while (fread(&order, sizeof(struct Order), 1, file) == 1) {
        if (!order.del && order.idCliente == chave) {
            fclose(file);
            return order; 
        }
    }

    fclose(file);
    return notFound; 
} 
void testeComparativoBusca(char pathProducts[], char pathIndexProducts[], char pathOrders[], char pathIndexOrders[]) {
    
    int64_t prod_chaves[] = {
        1313550904313512734,                   
        1313550904313512734,     
        1313551664447226717,    
        1313551664447226717         
    }; 
 
    int64_t order_chaves[] = {
        1515915625207851155, 
        0, 
        1515915625048493557, 
        1515915625207630915
    };
    
    printf("\n##  Comparação de Desempenho (Tempo Médio de 10.000 Consultas)  ##\n");
    printf("----------------------------------------------------\n");
    printf("| Chave | Tipo de Índice | Tempo Médio (ms) | Resultado |\n");
    printf("|---|---|---|---|\n");

    for (int i = 0; i < 4; i++) {
        int64_t chave = prod_chaves[i];
        
        clock_t inicio_arq = clock();
        struct Products p_arq = {.idProduto = -1};
        for (int r = 0; r < NUM_REPETICOES; r++) { 
            p_arq = buscaProductIndice(pathProducts, pathIndexProducts, chave); 
        }
        clock_t fim_arq = clock();
        double tempo_arq = medirTempo(inicio_arq, fim_arq) / NUM_REPETICOES;
        printf("| %lld | Arquivo (Parcial) | %.4f | %s |\n", chave, tempo_arq, p_arq.idProduto != -1 ? "OK" : "N/E");

        clock_t inicio_mem = clock();
        struct Products p_mem = {.idProduto = -1};
        for (int r = 0; r < NUM_REPETICOES; r++) { 
            p_mem = buscaProductBPlus(pathProducts, indiceProdMemoria, chave); 
        }
        clock_t fim_mem = clock();
        double tempo_mem = medirTempo(inicio_mem, fim_mem) / NUM_REPETICOES;
        printf("| %lld | Memória (B+) | %.4f | %s |\n", chave, tempo_mem, p_mem.idProduto != -1 ? "OK" : "N/E");
    }

    printf("----------------------------------------------------\n");
    for (int i = 0; i < 4; i++) {
        int64_t chave = order_chaves[i]; 
        
        clock_t inicio_seq = clock();
        struct Order o_seq = {.idPedido = -1};
        for (int r = 0; r < NUM_REPETICOES; r++) { 
            o_seq = buscaSequencialOrder(pathOrders, chave); 
        }
        clock_t fim_seq = clock();
        double tempo_seq = medirTempo(inicio_seq, fim_seq) / NUM_REPETICOES;
        printf("| %lld | Arquivo (Sequencial) | %.4f | %s |\n", chave, tempo_seq, o_seq.idPedido != -1 ? "OK" : "N/E");
        
        clock_t inicio_mem = clock();
        struct Order o_mem = {.idPedido = -1};
        for (int r = 0; r < NUM_REPETICOES; r++) { 
            o_mem = buscaHashTable(pathOrders, indiceOrderMemoria, chave); 
        }
        clock_t fim_mem = clock();
        double tempo_mem = medirTempo(inicio_mem, fim_mem) / NUM_REPETICOES;
        printf("| %lld | Memória (Hash-idCliente) | %.4f | %s |\n", chave, tempo_mem, o_mem.idPedido != -1 ? "OK" : "N/E");
    }
    printf("----------------------------------------------------\n");
}


void menu() {
 int opcao;
 do {
  printf("\n--- MENU ---\n");
  printf("1. Listar produtos\n");
  printf("2. Listar pedidos\n");
  printf("3. Inserir produto (Atualiza B+ e Índice Arquivo)\n");
  printf("4. Remover produto (Atualiza B+ e Índice Arquivo)\n");
  printf("5. Inserir pedido (Atualiza Hash e Índice Arquivo)\n");
  printf("6. Remover pedido (Atualiza Hash e Índice Arquivo)\n");
  printf("7. Buscar produto por índice de ARQUIVO (idProduto)\n");
  printf("8. Buscar pedido por índice de ARQUIVO (idPedido)\n");
        printf("9. Buscar produto por índice em MEMÓRIA (B+ / idProduto)\n");
        printf("10. Buscar pedido por índice em MEMÓRIA (Hash / idCliente)\n");
  printf("11. Executar Teste Comparativo de Buscas (Req. 3.2)\n");
  printf("0. Sair\n");
  printf("Escolha uma opcao: ");
  scanf("%d", &opcao);

  // Variáveis para entradas e resultados
        int64_t id;
        struct Products p;
        struct Order o;
        clock_t inicio, fim;
        double tempo_ms;
        
  if (opcao == 1) {
   int qtd;
   printf("Quantos produtos deseja exibir? ");
   scanf("%d", &qtd);
   lerProducts(pathProducts, qtd);
  } else if (opcao == 2) {
   int qtd;
   printf("Quantos pedidos deseja exibir? ");
   scanf("%d", &qtd);
   lerOrder(pathOrder, qtd);
  } else if (opcao == 3) {
   printf("IDProduto: "); scanf("%lld", &p.idProduto);
   printf("Categoria: "); scanf("%s", p.categoria);
   printf("Preço: "); scanf("%f", &p.preco);
   p.del = false;
   
            inicio = clock();
   inserirProductIndice(pathProducts, pathIndexProductId, p);
            carregarIndicesMemoria(); 
            fim = clock();
            printf("Inserção + Atualização de Índices (Arquivo e Memória): %.2f ms\n", medirTempo(inicio, fim));

  } else if (opcao == 4) {
   printf("IDProduto a remover: ");
   scanf("%lld", &id);
            inicio = clock();
   removerProductIndice(pathProducts, pathIndexProductId, id);
            carregarIndicesMemoria();
            fim = clock();
            printf("Remoção + Atualização de Índices (Arquivo e Memória): %.2f ms\n", medirTempo(inicio, fim));
            
  } else if (opcao == 5) {
   printf("IDPedido: "); scanf("%lld", &o.idPedido);
   printf("IDCliente: "); scanf("%lld", &o.idCliente);
   printf("Data: "); scanf("%s", o.date);
   o.del = false;

            inicio = clock();
   inserirOrderIndice(pathOrder, pathIndexOrderId, o);
            carregarIndicesMemoria(); 
            fim = clock();
            printf("Inserção + Atualização de Índices (Arquivo e Memória): %.2f ms\n", medirTempo(inicio, fim));

  } else if (opcao == 6) {
   printf("IDPedido a remover: ");
   scanf("%lld", &id);

            inicio = clock();
   removerOrderIndice(pathOrder, pathIndexOrderId, id);
            carregarIndicesMemoria();
            fim = clock();
            printf("Remoção + Atualização de Índices (Arquivo e Memória): %.2f ms\n", medirTempo(inicio, fim));

  } else if (opcao == 7) {
   printf("IDProduto para buscar (Index Arquivo): ");
   scanf("%lld", &id);
            inicio = clock();
   p = buscaProductIndice(pathProducts, pathIndexProductId, id);
            fim = clock();
            if (p.idProduto != -1) {
                printf("ENCONTRADO (Index Arquivo): IDProduto: %lld | Categoria: %s | Preço: %.2f\n", p.idProduto, p.categoria, p.preco);
            } else {
                printf("Produto com ID %lld não encontrado (Index Arquivo).\n", id);
            }
            printf("Tempo de Busca (Index Arquivo): %.4f ms\n", medirTempo(inicio, fim));

  } else if (opcao == 8) {
   printf("IDPedido para buscar (Index Arquivo): ");
   scanf("%lld", &id);
            inicio = clock();
   o = buscaOrderIndice(pathOrder, pathIndexOrderId, id);
            fim = clock();
            if (o.idPedido != -1) {
                printf("ENCONTRADO (Index Arquivo): Data: %s | IDCliente: %lld | IDPedido: %lld\n", o.date, o.idCliente, o.idPedido);
            } else {
                printf("Pedido com ID %lld não encontrado (Index Arquivo).\n", id);
            }
            printf("Tempo de Busca (Index Arquivo): %.4f ms\n", medirTempo(inicio, fim));

        } else if (opcao == 9) { 
            printf("IDProduto para buscar (B+ Memória): ");
   scanf("%lld", &id);
            inicio = clock();
   p = buscaProductBPlus(pathProducts, indiceProdMemoria, id);
            fim = clock();
            if (p.idProduto != -1) {
                printf("ENCONTRADO (B+ Memória): IDProduto: %lld | Categoria: %s | Preço: %.2f\n", p.idProduto, p.categoria, p.preco);
            } else {
                printf("Produto com ID %lld não encontrado (B+ Memória).\n", id);
            }
            printf("Tempo de Busca (B+ Memória): %.4f ms\n", medirTempo(inicio, fim));
        
        } else if (opcao == 10) { 
            printf("IDCliente para buscar (Hash Memória): ");
   scanf("%lld", &id);
            inicio = clock();
   o = buscaHashTable(pathOrder, indiceOrderMemoria, id);
            fim = clock();
            if (o.idPedido != -1) {
                printf("ENCONTRADO (Hash Memória): Data: %s | IDCliente: %lld | IDPedido: %lld\n", o.date, o.idCliente, o.idPedido);
            } else {
                printf("Pedido com ID Cliente %lld não encontrado (Hash Memória).\n", id);
            }
            printf("Tempo de Busca (Hash Memória): %.4f ms\n", medirTempo(inicio, fim));
        
        } else if (opcao == 11) { 
            testeComparativoBusca(pathProducts, pathIndexProductId, pathOrder, pathIndexOrderId);
        }

 } while (opcao != 0);
}


void freeHashEntry(struct HashEntry *entry) {
    struct HashEntry *tmp;
    while (entry != NULL) {
        tmp = entry;
        entry = entry->proximo;
        free(tmp);
    }
}

void freeHashTable(struct HashTable *hashTable) {
    if (hashTable == NULL) return;
    for (int i = 0; i < TAMANHO_HASH; i++) {
        freeHashEntry(hashTable->tabela[i]);
    }
    free(hashTable);
}

void freeBPlusTree(struct BPlusTree *tree) {
    if (tree == NULL) return;
    if (tree->raiz != NULL) {
        free(tree->raiz); 
    }
    free(tree);
}

int main() {

    verificarOuCriarArquivos();
    verificarOuCriarIndices(); 

    carregarIndicesMemoria(); 

    menu();
    
    freeBPlusTree(indiceProdMemoria);
 freeHashTable(indiceOrderMemoria);

    return 0;
}



