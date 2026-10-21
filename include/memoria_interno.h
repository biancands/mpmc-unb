#ifndef MEMORIA_INTERNO_H
#define MEMORIA_INTERNO_H

/*
 * Contrato interno do paradigma de memória compartilhada.
 * Combinado antes da implementação para que os três responsáveis
 * trabalhem em paralelo sem depender do código uns dos outros.
 *
 * Mecanismos usados: shmget/shmat/shmdt/shmctl e semget/semop/semctl
 * (System V). Sem pthread e sem <semaphore.h>.
 */

#include <sys/types.h>
#include <time.h>

/* AJUSTAR conforme o enunciado (suposição: buffer limitado por fila). */
#define CAPACIDADE_BUFFER 64
/* Máximo de processos filhos registrados durante uma execução. */
#define MAX_PROCESSOS 256

/* Índices no conjunto de semáforos dos buffers: 3 por buffer. */
#define SEM_MUTEX(i)  ((unsigned short)(3 * (i)))
#define SEM_VAZIOS(i) ((unsigned short)(3 * (i) + 1))
#define SEM_CHEIOS(i) ((unsigned short)(3 * (i) + 2))

/* Equivalente à struct mensagem do paradigma de mensagens. */
struct item {
    pid_t pid;
    unsigned short seq;
    unsigned char requer_confirmacao;
    int slot_produtor;      /* índice em processos[], usado no ack */
    time_t tempo_envio;
};

struct buffer_circular {
    int inicio;             /* próxima posição a ser lida */
    int fim;                /* próxima posição livre */
    struct item itens[CAPACIDADE_BUFFER];
};

/* Equivalente à struct resultado_processo + node_pid. */
struct registro_processo {
    pid_t pid;
    int em_uso;
    int eh_consumidor;
    int finalizado;
    time_t tempo_inicio;
    time_t tempo_fim;
    unsigned short mensagens_consumidas;
};

/* Layout do segmento de memória compartilhada. */
struct area_compartilhada {
    int numero_buffers;
    struct registro_processo processos[MAX_PROCESSOS];
    struct buffer_circular buffers[];   /* numero_buffers elementos */
};

struct contexto_memoria {
    int numero_produtores;
    int id_shm;
    int id_sem_buffers;     /* 3 * numero_produtores semáforos */
    int id_sem_ack;         /* MAX_PROCESSOS semáforos, um por slot */
    struct area_compartilhada *area;
};

/* ===== Pessoa A: ciclo de vida (memoria.c) ===== */
int  criar_recursos(struct contexto_memoria *ctx, int numero_produtores);
void remover_semaforos(struct contexto_memoria *ctx);  /* acorda bloqueados com EIDRM */
void liberar_memoria(struct contexto_memoria *ctx);    /* shmdt + IPC_RMID */
int  reservar_slot(struct contexto_memoria *ctx, int eh_consumidor);  /* -1 se cheio */

/* ===== Pessoa B: buffer sincronizado (memoria_buffer.c) ===== */
/* Retorno: 0 ok, -1 erro ou recursos removidos (shutdown). */
int sem_operar(int id_sem, unsigned short num, short delta, short flags);
int buffer_inserir(struct contexto_memoria *ctx, int indice, const struct item *item);
/* Retorno: 0 ok, 1 buffer vazio (só quando bloqueante == 0), -1 erro. */
int buffer_retirar(struct contexto_memoria *ctx, int indice, struct item *item, int bloqueante);

/* ===== Pessoa C: operações e resultados (memoria_ops.c) ===== */
int  produzir_memoria(struct contexto_memoria *ctx, long p, long n_msgs, long ack, int slot);
int  consumir_memoria(struct contexto_memoria *ctx, long p, long n_msgs, long ack, int slot);
void registrar_fim(struct contexto_memoria *ctx, int slot, unsigned short consumidas);
void imprimir_relatorio(const struct contexto_memoria *ctx);

#endif
