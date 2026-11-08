# mpmc-unb

Implementação do problema de múltiplos produtores e múltiplos consumidores usando memória compartilhada, semáforos e filas de mensagens em C.

## Arquitetura

O projeto gera um único programa final, `mpmc`. O `main` é responsável apenas por validar os argumentos iniciais e encaminhar a execução para o paradigma selecionado. Depois disso, cada módulo controla seu próprio prompt, processos, mecanismos IPC, estatísticas e encerramento.

```mermaid
flowchart TD
    Inicio["mpmc p paradigma"] --> Main["main.c<br/>valida p e paradigma"]
    Main --> Escolha{"Qual paradigma?"}

    Escolha -- "1" --> Memoria["executar_memoria(p)"]
    Escolha -- "2" --> Mensagens["executar_mensagens(p)"]

    Memoria --> ModuloMemoria["src/memoria/<br/>memória compartilhada + semáforos"]
    Mensagens --> ModuloMensagens["src/mensagens/<br/>filas de mensagens"]

    ModuloMemoria --> PromptMemoria["prompt, produz, consome e shutdown"]
    ModuloMensagens --> PromptMensagens["prompt, produz, consome e shutdown"]
```

Os módulos são independentes. A integração entre as duas frentes acontece somente pelas funções públicas declaradas em `include/memoria.h` e `include/mensagens.h`.

## Estrutura de diretórios

```text
mpmc-unb/
├── include/
│   ├── memoria.h
│   ├── memoria_interno.h
│   └── mensagens.h
├── src/
│   ├── main.c
│   ├── memoria/
│   │   ├── memoria.c
│   │   ├── memoria_buffer.c
│   │   └── memoria_ops.c
│   └── mensagens/
│   │   └── mensagens.c
├── .gitignore
├── Makefile
└── README.md
```

## Compilação

| Comando | Ação |
|---|---|
| `make` | compila os dois paradigmas e gera `build/mpmc` |
| `make clean` | remove os arquivos gerados |
