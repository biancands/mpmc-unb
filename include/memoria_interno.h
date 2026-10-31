#ifndef MEMORIA_INTERNO_H
#define MEMORIA_INTERNO_H

#include <sys/types.h>
#include <time.h>

#define TAMANHO_SHM 2048
#define CAPACIDADE_BUFFER 64
#define MAX_PROCESSOS 256

#define SEM_MUTEX(i)  ((unsigned short)(3 * (i)))
#define SEM_VAZIOS(i) ((unsigned short)(3 * (i) + 1))
#define SEM_CHEIOS(i) ((unsigned short)(3 * (i) + 2))

struct item {
    pid_t pid;
    unsigned short seq;
    unsigned char requer_confirmacao;
    int slot_produtor;
    time_t tempo_envio;
};

struct buffer_circular {
    int inicio;
    int fim;
    unsigned long consumidas_total;
    struct item itens[CAPACIDADE_BUFFER];
};

struct registro_processo {
    pid_t pid;
    int em_uso;
    int eh_consumidor;
    time_t tempo_inicio;
};

struct fim_processo {
    pid_t pid;
    time_t tempo_fim;
};

struct contexto_memoria {
    int numero_produtores;

    int *ids_shm;
    struct buffer_circular **buffers;

    int id_sem_buffers;
    int id_sem_ack;

    struct registro_processo processos[MAX_PROCESSOS];
    struct fim_processo fins[MAX_PROCESSOS];
    int quantidade_fins;
};

/* Ciclo de vida */
int criar_recursos(struct contexto_memoria *ctx, int numero_produtores);
void remover_semaforos(struct contexto_memoria *ctx);
void liberar_memoria(struct contexto_memoria *ctx);
int reservar_slot(struct contexto_memoria *ctx, int eh_consumidor);

/* Buffer */
int sem_operar(int id_sem, unsigned short num, short delta, short flags);

int buffer_inserir(struct contexto_memoria *ctx,
                   int indice,
                   const struct item *item);

int buffer_retirar(struct contexto_memoria *ctx,
                   int indice,
                   struct item *item,
                   int bloqueante);

/* Operações */
int produzir_memoria(struct contexto_memoria *ctx,
                     long p,
                     long n_msgs,
                     long ack,
                     int slot);

int consumir_memoria(struct contexto_memoria *ctx,
                     long p,
                     long n_msgs,
                     long ack,
                     int slot);

void registrar_fim(struct contexto_memoria *ctx, pid_t pid);

void imprimir_relatorio(const struct contexto_memoria *ctx);

#endif