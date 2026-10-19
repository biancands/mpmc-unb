#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h> // Para usar INT_MIN e INT_MAX
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mensagens.h"
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define TIPO_MENSAGEM 1

struct mensagem {
    long mtype;  // define a categoria da mensagem
    pid_t pid;     
    unsigned short seq;  // número sequencial da mensagem
    unsigned char requer_confirmacao;
    time_t tempo_envio;
};

struct resultado_processo {
    long mtype;
    time_t tempo_fim;
    pid_t pid;
    unsigned short mensagens_consumidas;
};

struct node_pid {
    pid_t pid;
    struct node_pid *next;
    time_t tempo_inicio;
    int eh_consumidor;
};

static char **split(const char *str, const char *delim);
static int converter_para_inteiro(const char *str, int *sucesso);
static void encerrar_processos(struct node_pid *head, int id_fila_resultados);
static int produz(long p, long n_msgs, long ack, int *ids_filas, int numero_produtores);
static int consome(long p, long n_msgs, long ack, int *ids_filas, int numero_produtores, unsigned short *mensagens_consumidas);
static int registrar_processo(struct node_pid **head, pid_t novo_pid, time_t tempo_inicio, int eh_consumidor);
static void recolher_filhos(int sig);
static void liberar_tokens(char **tokens);
static void remover_filas(int *ids_filas, int quantidade_filas);
static void remover_fila(int id_fila);
static int enviar_resultado(int id_fila_resultados, unsigned short quantidade_consumida);

int executar_mensagens(int numero_produtores)
{
    time_t tempo_inicio_mpmc = time(NULL);
    int quantidade_filas = numero_produtores + 2; // +1 para confirmacoes de consumo e +1 para resultados
    printf("Executando o paradigma de troca de mensagens com %d produtores...\n", numero_produtores);

    if (signal(SIGCHLD, recolher_filhos) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    struct node_pid *head = NULL;

    int *ids_filas;

    ids_filas = malloc(quantidade_filas * sizeof(int));

    if (ids_filas == NULL) {
        perror("malloc");
        return 1;
    }

    for (int indice_fila = 0; indice_fila < quantidade_filas; indice_fila++) {
        ids_filas[indice_fila] = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
        if (ids_filas[indice_fila] == -1) {
            perror("msgget");
            remover_filas(ids_filas, indice_fila);
            free(ids_filas);
            return 1;
        }
    }

    while (1)
    {
        printf(">> ");
        fflush(stdout);

        char buffer[256];

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            int resultado = ferror(stdin);
            if (resultado != 0) {
                perror("fgets");
            }
            remover_filas(ids_filas, numero_produtores + 1);
            encerrar_processos(head, ids_filas[numero_produtores + 1]);

            remover_fila(ids_filas[numero_produtores + 1]);

            free(ids_filas);
            return resultado != 0;
        }

        if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
            int caractere;

            while ((caractere = fgetc(stdin)) != '\n' && caractere != EOF) { }

            fprintf(stderr, "Erro: comando excede o tamanho máximo permitido.\n");
            continue;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0'; // Remove a quebra de linha do final

        char **tokens = split(buffer, " \t\n");
        if (tokens == NULL)
        {
            fprintf(stderr, "Erro ao dividir a mensagem em tokens.\n");
            continue;
        }

        if (tokens[0] == NULL)
        {
            liberar_tokens(tokens);
            continue;
        }

        printf("Mensagem recebida: %s\n", buffer);

        int is_shutdown = strcmp(tokens[0], "shutdown") == 0;
        int is_produz = strcmp(tokens[0], "produz") == 0;
        int is_consome = strcmp(tokens[0], "consome") == 0;

        int numTokens = 0;
        while (tokens[numTokens] != NULL)
        {
            numTokens++;
        }

        if (is_shutdown) {
            if (numTokens != 1) {
                fprintf(stderr, "Mensagem inválida. O comando 'shutdown' não aceita argumentos.\n");
                liberar_tokens(tokens);
                continue;
            }
            else {
                remover_filas(ids_filas, numero_produtores + 1);
                encerrar_processos(head, ids_filas[numero_produtores + 1]);

                printf("Tempo total do mpmc: %.0f segundos.\n", difftime(time(NULL), tempo_inicio_mpmc));

                remover_fila(ids_filas[numero_produtores + 1]);

                free(ids_filas);
                liberar_tokens(tokens);
                return 0;
            }
        }

        if (!is_produz && !is_consome)
        {
            fprintf(stderr, "Mensagem inválida. Esperado 'produz' ou 'consome' como primeiro token.\n");
            liberar_tokens(tokens);
            continue;
        }

        if (numTokens != 5)
        {
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
        int error_conversao = 0;
        for (int i = 1; i < numTokens - 1; i++)
        {
            int sucesso;
            int valor_convertido = converter_para_inteiro(tokens[i], &sucesso);
            if (!sucesso)
            {
                fprintf(stderr, "Erro ao converter o token %d para inteiro.\n", i);
                fprintf(stderr, "Token: %s\n", tokens[i]);

                // Libera a memória alocada para os tokens
                liberar_tokens(tokens);
                error_conversao = 1;
                break;
            }
            valores[i - 1] = valor_convertido;
        }

        if (error_conversao) {
            continue;
        }

        long p = valores[0];
        long n_msgs = valores[1];
        long ack = valores[2];

        if (ack != 1 && ack != 2) {
            fprintf(stderr, "Ack deve ser 1 ou 2.\n");
            liberar_tokens(tokens);
            continue;
        }

        if (n_msgs <= 0 || n_msgs > 1024) {
            fprintf(stderr, "O número de mensagens deve estar entre 1 e 1024\n");
            liberar_tokens(tokens);
            continue;
        }


        if (is_produz)
        {
            if (p >= 0 && p <= numero_produtores) {
                printf("Comando de produção validado.\n");
            }
            else {
                fprintf(stderr, "Erro: para 'produz', p deve estar entre 0 e %d.\n", numero_produtores);
                liberar_tokens(tokens);
                continue;
            }
        }

        else if (is_consome)
        {
            if (p >= 1 && p <= numero_produtores) {
                printf("Comando de consumo validado.\n");
            }
            else {
                fprintf(stderr, "Erro: para 'consome', p deve estar entre 1 e %d.\n", numero_produtores);
                liberar_tokens(tokens);
                continue;
            }
        }

        liberar_tokens(tokens);

        printf("Comando validado com sucesso.\n");

        printf("Valores extraídos: p = %ld, n_msgs = %ld, ack = %ld\n", p, n_msgs, ack);

        fflush(NULL);
        pid_t pid = fork();
    
        if (pid < 0) {
            perror("fork");
            continue;
        }
        
        // filho
        else if (pid == 0) {
            if (is_produz)
            {
                int resultado_producao;
                resultado_producao = produz(p, n_msgs, ack, ids_filas, numero_produtores);
                if (resultado_producao == 0){
                    printf("Produção concluída com sucesso.\n");
                }
                if (enviar_resultado(ids_filas[numero_produtores + 1], 0) == 1) {
                    resultado_producao = 1;
                }
                exit(resultado_producao);
            }

            if (is_consome)
            {
                int resultado_consumo;
                unsigned short mensagens_consumidas;
                resultado_consumo = consome(p, n_msgs, ack, ids_filas, numero_produtores, &mensagens_consumidas);
                if (resultado_consumo == 0) {
                    printf("Consumo concluído com sucesso.\n");
                }
                if (enviar_resultado(ids_filas[numero_produtores + 1], mensagens_consumidas) == 1) {
                    resultado_consumo = 1;
                }
                exit(resultado_consumo);
            }    

            exit(0);
        } 
        // pai
        else {
            if(registrar_processo(&head, pid, time(NULL), is_consome) == 0) {
                if (kill(pid, SIGTERM) == -1) {
                    if (errno != ESRCH) {
                        perror("kill");
                    }
                } else {
                    if ((waitpid(pid, NULL, 0) == -1) && errno != ECHILD) {
                        perror("waitpid");
                    }
                }
            }
        }

        
    }

    return 1;
}

static void liberar_tokens(char **tokens) {
    if (tokens == NULL) {
        return;
    }
    int i = 0;
    while (tokens[i] != NULL) {
        free(tokens[i]);
        i++;
    }
    free(tokens);
}

static void remover_fila(int id_fila) {
    if (msgctl(id_fila, IPC_RMID, NULL) == -1) {
        perror("msgctl");
    }
}

static void remover_filas(int *ids_filas, int quantidade_filas) {
    for (int indice_fila = 0; indice_fila < quantidade_filas; indice_fila++) {
        remover_fila(ids_filas[indice_fila]);
    }
}

static char **split(const char *str, const char *delim)
{
    int numChunks = 0;
    int beforeIsDelim = 1;
    const char *head = str;

    // Remove o delimitador do início da string
    while (str != NULL && *str != '\0' && strchr(delim, *str) != NULL)
    {
        str++;
    }

    while (str != NULL && *str != '\0')
    {
        if (strchr(delim, *str) == NULL) // nao é delimitador
        {
            if (beforeIsDelim == 1)
            {
                numChunks++;
            }
            beforeIsDelim = 0;
        }
        else if (beforeIsDelim == 0)
        {
            beforeIsDelim = 1;
        }
        str++;
    }

    str = head;

    char **ans = malloc((numChunks + 1) * sizeof(char *));
    if (ans == NULL)
    {
        fprintf(stderr, "Erro ao alocar memória para os tokens.\n");
        return NULL;
    }
    ans[numChunks] = NULL;

    int chunkIndex = 0;
    beforeIsDelim = 1;

    // Remove o delimitador do início da string
    while (str != NULL && *str != '\0' && strchr(delim, *str) != NULL)
    {
        str++;
    }

    while (str != NULL && *str != '\0')
    {
        if (strchr(delim, *str) == NULL) // nao é delimitador
        {
            if (beforeIsDelim == 1)
            {
                const char *start = str;
                while (*str != '\0' && strchr(delim, *str) == NULL)
                {
                    str++;
                }
                size_t length = str - start;
                ans[chunkIndex] = malloc((length + 1) * sizeof(char));
                if (ans[chunkIndex] == NULL)
                {
                    fprintf(stderr, "Erro ao alocar memória para o token.\n");
                    // Libera a memória alocada anteriormente
                    for (int i = 0; i < chunkIndex; i++)
                    {
                        free(ans[i]);
                    }
                    free(ans);
                    return NULL;
                }
                strncpy(ans[chunkIndex], start, length);
                ans[chunkIndex][length] = '\0';
                chunkIndex++;
            }
            beforeIsDelim = 0;
            continue;
        }
        else if (beforeIsDelim == 0)
        {
            beforeIsDelim = 1;
        }
        str++;
    }

    return ans;
}

// Função para converter string em inteiro com validação robusta
static int converter_para_inteiro(const char *str, int *sucesso) {
    char *fim;
    errno = 0; // Limpa o erro antes da conversão

    long val = strtol(str, &fim, 10);

    // 1. Verifica se houve estouro de capacidade (ERANGE)
    // 2. Verifica se o valor cabe em um 'int' normal (evita valores gigantescos)
    if (errno == ERANGE || val > INT_MAX || val < INT_MIN)
    {
        fprintf(stderr, "Erro: Número fora do intervalo suportado.\n");
        *sucesso = 0;
        return 0;
    }

    // Verifica se nenhum caractere foi convertido (ex: usuário digitou letras)
    if (fim == str)
    {
        fprintf(stderr, "Erro: Nenhum número encontrado na string '%s'.\n", str);
        *sucesso = 0;
        return 0;
    }

    // Verifica se sobrou lixo no final da string (ex: "2abc")
    if (*fim != '\0')
    {
        fprintf(stderr, "Erro: A string contém caracteres inválidos após o número ('%s').\n", fim);
        *sucesso = 0;
        return 0;
    }

    *sucesso = 1;
    return (int)val;
}

static int registrar_processo(struct node_pid **head, pid_t novo_pid, time_t tempo_inicio, int eh_consumidor) {
    struct node_pid *next = malloc(sizeof(struct node_pid));
    if (next == NULL) {
        fprintf(stderr, "Erro ao alocar memória para o nó.\n");
        return 0;
    }
    
    next->pid = novo_pid;
    next->next = NULL;
    next->tempo_inicio = tempo_inicio;
    next->eh_consumidor = eh_consumidor;

    if (*head == NULL) {
        *head = next;
    } else {
        struct node_pid *atual = *head;
        while (atual->next != NULL) {
            atual = atual->next;
        }
        atual->next = next;
    }
    return 1;
}

static int produz(long p, long n_msgs, long ack, int *ids_filas, int numero_produtores) {
    struct mensagem mensagem;
    mensagem.mtype = TIPO_MENSAGEM;
    mensagem.pid = getpid();
    mensagem.requer_confirmacao = (ack == 1) ? 1 : 0;

    int id_fila_confirmacao = ids_filas[numero_produtores];
    int confirmacoes_recebidas = 0;

    for (int sequencia = 1; sequencia <= n_msgs; sequencia++) {
        int indice_fila;
        if (p == 0) {
            indice_fila = (sequencia - 1) % numero_produtores;
        } else {
            indice_fila = p - 1;
        }

        mensagem.seq = sequencia;
        mensagem.tempo_envio = time(NULL);

        if(msgsnd(ids_filas[indice_fila], &mensagem, (sizeof(mensagem) - sizeof(mensagem.mtype)), 0) == -1) {
            if (errno == EINTR || errno == EIDRM || errno == EINVAL) {
                return 1;
            }
            perror("msgsnd");
            return 1;
        }

        if (ack == 1) {
            while (msgrcv(id_fila_confirmacao, &mensagem, (sizeof(mensagem) - sizeof(mensagem.mtype)), getpid(), IPC_NOWAIT) != -1) {
                confirmacoes_recebidas++;
            }
            if (errno == EINTR || errno == EIDRM || errno == EINVAL) {
                return 1;
            }
            if (errno != ENOMSG) {
                perror("msgrcv oportunista");
                return 1;
            }
        }
    }

    if (ack == 1) {
        while (confirmacoes_recebidas < n_msgs) {
            if (msgrcv(id_fila_confirmacao, &mensagem, (sizeof(mensagem) - sizeof(mensagem.mtype)), getpid(), 0) == -1) {
                if (errno == EINTR) return 1;
                perror("msgrcv final");
                return 1;
            }
            confirmacoes_recebidas++;
        }
    }

    if (p != 0) {
        printf("%ld mensagens produzidas na fila %ld\n", n_msgs, p);
    } else {
        printf("%ld mensagens produzidas no padrão striped\n", n_msgs);
    }

    return 0;
}

static int consome(long p, long n_msgs, long ack, int *ids_filas, int numero_produtores, unsigned short *mensagens_consumidas) {
    int indice_fila_confirmacao = ids_filas[numero_produtores];
    int flags_recebimento = 0;
    *mensagens_consumidas = 0;
    if (ack == 2) {
        flags_recebimento = IPC_NOWAIT;
    }
    struct mensagem mensagem;

    for (int mensagens_recebidas = 0; mensagens_recebidas < n_msgs; mensagens_recebidas++) {
        if (msgrcv(ids_filas[p - 1], &mensagem, (sizeof(mensagem) - sizeof(mensagem.mtype)), TIPO_MENSAGEM, flags_recebimento) == -1) {
            if (ack == 2 && errno == ENOMSG) {
                fprintf(stderr, "Erro: foram solicitadas %ld mensagens, mas apenas %d foram consumidas.\n", n_msgs, mensagens_recebidas);
                return 1;
            }
            if (errno == EINTR || errno == EIDRM || errno == EINVAL) {
                return 1;
            }
            perror("msgrcv");
            return 1;
        }
        (*mensagens_consumidas)++;
        printf("Mensagem consumida: pid=%d, seq=%d, tempo=%lld\n", (int)mensagem.pid, mensagem.seq, (long long)mensagem.tempo_envio);

        if (mensagem.requer_confirmacao == 1) {
            mensagem.mtype = mensagem.pid;
            if (msgsnd(indice_fila_confirmacao, &mensagem, (sizeof(mensagem) - sizeof(mensagem.mtype)), 0) == -1) {
                if (errno == EINTR || errno == EIDRM || errno == EINVAL) {
                    return 1;
                }
                perror("msgsnd");
                return 1;
            }
        }
    }
    printf("%ld mensagens consumidas da fila %ld\n", n_msgs, p);
    return 0;
}

static void encerrar_processos(struct node_pid *head, int id_fila_resultados) {
    struct node_pid *atual = head;
    unsigned long total_mensagens_consumidas = 0;
    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
        perror("signal");
    }
    while (atual != NULL) {
        struct node_pid *proximo = atual->next;
        struct resultado_processo resultado_p;

        pid_t resultado = waitpid(atual->pid, NULL, 0);

        if (resultado == -1 && errno != ECHILD) {
            perror("waitpid");
        }

        if (msgrcv(id_fila_resultados, &resultado_p, (sizeof(resultado_p) - sizeof(resultado_p.mtype)), atual->pid, IPC_NOWAIT) == -1) {
            if (errno != ENOMSG) {
                perror("msgrcv");
            }
        } else {
            printf("Relatório recebido: pid=%d, consumidas=%hu, tempo=%.0f segundos.\n", (int)resultado_p.pid, resultado_p.mensagens_consumidas, (difftime(resultado_p.tempo_fim, atual->tempo_inicio)));
            total_mensagens_consumidas += resultado_p.mensagens_consumidas;
        }

        free(atual);
        atual = proximo;
    }
    printf("Total de mensagens consumidas: %lu\n", total_mensagens_consumidas);
}

static void recolher_filhos(int sig) {
    (void)sig;
    int errno_salvo = errno;
    signal(SIGCHLD, recolher_filhos);
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
    errno = errno_salvo;
}

static int enviar_resultado(int id_fila_resultados, unsigned short quantidade_consumida) {
    struct resultado_processo resultado_processo;
    resultado_processo.mtype = getpid();
    resultado_processo.tempo_fim = time(NULL);
    resultado_processo.pid = getpid();
    resultado_processo.mensagens_consumidas = quantidade_consumida;

    if (msgsnd(id_fila_resultados, &resultado_processo, (sizeof(resultado_processo) - sizeof(resultado_processo.mtype)), 0) == -1) {
        perror("msgsnd");
        return 1;
    }
    return 0;
}
