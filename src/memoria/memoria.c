#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
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

#define TAMANHO_COMANDO 256
#define MAX_MENSAGENS 1024

_Static_assert(sizeof(struct buffer_circular) <= TAMANHO_SHM, "buffer_circular nao cabe em TAMANHO_SHM");

/* No Linux a union semun precisa ser declarada pelo programa (man 2 semctl) */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* O handler de SIGCHLD não recebe parâmetros, então guarda o contexto aqui */
static struct contexto_memoria *ctx_sinal = NULL;

static char **split(const char *str, const char *delim);
static int converter_para_inteiro(const char *str, int *sucesso);
static void liberar_tokens(char **tokens);
static void recolher_filhos(int sig);
static int instalar_handler_sigchld(void);
static void encerrar(struct contexto_memoria *ctx, time_t tempo_inicio_mpmc);
static int criar_semaforos(int quantidade, int eh_buffers);

int executar_memoria(int numero_produtores) {
    time_t tempo_inicio_mpmc = time(NULL);
    struct contexto_memoria ctx;

    if (criar_recursos(&ctx, numero_produtores) != 0) {
        return 1;
    }

    ctx_sinal = &ctx;
    if (instalar_handler_sigchld() != 0) {
        ctx_sinal = NULL;
        remover_semaforos(&ctx);
        liberar_memoria(&ctx);
        return 1;
    }

    printf("Executando o paradigma de memória compartilhada com %d produtores...\n", numero_produtores);

    while (1) {
        char buffer[TAMANHO_COMANDO];

        printf(">> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            int resultado = ferror(stdin);
            if (resultado != 0) {
                perror("fgets");
            }
            encerrar(&ctx, tempo_inicio_mpmc);
            return resultado != 0;
        }

        if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
            int caractere;
            while ((caractere = fgetc(stdin)) != '\n' && caractere != EOF) { }

            fprintf(stderr, "Erro: comando excede o tamanho máximo permitido.\n");
            continue;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0';

        char **tokens = split(buffer, " \t\n");
        if (tokens == NULL) {
            fprintf(stderr, "Erro ao dividir a mensagem em tokens.\n");
            continue;
        }

        if (tokens[0] == NULL) {
            liberar_tokens(tokens);
            continue;
        }

        printf("Mensagem recebida: %s\n", buffer);

        int is_shutdown = strcmp(tokens[0], "shutdown") == 0;
        int is_produz = strcmp(tokens[0], "produz") == 0;
        int is_consome = strcmp(tokens[0], "consome") == 0;

        int num_tokens = 0;
        while (tokens[num_tokens] != NULL) {
            num_tokens++;
        }

        if (is_shutdown) {
            liberar_tokens(tokens);
            if (num_tokens != 1) {
                fprintf(stderr, "Mensagem inválida. O comando 'shutdown' não aceita argumentos.\n");
                continue;
            }
            encerrar(&ctx, tempo_inicio_mpmc);
            return 0;
        }

        if (!is_produz && !is_consome) {
            fprintf(stderr, "Mensagem inválida. Esperado 'produz' ou 'consome' como primeiro token.\n");
            liberar_tokens(tokens);
            continue;
        }

        if (num_tokens != 5) {
            fprintf(stderr, "Mensagem inválida. Esperado exatamente 5 tokens.\n");
            liberar_tokens(tokens);
            continue;
        }

        if (strcmp(tokens[4], "&") != 0) {
            fprintf(stderr, "Mensagem inválida. O último token deve ser '&'.\n");
            liberar_tokens(tokens);
            continue;
        }

        long valores[3];
        int erro_conversao = 0;
        for (int i = 1; i <= 3; i++) {
            int sucesso;
            int valor = converter_para_inteiro(tokens[i], &sucesso);
            if (!sucesso) {
                fprintf(stderr, "Erro ao converter o token %d para inteiro.\n", i);
                fprintf(stderr, "Token: %s\n", tokens[i]);
                erro_conversao = 1;
                break;
            }
            valores[i - 1] = valor;
        }
        liberar_tokens(tokens);

        if (erro_conversao) {
            continue;
        }

        long p = valores[0];
        long n_msgs = valores[1];
        long ack = valores[2];

        if (ack != 1 && ack != 2) {
            fprintf(stderr, "Ack deve ser 1 ou 2.\n");
            continue;
        }

        if (n_msgs <= 0 || n_msgs > MAX_MENSAGENS) {
            fprintf(stderr, "O número de mensagens deve estar entre 1 e %d\n", MAX_MENSAGENS);
            continue;
        }

        if (is_produz) {
            if (p < 0 || p > numero_produtores) {
                fprintf(stderr, "Erro: para 'produz', p deve estar entre 0 e %d.\n", numero_produtores);
                continue;
            }
            printf("Comando de produção validado.\n");
        } else {
            if (p < 1 || p > numero_produtores) {
                fprintf(stderr, "Erro: para 'consome', p deve estar entre 1 e %d.\n", numero_produtores);
                continue;
            }
            printf("Comando de consumo validado.\n");
        }

        int slot = reservar_slot(&ctx, is_consome);
        if (slot == -1) {
            fprintf(stderr, "Erro: limite de %d processos atingido. Comando recusado.\n", MAX_PROCESSOS);
            continue;
        }

        printf("Comando validado com sucesso.\n");
        printf("Valores extraídos: p = %ld, n_msgs = %ld, ack = %ld\n", p, n_msgs, ack);

        fflush(NULL);
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            ctx.processos[slot].em_uso = 0;
            continue;
        }

        if (pid == 0) {
            int resultado;
            if (is_produz) {
                resultado = produzir_memoria(&ctx, p, n_msgs, ack, slot);
                if (resultado == 0) {
                    printf("Produção concluída com sucesso.\n");
                }
            } else {
                resultado = consumir_memoria(&ctx, p, n_msgs, ack, slot);
                if (resultado == 0) {
                    printf("Consumo concluído com sucesso.\n");
                }
            }
            exit(resultado);
        }

        ctx.processos[slot].pid = pid;
    }
}

int criar_recursos(struct contexto_memoria *ctx, int numero_produtores) {
    ctx->numero_produtores = numero_produtores;
    ctx->ids_shm = NULL;
    ctx->buffers = NULL;
    ctx->id_sem_buffers = -1;
    ctx->id_sem_ack = -1;
    ctx->quantidade_fins = 0;
    memset(ctx->processos, 0, sizeof(ctx->processos));
    memset(ctx->fins, 0, sizeof(ctx->fins));

    /* SEM_MUTEX/VAZIOS/CHEIOS usam unsigned short como índice */
    if (numero_produtores > USHRT_MAX / 3) {
        fprintf(stderr, "Erro: quantidade de produtores excede o limite suportado.\n");
        return -1;
    }

    ctx->ids_shm = malloc((size_t)numero_produtores * sizeof(int));
    ctx->buffers = calloc((size_t)numero_produtores, sizeof(struct buffer_circular *));
    if (ctx->ids_shm == NULL || ctx->buffers == NULL) {
        perror("malloc");
        free(ctx->ids_shm);
        free(ctx->buffers);
        ctx->ids_shm = NULL;
        ctx->buffers = NULL;
        return -1;
    }

    /* -1 marca o que ainda não foi criado, para liberar_memoria saber parar */
    for (int i = 0; i < numero_produtores; i++) {
        ctx->ids_shm[i] = -1;
    }

    for (int i = 0; i < numero_produtores; i++) {
        ctx->ids_shm[i] = shmget(IPC_PRIVATE, TAMANHO_SHM, IPC_CREAT | 0600);
        if (ctx->ids_shm[i] == -1) {
            if (errno == ENOSPC) {
                fprintf(stderr, "Erro: limite de segmentos de memória compartilhada do sistema atingido (%d criados).\n", i);
            } else {
                perror("shmget");
            }
            liberar_memoria(ctx);
            return -1;
        }

        void *endereco = shmat(ctx->ids_shm[i], NULL, 0);
        if (endereco == (void *)-1) {
            perror("shmat");
            liberar_memoria(ctx);
            return -1;
        }

        ctx->buffers[i] = endereco;
        memset(ctx->buffers[i], 0, TAMANHO_SHM);
    }

    ctx->id_sem_buffers = criar_semaforos(3 * numero_produtores, 1);
    if (ctx->id_sem_buffers == -1) {
        liberar_memoria(ctx);
        return -1;
    }

    ctx->id_sem_ack = criar_semaforos(MAX_PROCESSOS, 0);
    if (ctx->id_sem_ack == -1) {
        remover_semaforos(ctx);
        liberar_memoria(ctx);
        return -1;
    }

    return 0;
}

void remover_semaforos(struct contexto_memoria *ctx) {
    if (ctx->id_sem_buffers != -1) {
        if (semctl(ctx->id_sem_buffers, 0, IPC_RMID) == -1) {
            perror("semctl");
        }
        ctx->id_sem_buffers = -1;
    }

    if (ctx->id_sem_ack != -1) {
        if (semctl(ctx->id_sem_ack, 0, IPC_RMID) == -1) {
            perror("semctl");
        }
        ctx->id_sem_ack = -1;
    }
}

void liberar_memoria(struct contexto_memoria *ctx) {
    if (ctx->ids_shm != NULL) {
        for (int i = 0; i < ctx->numero_produtores; i++) {
            if (ctx->buffers != NULL && ctx->buffers[i] != NULL) {
                if (shmdt(ctx->buffers[i]) == -1) {
                    perror("shmdt");
                }
                ctx->buffers[i] = NULL;
            }
            if (ctx->ids_shm[i] != -1) {
                if (shmctl(ctx->ids_shm[i], IPC_RMID, NULL) == -1) {
                    perror("shmctl");
                }
                ctx->ids_shm[i] = -1;
            }
        }
    }

    free(ctx->ids_shm);
    free(ctx->buffers);
    ctx->ids_shm = NULL;
    ctx->buffers = NULL;
}

int reservar_slot(struct contexto_memoria *ctx, int eh_consumidor) {
    /*
     * Slots não são reaproveitados: o relatório final precisa de todos os
     * processos e o semáforo de ack do slot pode ter sobrado com valor.
     */
    for (int i = 0; i < MAX_PROCESSOS; i++) {
        if (!ctx->processos[i].em_uso) {
            ctx->processos[i].em_uso = 1;
            ctx->processos[i].eh_consumidor = eh_consumidor;
            ctx->processos[i].tempo_inicio = time(NULL);
            ctx->processos[i].pid = 0;
            return i;
        }
    }
    return -1;
}

/*
 * Cria um conjunto com 'quantidade' semáforos. Nos buffers cada trio começa com
 * MUTEX = 1, VAZIOS = CAPACIDADE_BUFFER e CHEIOS = 0. Nos de ack, tudo em 0.
 */
static int criar_semaforos(int quantidade, int eh_buffers) {
    int id_sem = semget(IPC_PRIVATE, quantidade, IPC_CREAT | 0600);
    if (id_sem == -1) {
        if (errno == EINVAL) {
            fprintf(stderr, "Erro: %d semáforos excedem o limite por conjunto do sistema (SEMMSL).\n", quantidade);
        } else if (errno == ENOSPC) {
            fprintf(stderr, "Erro: limite de semáforos do sistema atingido.\n");
        } else {
            perror("semget");
        }
        return -1;
    }

    unsigned short *valores = calloc((size_t)quantidade, sizeof(unsigned short));
    if (valores == NULL) {
        perror("calloc");
        semctl(id_sem, 0, IPC_RMID);
        return -1;
    }

    if (eh_buffers) {
        for (int i = 0; i < quantidade / 3; i++) {
            valores[SEM_MUTEX(i)] = 1;
            valores[SEM_VAZIOS(i)] = CAPACIDADE_BUFFER;
            valores[SEM_CHEIOS(i)] = 0;
        }
    }

    union semun arg;
    arg.array = valores;
    if (semctl(id_sem, 0, SETALL, arg) == -1) {
        perror("semctl SETALL");
        free(valores);
        semctl(id_sem, 0, IPC_RMID);
        return -1;
    }

    free(valores);
    return id_sem;
}

static int instalar_handler_sigchld(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = recolher_filhos;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

static void recolher_filhos(int sig) {
    (void)sig;
    int errno_salvo = errno;
    pid_t pid;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        if (ctx_sinal != NULL) {
            registrar_fim(ctx_sinal, pid);
        }
    }
    errno = errno_salvo;
}

static void encerrar(struct contexto_memoria *ctx, time_t tempo_inicio_mpmc) {
    /* Sem os semáforos, quem estiver bloqueado em semop recebe EIDRM e sai */
    remover_semaforos(ctx);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
    }
    ctx_sinal = NULL;

    for (int i = 0; i < MAX_PROCESSOS; i++) {
        pid_t pid = ctx->processos[i].pid;
        if (!ctx->processos[i].em_uso || pid <= 0) {
            continue;
        }

        pid_t resultado = waitpid(pid, NULL, 0);
        if (resultado == pid) {
            registrar_fim(ctx, pid);
        } else if (resultado == -1 && errno != ECHILD) {
            perror("waitpid");
        }
    }

    imprimir_relatorio(ctx);
    printf("Tempo total do mpmc: %.0f segundos.\n", difftime(time(NULL), tempo_inicio_mpmc));

    liberar_memoria(ctx);
}

static void liberar_tokens(char **tokens) {
    if (tokens == NULL) {
        return;
    }
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

static char **split(const char *str, const char *delim) {
    int num_partes = 0;
    int anterior_eh_delim = 1;

    for (const char *c = str; c != NULL && *c != '\0'; c++) {
        if (strchr(delim, *c) == NULL) {
            if (anterior_eh_delim) {
                num_partes++;
            }
            anterior_eh_delim = 0;
        } else {
            anterior_eh_delim = 1;
        }
    }

    char **partes = malloc((num_partes + 1) * sizeof(char *));
    if (partes == NULL) {
        fprintf(stderr, "Erro ao alocar memória para os tokens.\n");
        return NULL;
    }
    partes[num_partes] = NULL;

    int indice = 0;
    while (str != NULL && *str != '\0') {
        if (strchr(delim, *str) != NULL) {
            str++;
            continue;
        }

        const char *inicio = str;
        while (*str != '\0' && strchr(delim, *str) == NULL) {
            str++;
        }

        size_t tamanho = str - inicio;
        partes[indice] = malloc(tamanho + 1);
        if (partes[indice] == NULL) {
            fprintf(stderr, "Erro ao alocar memória para o token.\n");
            for (int i = 0; i < indice; i++) {
                free(partes[i]);
            }
            free(partes);
            return NULL;
        }
        memcpy(partes[indice], inicio, tamanho);
        partes[indice][tamanho] = '\0';
        indice++;
    }

    return partes;
}

static int converter_para_inteiro(const char *str, int *sucesso) {
    char *fim;
    errno = 0;

    long val = strtol(str, &fim, 10);

    if (errno == ERANGE || val > INT_MAX || val < INT_MIN) {
        fprintf(stderr, "Erro: Número fora do intervalo suportado.\n");
        *sucesso = 0;
        return 0;
    }

    if (fim == str) {
        fprintf(stderr, "Erro: Nenhum número encontrado na string '%s'.\n", str);
        *sucesso = 0;
        return 0;
    }

    if (*fim != '\0') {
        fprintf(stderr, "Erro: A string contém caracteres inválidos após o número ('%s').\n", fim);
        *sucesso = 0;
        return 0;
    }

    *sucesso = 1;
    return (int)val;
}
