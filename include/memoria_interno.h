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

/* O enunciado exige 2048 bytes por memória compartilhada (uma por produtor). */
#define TAMANHO_SHM 2048
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

/* Ocupa uma shm inteira de TAMANHO_SHM bytes (uma por produtor). */
struct buffer_circular {
    int inicio;             /* próxima posição a ser lida */
    int fim;                /* próxima posição livre */
    unsigned long consumidas_total;   /* total consumido deste buffer, protegido pelo MUTEX */
    struct item itens[CAPACIDADE_BUFFER];
};

/* Tabela do mpmc (memória privada do pai, não fica em shm). */
struct registro_processo {
    pid_t pid;
    int em_uso;
    int eh_consumidor;
    time_t tempo_inicio;
};

/* Preenchido pelo pai quando recolhe um filho (waitpid). */
struct fim_processo {
    pid_t pid;
    time_t tempo_fim;
};

struct contexto_memoria {
    int numero_produtores;
    int *ids_shm;                       /* numero_produtores shms, uma por buffer */
    struct buffer_circular **buffers;   /* buffers[i] = shm do produtor i + 1 */
    int id_sem_buffers;     /* 3 * numero_produtores semáforos */
    int id_sem_ack;         /* MAX_PROCESSOS semáforos, um por slot */
    struct registro_processo processos[MAX_PROCESSOS];
    struct fim_processo fins[MAX_PROCESSOS];
    int quantidade_fins;
};

/* ===== Pessoa A: ciclo de vida (memoria.c) ===== */
int  criar_recursos(struct contexto_memoria *ctx, int numero_produtores);
void remover_semaforos(struct contexto_memoria *ctx);  /* acorda bloqueados com EIDRM */
void liberar_memoria(struct contexto_memoria *ctx);    /* shmdt + IPC_RMID em todas as shms */
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
void registrar_fim(struct contexto_memoria *ctx, pid_t pid);   /* chamada pelo pai ao recolher o filho */
void imprimir_relatorio(const struct contexto_memoria *ctx);

#endif