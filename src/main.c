#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <p> <paradigma>\n", argv[0]);
        return 1;
    }

    long valores[2];

    for (int argumento = 1; argumento < argc; argumento++) {
        long arg;
        char *fim;
        errno = 0;
        arg = strtol(argv[argumento], &fim, 10);

        if (errno == ERANGE) {
            fprintf(stderr, "Valor fora do intervalo permitido\n");
            return 1;
        }
        if (fim == argv[argumento]) {
            fprintf(stderr, "Nenhum caractere foi convertido\n");
            return 1;
        }
        else if (*fim != '\0') {
            fprintf(stderr, "O argumento %s é inválido\n", argv[argumento]);
            return 1;
        }
        if (argumento == 1 && arg < 1) {
            fprintf(stderr, "O argumento 'p' deve ser maior que zero\n");
            return 1;
        }
        else if (argumento == 1 && arg > INT_MAX) {
            fprintf(stderr, "O argumento 'p' deve ser menor ou igual a %d\n", INT_MAX);
            return 1;
        }

        else if (argumento == 2 && (arg != 1 && arg != 2)) {
            fprintf(stderr, "O argumento 'paradigma' deve ser 1 ou 2\n");
            return 1;
        }

        valores[argumento - 1] = arg;
    }
    int numero_produtores;
    int paradigma;
    numero_produtores = (int)valores[0];
    paradigma = (int)valores[1];
    printf("Produtores: %d\n", numero_produtores);
    printf("Paradigma: %d\n", paradigma);

    return 0;
}
