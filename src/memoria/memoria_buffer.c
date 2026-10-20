/*
 * Paradigma de memória compartilhada: buffer circular sincronizado.
 * Responsável: PESSOA B
 *
 * Escopo:
 *   - sem_operar: wrapper de semop
 *   - buffer_inserir / buffer_retirar com P/V em MUTEX, VAZIOS e CHEIOS
 *   - modo não bloqueante (IPC_NOWAIT) para "consome ... 2"
 *   - EIDRM/EINVAL/EINTR retornam -1 (é assim que os filhos saem no shutdown)
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "memoria_interno.h"

int sem_operar(int id_sem, unsigned short num, short delta, short flags)
{
    /*
     * TODO (B): struct sembuf op = { num, delta, flags }; semop(id_sem, &op, 1)
     *   Retorno: 0 ok; 1 se IPC_NOWAIT e errno == EAGAIN; -1 nos demais erros.
     *   Só chamar perror quando o erro não for esperado (EIDRM/EINVAL no shutdown).
     */
    (void)id_sem;
    (void)num;
    (void)delta;
    (void)flags;
    return -1;
}

int buffer_inserir(struct contexto_memoria *ctx, int indice, const struct item *item)
{
    /*
     * TODO (B):
     *   P(VAZIOS) -> P(MUTEX) -> itens[fim] = *item;
     *   fim = (fim + 1) % CAPACIDADE_BUFFER -> V(MUTEX) -> V(CHEIOS)
     */
    (void)ctx;
    (void)indice;
    (void)item;
    return -1;
}

int buffer_retirar(struct contexto_memoria *ctx, int indice, struct item *item, int bloqueante)
{
    /*
     * TODO (B):
     *   P(CHEIOS) (com IPC_NOWAIT se !bloqueante; vazio -> retornar 1)
     *   -> P(MUTEX) -> *item = itens[inicio];
     *   inicio = (inicio + 1) % CAPACIDADE_BUFFER -> V(MUTEX) -> V(VAZIOS)
     */
    (void)ctx;
    (void)indice;
    (void)item;
    (void)bloqueante;
    return -1;
}
