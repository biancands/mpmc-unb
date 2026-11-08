#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "memoria_interno.h"

int sem_operar(int id_sem, unsigned short num, short delta, short flags)
{
    struct sembuf operacao;

    operacao.sem_num = num;
    operacao.sem_op = delta;
    operacao.sem_flg = flags;

    if (semop(id_sem, &operacao, 1) == -1) {
        if ((flags & IPC_NOWAIT) && errno == EAGAIN) {
            return 1;
        }
        if (errno != EIDRM && errno != EINVAL && errno != EINTR) {
            perror("semop");
        }
        return -1;
    }

    return 0;
}

int buffer_inserir(struct contexto_memoria *ctx, int indice, const struct item *item)
{
    struct buffer_circular *buffer = ctx->buffers[indice];

    if (sem_operar(ctx->id_sem_buffers, SEM_VAZIOS(indice), -1, 0) != 0) {
        return -1;
    }
    if (sem_operar(ctx->id_sem_buffers, SEM_MUTEX(indice), -1, 0) != 0) {
        return -1;
    }

    buffer->itens[buffer->fim] = *item;
    buffer->fim = (buffer->fim + 1) % CAPACIDADE_BUFFER;

    if (sem_operar(ctx->id_sem_buffers, SEM_MUTEX(indice), 1, 0) != 0) {
        return -1;
    }
    if (sem_operar(ctx->id_sem_buffers, SEM_CHEIOS(indice), 1, 0) != 0) {
        return -1;
    }

    return 0;
}

int buffer_retirar(struct contexto_memoria *ctx, int indice, struct item *item, int bloqueante)
{
    struct buffer_circular *buffer = ctx->buffers[indice];
    short flags = bloqueante ? 0 : IPC_NOWAIT;

    int resultado = sem_operar(ctx->id_sem_buffers, SEM_CHEIOS(indice), -1, flags);
    if (resultado != 0) {
        return resultado;
    }
    if (sem_operar(ctx->id_sem_buffers, SEM_MUTEX(indice), -1, 0) != 0) {
        return -1;
    }

    *item = buffer->itens[buffer->inicio];
    buffer->inicio = (buffer->inicio + 1) % CAPACIDADE_BUFFER;
    buffer->consumidas_total++;

    if (sem_operar(ctx->id_sem_buffers, SEM_MUTEX(indice), 1, 0) != 0) {
        return -1;
    }
    if (sem_operar(ctx->id_sem_buffers, SEM_VAZIOS(indice), 1, 0) != 0) {
        return -1;
    }

    return 0;
}
