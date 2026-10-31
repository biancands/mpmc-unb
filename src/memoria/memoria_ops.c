#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "memoria_interno.h"

int produzir_memoria(struct contexto_memoria *ctx, long p, long n_msgs, long ack, int slot)
{
    struct item item;
    item.pid = getpid();
    item.requer_confirmacao = (ack == 1) ? 1 : 0;
    item.slot_produtor = slot;

    for (int sequencia = 1; sequencia <= n_msgs; sequencia++) {
        int indice_buffer;
        if (p == 0) {
            indice_buffer = (sequencia - 1) % ctx->numero_produtores;
        } else {
            indice_buffer = p - 1;
        }

        item.seq = sequencia;
        item.tempo_envio = time(NULL);

        if (buffer_inserir(ctx, indice_buffer, &item) != 0) {
            return 1;
        }
    }

    if (ack == 1) {
        for (int confirmacoes = 0; confirmacoes < n_msgs; confirmacoes++) {
            if (sem_operar(ctx->id_sem_ack, slot, -1, 0) != 0) {
                return 1;
            }
        }
    }

    if (p != 0) {
        printf("%ld mensagens produzidas no buffer %ld\n", n_msgs, p);
    } else {
        printf("%ld mensagens produzidas no padrão striped\n", n_msgs);
    }

    return 0;
}

int consumir_memoria(struct contexto_memoria *ctx, long p, long n_msgs, long ack, int slot)
{
    struct item item;
    int bloqueante = (ack != 2);
    (void)slot;

    for (int recebidas = 0; recebidas < n_msgs; recebidas++) {
        int resultado = buffer_retirar(ctx, p - 1, &item, bloqueante);

        if (resultado == 1) {
            fprintf(stderr, "Erro: foram solicitadas %ld mensagens, mas apenas %d foram consumidas.\n", n_msgs, recebidas);
            return 1;
        }
        if (resultado != 0) {
            return 1;
        }

        printf("Mensagem consumida: pid=%d, seq=%d, tempo=%lld\n", (int)item.pid, item.seq, (long long)item.tempo_envio);

        if (item.requer_confirmacao == 1) {
            if (sem_operar(ctx->id_sem_ack, item.slot_produtor, 1, 0) != 0) {
                return 1;
            }
        }
    }

    printf("%ld mensagens consumidas do buffer %ld\n", n_msgs, p);
    return 0;
}

void registrar_fim(struct contexto_memoria *ctx, pid_t pid)
{
    for (int i = 0; i < ctx->quantidade_fins; i++) {
        if (ctx->fins[i].pid == pid) {
            return;
        }
    }

    if (ctx->quantidade_fins < MAX_PROCESSOS) {
        ctx->fins[ctx->quantidade_fins].pid = pid;
        ctx->fins[ctx->quantidade_fins].tempo_fim = time(NULL);
        ctx->quantidade_fins++;
    }
}

void imprimir_relatorio(const struct contexto_memoria *ctx)
{
    unsigned long total_mensagens_consumidas = 0;

    for (int i = 0; i < MAX_PROCESSOS; i++) {
        const struct registro_processo *processo = &ctx->processos[i];
        time_t tempo_fim = time(NULL);

        if (!processo->em_uso) {
            continue;
        }

        for (int j = 0; j < ctx->quantidade_fins; j++) {
            if (ctx->fins[j].pid == processo->pid) {
                tempo_fim = ctx->fins[j].tempo_fim;
            }
        }

        printf("Relatório recebido: pid=%d, tempo=%.0f segundos.\n", (int)processo->pid, difftime(tempo_fim, processo->tempo_inicio));
    }

    if (ctx->buffers != NULL) {
        for (int i = 0; i < ctx->numero_produtores; i++) {
            printf("Mensagens consumidas do buffer %d: %lu\n", i + 1, ctx->buffers[i]->consumidas_total);
            total_mensagens_consumidas += ctx->buffers[i]->consumidas_total;
        }
    }

    printf("Total de mensagens consumidas: %lu\n", total_mensagens_consumidas);
}