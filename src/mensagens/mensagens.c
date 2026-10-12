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

struct mensagem {
    long mtype;  // define a categoria da mensagem
    pid_t pid;     
    int seq;  // número sequencial da mensagem
    time_t tempo_envio;
};

struct node_pid {
    pid_t pid;
    struct node_pid *next;
};

struct node_fila {
    int id_fila;
    struct node_fila *next;
};

static char **split(const char *str, const char *delim);
static int converter_para_inteiro(const char *str, int *sucesso);
static void encerrar_processos(struct node_pid *head);
static void produz(long p, long n_msgs, long ack, int *ids_filas, int numero_produtores);
static void consome(long p, long n_msgs, long ack);
static int registrar_processo(struct node_pid **head, pid_t novo_pid);
static void recolher_filhos(int sig);
static void liberar_tokens(char **tokens);
static void remover_filas(int *ids_filas, int quantidade_filas);

int executar_mensagens(int numero_produtores)
{
    printf("Executando o paradigma de troca de mensagens com %d produtores...\n", numero_produtores);

    if (signal(SIGCHLD, recolher_filhos) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    struct node_pid *head = NULL;

    int *ids_filas;

    ids_filas = malloc(numero_produtores * sizeof(int));

    if (ids_filas == NULL) {
        perror("malloc");
        return 1;
    }

    for (int indice_fila = 0; indice_fila < numero_produtores; indice_fila++) {
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
            encerrar_processos(head);

            remover_filas(ids_filas, numero_produtores);

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
                encerrar_processos(head);

                remover_filas(ids_filas, numero_produtores);

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
                printf("Mensagem de produção recebida.\n");
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
                printf("Mensagem de consumo recebida.\n");
            }
            else {
                fprintf(stderr, "Erro: para 'consome', p deve estar entre 1 e %d.\n", numero_produtores);
                liberar_tokens(tokens);
                continue;
            }
        }

        liberar_tokens(tokens);

        printf("Mensagem processada com sucesso.\n");

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
                produz(p, n_msgs, ack, ids_filas, numero_produtores);
                printf("Mensagem de produção recebida.\n");
            }

            if (is_consome)
            {
                consome(p, n_msgs, ack);
                printf("Mensagem de consumo recebida.\n");
            }    

            exit(0);
        } 
        // pai
        else {
            if(registrar_processo(&head, pid) == 0) {
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

static void remover_filas(int *ids_filas, int quantidade_filas) {
    for (int indice_fila = 0; indice_fila < quantidade_filas; indice_fila++) {
        if (msgctl(ids_filas[indice_fila], IPC_RMID, NULL) == -1) {
            perror("msgctl");
        }
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

static int registrar_processo(struct node_pid **head, pid_t novo_pid) {
    struct node_pid *next = malloc(sizeof(struct node_pid));
    if (next == NULL) {
        fprintf(stderr, "Erro ao alocar memória para o nó.\n");
        return 0;
    }
    
    next->pid = novo_pid;
    next->next = NULL;

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

static void produz(long p, long n_msgs, long ack, int *ids_filas, int numero_produtores) {
    (void)ack;
    (void)ids_filas;
    (void)numero_produtores;
    printf("produzindo %ld mensagens na fila %ld\n", n_msgs, p);
}

static void consome(long p, long n_msgs, long ack) {
    (void)ack;
    printf("consumindo %ld mensagens da fila %ld\n", n_msgs, p);
}

static void encerrar_processos(struct node_pid *head) {
    struct node_pid *atual = head;
    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR) {
        perror("signal");
    }
    while (atual != NULL) {
        struct node_pid *proximo = atual->next;

        pid_t resultado = waitpid(atual->pid, NULL, WNOHANG);

        if (resultado == 0) {
            if (kill(atual->pid, SIGTERM) == -1) {
                // Só avisa se o erro não for "Processo já inexistente"
                if (errno != ESRCH) {
                    fprintf(stderr, "Erro ao enviar sinal para o processo %d: %s\n", atual->pid, strerror(errno));
                }
            } else {
                if (waitpid(atual->pid, NULL, 0) == -1 && errno != ECHILD) {
                    perror("waitpid");
                }
            }
        } else if (resultado == -1 && errno != ECHILD) {
            perror("waitpid");
        }

        free(atual);
        atual = proximo;
    }
}

static void recolher_filhos(int sig) {
    (void)sig;
    int errno_salvo = errno;
    signal(SIGCHLD, recolher_filhos);
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
    errno = errno_salvo;
}
