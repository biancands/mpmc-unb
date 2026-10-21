/*
 * Paradigma de memória compartilhada: ciclo de vida.
 * Responsável: PESSOA A
 *
 * Escopo:
 *   - executar_memoria: prompt, validação dos comandos e fork
 *     (reaproveitar split, converter_para_inteiro e validações de mensagens.c)
 *   - criação e remoção de shm e semáforos (System V)
 *   - tabela de processos (reservar_slot), SIGCHLD sem zumbi
 *   - shutdown / EOF na ordem combinada (ver executar_memoria)
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "memoria.h"
#include "memoria_interno.h"

int executar_memoria(int numero_produtores)
{
    struct contexto_memoria ctx;

    if (criar_recursos(&ctx, numero_produtores) != 0) {
        return 1;
    }

    /*
     * TODO (A): laço do prompt, igual ao de mensagens.c
     *   - "produz p n ack &"  -> slot = reservar_slot(&ctx, 0); fork;
     *       filho: r = produzir_memoria(&ctx, p, n, ack, slot);
     *              registrar_fim(&ctx, slot, 0); exit(r);
     *   - "consome p n ack &" -> slot = reservar_slot(&ctx, 1); fork;
     *       filho: r = consumir_memoria(&ctx, p, n, ack, slot); exit(r);
     *       (consumir_memoria chama registrar_fim com a contagem)
     *   - pai, após o fork: ctx.area->processos[slot].pid = pid;
     *   - slot == -1: recusar o comando com mensagem de erro, sem fork.
     *   - SIGCHLD: waitpid(-1, NULL, WNOHANG) em laço (sem zumbi).
     */
    fprintf(stderr, "Prompt da memória compartilhada ainda não implementado.\n");

    /*
     * Ordem obrigatória de encerramento (shutdown e EOF):
     *   1. remover_semaforos  -> filhos bloqueados recebem EIDRM e saem
     *   2. waitpid em todos os filhos (restaurar SIGCHLD para SIG_DFL antes)
     *   3. imprimir_relatorio -> lê a tabela na shm
     *   4. tempo total do mpmc
     *   5. liberar_memoria    -> shmdt + IPC_RMID
     */
    remover_semaforos(&ctx);
    imprimir_relatorio(&ctx);
    liberar_memoria(&ctx);
    return 1;
}

int criar_recursos(struct contexto_memoria *ctx, int numero_produtores)
{
    /*
     * TODO (A):
     *   - shmget(IPC_PRIVATE, sizeof(struct area_compartilhada)
     *            + numero_produtores * sizeof(struct buffer_circular), ...)
     *     cuidado com overflow nesse cálculo para p grande
     *   - shmat, zerar a área, area->numero_buffers = numero_produtores
     *   - id_sem_buffers: semget com 3 * numero_produtores semáforos
     *     (tratar EINVAL/ENOSPC: limite SEMMSL do sistema)
     *     SETALL: MUTEX = 1, VAZIOS = CAPACIDADE_BUFFER, CHEIOS = 0
     *   - id_sem_ack: semget com MAX_PROCESSOS semáforos, todos em 0
     *   - em qualquer falha, desfazer o que já foi criado
     */
    ctx->numero_produtores = numero_produtores;
    ctx->id_shm = -1;
    ctx->id_sem_buffers = -1;
    ctx->id_sem_ack = -1;
    ctx->area = NULL;
    fprintf(stderr, "criar_recursos ainda não implementado.\n");
    return -1;
}

void remover_semaforos(struct contexto_memoria *ctx)
{
    /* TODO (A): semctl(..., 0, IPC_RMID) nos dois conjuntos, se != -1. */
    (void)ctx;
}

void liberar_memoria(struct contexto_memoria *ctx)
{
    /* TODO (A): shmdt(ctx->area) e shmctl(ctx->id_shm, IPC_RMID, NULL), se válidos. */
    (void)ctx;
}

int reservar_slot(struct contexto_memoria *ctx, int eh_consumidor)
{
    /*
     * TODO (A): primeiro processos[i] com em_uso == 0;
     *   preencher em_uso, eh_consumidor, tempo_inicio; retornar i.
     *   Chamado só pelo pai, antes do fork.
     *   Tabela cheia: retornar -1.
     */
    (void)ctx;
    (void)eh_consumidor;
    return -1;
}
