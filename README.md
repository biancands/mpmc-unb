# mpmc-unb

Implementação do problema de múltiplos produtores e múltiplos consumidores usando memória compartilhada, semáforos e filas de mensagens em C.

## Arquitetura

O projeto gera um único programa final, `mpmc`. O `main` será responsável apenas por validar os argumentos iniciais e encaminhar a execução para o paradigma selecionado. Depois disso, cada módulo controla seu próprio prompt, processos, mecanismos IPC, estatísticas e encerramento.

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
│   ├── mensagens/
│   │   └── mensagens.c
│   └── stubs/
│       ├── memoria_stub.c
│       └── mensagens_stub.c
├── Makefile
└── README.md
```

Os stubs permitem compilar e testar um paradigma mesmo quando o outro ainda não estiver implementado.

## Divisão do paradigma de memória compartilhada

O contrato entre as partes está em `include/memoria_interno.h`. Cada arquivo tem um único responsável, e os pontos a implementar estão marcados com `TODO (A)`, `TODO (B)` ou `TODO (C)`.

| Arquivo | Responsável | Escopo |
|---|---|---|
| `src/memoria/memoria.c` | A | prompt, fork, criação e remoção de shm e semáforos, tabela de processos, SIGCHLD, shutdown |
| `src/memoria/memoria_buffer.c` | B | buffer circular sincronizado com semáforos System V (`semop`), modo não bloqueante |
| `src/memoria/memoria_ops.c` | C | produção (direta e striped), consumo, confirmação (ack), relatório e testes de integração |

Alterações em `include/memoria_interno.h` devem ser combinadas entre os três antes do commit. Após alterar o header, rode `make clean`.

## Compilação

| Comando | Módulos utilizados | Executável |
|---|---|---|
| `make mensagens` | mensagens reais + stub de memória | `build/mpmc-msg` |
| `make memoria` | memória real + stub de mensagens | `build/mpmc-shm` |
| `make completo` | ambas as implementações reais | `build/mpmc` |
| `make clean` | remove os arquivos gerados | - |
