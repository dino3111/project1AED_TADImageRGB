# AED 2025 - Trabalho 1: Imagens com Cor Indexada (Pseudocor)

## Grade: 17,4

Este projeto foi desenvolvido como parte do Trabalho 1 da disciplina de **Algoritmos e Estruturas de Dados (AED)**, no ano letivo de 2025. O objetivo é implementar operações básicas e avançadas para manipulação de imagens com cor indexada.

## Estrutura do Projeto

```plaintext
AED_2526_TRAB_1_FICHEIROS_ALUNOS/
├── img/                  # Imagens de exemplo
├── imageRGB.c            # Implementação principal
├── imageRGB.h            # Header principal
├── PixelCoords*.c/.h     # Estruturas auxiliares
├── instrumentation.c/.h  # Contadores de operações
├── error.c/.h            # Tratamento de erros
├── Makefile              # Script de compilação
├── README.md             # Este arquivo
```


## Instruções de Uso

### 1. Limpar Arquivos Gerados

Antes de compilar, remova arquivos gerados anteriormente:
```bash
$ make clean
```

### 2. Compilar o Projeto

Para compilar todos os arquivos e gerar os executáveis:
```bash
$ make
```

### 3. Executar Testes Básicos

Para executar os testes básicos e verificar as funcionalidades principais:
```bash
$ ./imageRGBTest
```


## Notas Importantes

- Certifique-se de que todas as dependências estão instaladas.
- Utilize o comando `make clean` antes de cada recompilação para evitar conflitos.
- As imagens geradas pelos testes serão salvas no diretório atual.


**Desenvolvido por:**
---

- Claudino José Martins - 127368 - LEI
- Maria Moreira Mané - 125102 - LEI
