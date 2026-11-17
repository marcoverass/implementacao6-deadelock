# Algoritmo do banqueiro

O programa lê os recursos disponíveis pela linha de comando e os limites de cada cliente pelo arquivo `customer.txt`. Em seguida, processa os comandos de `commands.txt` e grava as decisões e os estados solicitados em `result.txt`.

## Compilar e executar

Coloque `customer.txt` e `commands.txt` na pasta `mavss`. Na raiz do repositório, use um terminal Linux, Unix ou macOS com compilador C e `make`:

```sh
cd mavss
make
./banker 10 5 7
```

Cada argumento indica a quantidade inicial de um tipo de recurso. Em `customer.txt`, coloque uma linha por cliente, com o máximo de cada recurso separado por vírgulas. A primeira linha corresponde ao cliente `0`, a segunda ao cliente `1` e assim por diante.

Em `commands.txt`, escreva um comando por linha:

- `RQ 0 0 2 1` solicita os recursos `0 2 1` para o cliente `0`.
- `RL 0 0 1 1` devolve os recursos `0 1 1` do cliente `0`.
- `*` grava o estado atual, com máximo, alocação, necessidade e disponibilidade.

Uma solicitação é aprovada quando cabe no máximo do cliente e no saldo atual e deixa uma sequência possível para todos os clientes terminarem. Uma devolução acima da alocação do cliente é recusada. As recusas são registradas em `result.txt` e o programa segue para o próximo comando. Entradas malformadas e falhas de arquivo são informadas na saída de erro e encerram a execução com código diferente de zero.

Use `make clean` para remover o executável.
