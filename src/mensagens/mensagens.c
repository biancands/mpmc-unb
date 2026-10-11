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

char **split(const char *str, const char *delim);
int converter_para_inteiro(const char *str, int *sucesso);
void adicionar_processo(struct node_pid **head, pid_t novo_pid);
void shutdown(struct node_pid *head);
void produz(long p, long n_msgs, long ack);
void consome(long p, long n_msgs, long ack);
void stop(int sig);
void novo_processo(struct node_pid **head, pid_t novo_pid);

int executar_mensagens(int numero_produtores)
{
    printf("Executando o paradigma de troca de mensagens com %d produtores...\n", numero_produtores);
    signal(SIGTERM, stop);

    struct node_pid *head = NULL;
    
    while (1)
    {
        printf(">> ");

        char buffer[256];
        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            return 0;
        }
        buffer[strcspn(buffer, "\r\n")] = '\0'; // Remove a quebra de linha do final
        printf("Mensagem recebida: %s\n", buffer);

        char **tokens = split(buffer, " \t\n");
        if (tokens == NULL)
        {
            fprintf(stderr, "Erro ao dividir a mensagem em tokens.\n");
            continue;
        }

        if (tokens[0] == NULL)
        {
            free(tokens);
            continue;
        }

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
                for (int i = 0; i < numTokens; i++) {
                    free(tokens[i]);
                }
                free(tokens);
                continue;
            }
            else {
                shutdown(head);
                for (int i = 0; i < numTokens; i++)
                {
                    free(tokens[i]);
                }
                free(tokens);
                return 0;
            }
        }

        if (!is_produz && !is_consome)
        {
            fprintf(stderr, "Mensagem inválida. Esperado 'produz' ou 'consome' como primeiro token.\n");

            for (int i = 0; i < numTokens; i++)
            {
                free(tokens[i]);
            }
            free(tokens);
            continue;;
        }

        if (numTokens != 5)
        {
            fprintf(stderr, "Mensagem inválida. Esperado exatamente 5 tokens.\n");
            for (int i = 0; i < numTokens; i++)
            {
                free(tokens[i]);
            }
            free(tokens);
            continue;
        }

        if (strcmp(tokens[4], "&") != 0) {
            fprintf(stderr, "Mensagem inválida. O último token deve ser '&'.\n");
            for (int i = 0; i < numTokens; i++) {
                free(tokens[i]);
            }
            free(tokens);

            continue;
        }

        long valores[3];
        int error_conversao = 0;
        for (int i = 1; i < numTokens - 1; i++)
        {
            int sucesso;
            int p = converter_para_inteiro(tokens[i], &sucesso);
            if (!sucesso)
            {
                fprintf(stderr, "Erro ao converter o token %d para inteiro.\n", i);
                fprintf(stderr, "Token: %s\n", tokens[i]);

                // Libera a memória alocada para os tokens
                for (int i = 0; i < numTokens; i++)
                {
                    free(tokens[i]);
                }
                free(tokens);
                error_conversao = 1;
                break;
            }
            valores[i - 1] = p;
        }

        if (error_conversao) {
            continue;
        }

        for (int i = 0; i < numTokens; i++)
        {
            free(tokens[i]);
        }
        free(tokens);

        long p = valores[0];



        if (is_produz)
        {
            if (p >= 0 && p <= numero_produtores) {
                printf("Mensagem de produção recebida.\n");
            }
            else {
                fprintf(stderr, "Erro: para 'produz', p deve estar entre 0 e %d.\n", numero_produtores);
                for (int i = 0; i < numTokens; i ++) {
                    free(tokens[i]);
                }
                free(tokens);
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
                for (int i = 0; i < numTokens; i ++) {
                    free(tokens[i]);
                }
                free(tokens);
                continue;
            }
        }


        printf("Mensagem processada com sucesso.\n");
        long n_msgs = valores[1];
        long ack = valores[2];

        printf("Valores extraídos: p = %ld, n_msgs = %ld, ack = %ld\n", p, n_msgs, ack);

        int pid = fork();
    
        if (pid < 0) {
            fprintf(stderr, "Erro na criação do processo.\n");
        }
        
        // filho
        else if (pid == 0) {
            if (is_produz)
            {
                produz(p, n_msgs, ack);
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
            novo_processo(&head, pid);
        }

        
    }

    return 1;
}

char **split(const char *str, const char *delim)
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
int converter_para_inteiro(const char *str, int *sucesso) {
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

void novo_processo(struct node_pid **head, pid_t novo_pid) {
    struct node_pid *next = malloc(sizeof(struct node_pid));
    if (next == NULL) {
        fprintf(stderr, "Erro ao alocar memória para o nó.\n");
        return;
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
}

void produz(long p, long n_msgs, long ack) {
    printf("produzindo %d mensagens na fila %d", n_msgs, p);
}

void consome(long p, long n_msgs, long ack) {
    printf("consumindo %d mensagens da fila %d", n_msgs, p);
}

void shutdown(struct node_pid *head) {
    struct node_pid *atual = head;
    while (atual != NULL) {
        struct node_pid *proximo = atual->next;
        
        if (kill(atual->pid, SIGTERM) == -1) {
            // Só avisa se o erro não for "Processo já inexistente"
            if (errno != ESRCH) {
                fprintf(stderr, "Erro ao enviar sinal para o processo %d: %s\n", atual->pid, strerror(errno));
            }
        } else {
            waitpid(atual->pid, NULL, 0);
        }

        free(atual);
        atual = proximo;
    }
}

void stop(int sig) {
    exit(0);
}