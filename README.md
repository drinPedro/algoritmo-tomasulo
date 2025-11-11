# Algoritmo Tomasulo (C++) 
**Trabalho 2 – Simulação ciclo a ciclo do algoritmo de Tomasulo**

## README

---

## Descrição Geral

Este projeto implementa um **simulador completo do algoritmo de Tomasulo**, que executa **instruções ciclo a ciclo**, controlando:
- Estações de reserva (para soma/subtração e multiplicação/divisão)
- Buffers de carga e armazenamento (Load/Store)
- Reorder Buffer (ROB)
- Banco de registradores e memória principal

O simulador exibe, a cada ciclo, o estado interno de todas as estruturas, permitindo observar **emissão, execução e escrita dos resultados** das instruções de forma detalhada.

O algoritmo foi implementado em **C++17** e pode ser executado tanto em **Linux** quanto em **Windows**.

---

## Compilação

### Linux
```bash
g++ -std=c++17 -O2 -o tomasulo trab2.cpp
```

### Windows
```bash
g++ -std=c++17 -O2 -o tomasulo.exe trab2.cpp
```

---

## Execução

### Usando o arquivo padrão (`instrucoes.txt`):
```bash
./tomasulo
```

### Ou especificando outro arquivo:
```bash
./tomasulo caminho/para/arquivo.txt
```

---

## Exemplo de Entrada (`instrucoes.txt`)

```txt
LD R1, 5  
LD R2, 10    
ADD R3, R1, R2 
ST R3, 20
```

**Descrição:**
1. Carrega o valor imediato `5` em `R1`
2. Carrega o valor imediato `10` em `R2`
3. Soma `R1` + `R2` e armazena o resultado em `R3`
4. Armazena o conteúdo de `R3` na posição de memória `20`

---

## Funcionamento

O simulador segue as etapas do **algoritmo de Tomasulo**, controlando os seguintes estágios a cada ciclo:

1. **Emissão (Issue):** Verifica dependências e estrutura disponível (RS, Load/Store Buffer, ROB).  
2. **Execução (Execute):** Inicia operações cujos operandos estão prontos.  
3. **Escrita de resultado (Write Result):** Transmite resultados no barramento comum.  
4. **Commit (Consolidação):** Escreve resultados no banco de registradores ou memória em ordem.

---

## Estruturas Principais

| Estrutura | Função |
|------------|---------|
| **RS (Estação de Reserva)** | Armazena operações pendentes de execução. |
| **Load Buffer** | Controla instruções de carga imediata (LD). |
| **Store Buffer** | Gerencia instruções de armazenamento (ST). |
| **ROB (Reorder Buffer)** | Garante execução fora de ordem com término em ordem. |
| **Registradores** | Armazena valores e tags de dependência. |
| **Memória** | Representada por um vetor de 1024 posições inteiras. |

---

## Exemplo de Saída

```
====== SIMULADOR TOMASULO ======
Carregadas 4 instrucoes.
LD funciona como LI (Load Immediate)
================================

------------------------------------------------------------
CICLO: 1
PC: 0 / 4

ESTACOES DE RESERVA (ADD/SUB):
Idx | Ocup | Op  | Vj   | Vk   | Qj | Qk | ROB
  0 |    0 | --  |    0 |    0 |  0 |  0 |  0
  ...

BUFFERS DE CARGA (Load Immediate):
Idx | Ocup | Imm  | ROB | ExecRest
  0 |    1 |    5 |   1 |   2
  ...

ROB (cabeca=1 cauda=3):
Idx | Ocup | Op  | Dest | Pronta | Valor | Instr
  1 |    1 | LD  |   1 |     0 |     0 | LD R1, 5
  2 |    1 | LD  |   2 |     0 |     0 | LD R2, 10
  ...

Registradores (valor : tag):
R00=    0 : t= 0    R01=    0 : t= 1    R02=    0 : t= 2    R03=    0 : t= 0
...

====== EXECUCAO FINALIZADA em 9 ciclos ======
```

---
