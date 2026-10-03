#include <stdio.h>

int main(int argc, char *argv[]) {
    
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <p> <paradigma>\n", argv[0]);
        return 1;
    }
    printf("Hello, world!\n");
    return 0;
}
