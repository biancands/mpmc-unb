/*
 * Paradigma de memória compartilhada: produção, consumo, ack e resultados.
 * Responsável: PESSOA C
 *
 * Escopo:
 *   - produzir_memoria (fila direta e striped com p = 0)
 *   - consumir_memoria (ack = 2 -> não bloqueante, como em mensagens.c)
 *   - confirmação (ack = 1 no produz) via id_sem_ack
 *   - registrar_fim / imprimir_relatorio
 *   - testes de integração: deadlock, `ps` (zumbis), `ipcs` (recursos órfãos)
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "memoria_interno.h"

int produzir_memoria(struct contexto_memoria *ctx, long p, long n_msgs, long ack, int slot)
{
    /*
     * TODO (C): para seq = 1..n_msgs:
     *   indice = (p == 0) ? (seq - 1) % numero_produtores : p - 1;
     *   item = { getpid(), seq, ack == 1, slot, time(NULL) };
     *   buffer_inserir(ctx, indice, &item)  (-1 -> retornar 1)
     * Se ack == 1: sem_operar(id_sem_ack, slot, -1, 0) n_msgs vezes.
     * Mensagem final igual à de mensagens.c.
     */
    (void)ctx;
    (void)p;
    (void)n_msgs;
    (void)ack;
    (void)slot;
    return 1;
}

int consumir_memoria(struct contexto_memoria *ctx, long p, long n_msgs, long ack, int slot)
{
    /*
     * TODO (C): retirar n_msgs de buffers[p - 1] (bloqueante = (ack != 2));
     *   retorno 1 de buffer_retirar -> mensagem "solicitadas X, consumidas Y";
     *   imprimir cada item consumido;
     *   se item.requer_confirmacao: sem_operar(id_sem_ack, item.slot_produtor, +1, 0).
     * Sempre chamar registrar_fim(ctx, slot, consumidas) antes de retornar,
     * inclusive em erro (o relatório precisa da contagem parcial).
     */
    (void)ctx;
    (void)p;
    (void)n_msgs;
    (void)ack;
    (void)slot;
    return 1;
}

void registrar_fim(struct contexto_memoria *ctx, int slot, unsigned short consumidas)
{
    /*
     * TODO (C): processos[slot].mensagens_consumidas = consumidas;
     *   tempo_fim = time(NULL); finalizado = 1.
     *   Cada slot é escrito por um único filho: não precisa de mutex.
     */
    (void)ctx;
    (void)slot;
    (void)consumidas;
}

void imprimir_relatorio(const struct contexto_memoria *ctx)
{
    /*
     * TODO (C): para cada slot em_uso e finalizado:
     *   "Relatório recebido: pid=%d, consumidas=%hu, tempo=%.0f segundos."
     *   e ao final "Total de mensagens consumidas: %lu" (mesmo formato de mensagens.c).
     *   ctx->area pode ser NULL se criar_recursos falhou.
     */
    (void)ctx;
}
