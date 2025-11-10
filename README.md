# Algoritmo Tomasulo (C++) 
Trabalho 02 - Algoritmo de tomasulo em C/C++

## README

### Objetivo

Simular o algoritmo de Tomasulo (superescalar) ciclo-a-ciclo. O simulador implementa emissão (issue), execução (execute) e gravação de resultados (write-back) com estações de reserva, unidades funcionais, CDB (restrição de 1 broadcast por ciclo), renomeação por tags (Qi), e buffers de load/store. O usuário pode configurar latências e largura de emissão (issue width).

### O que está implementado

* Instruções suportadas: `ADD`, `SUB`, `MUL`, `DIV`, `LD`, `ST`.

  * `ADD dest src1 src2` (registradores)
  * `SUB dest src1 src2`
  * `MUL dest src1 src2`
  * `DIV dest src1 src2`
  * `LD dest address` — carrega um valor imediato da memória (ex.: `LD R1 100`)
  * `ST src address` — armazena valor de `src` em memória no endereço imediato
* Arquivo de entrada: texto com uma instrução por linha (ver seção abaixo).
* Registro de inteiros: `R0` .. `R31` (32 registradores). Todos inicializados a 0.
* Memória simples: mapa de inteiros por endereço (endereços são inteiros). Inicialmente vazia.
* Estaçõess de reserva para Adder (`ADD`/`SUB`) e Multiplier (`MUL`/`DIV`), com número configurável de entradas.
* Load/Store Buffer para `LD` e `ST`.
* Latências (ciclos) por tipo de unidade configuráveis.
* Largura de emissão (número de instruções que podem ser emitidas por ciclo) configurável para simular superescalaridade.
* CDB único (apenas um resultado pode ser gravado por ciclo). Se houver mais de uma instrução pronta para escrever, escolhemos pela ordem de término (primeiro a terminar tem prioridade), e se empatar, pela ordem de emissão.
* Saída ciclo-a-ciclo: estado das estações de reserva (conteúdo, Qi/Qj, Vj/Vk, ocupação, tempo restante), register status (Qi por registrador e seus valores), e estado da memória.

### O que *não* foi implementado

* Reorder Buffer (ROB) e commit in-order. O simulador adota um modelo onde o resultado, uma vez publicado no CDB, atualiza o registrador imediatamente (modelo simplificado de Tomasulo). Se desejar ROB + commit, pode-se estender.
* Branches/Controle de fluxo e previsão de desvio.

### Formato do arquivo de entrada (exemplos)

Cada linha contém uma instrução. Comentários começam com `#`.

Exemplo:

```
# exemplo.txt
LD R1 100       # R1 = MEM[100]
LD R2 104
ADD R3 R1 R2    # R3 = R1 + R2
MUL R4 R3 R2
ST R4 200       # MEM[200] = R4
```

Registradores devem ser referenciados como `R<num>` (ex.: `R0`, `R15`). Endereços de memória para LD/ST são inteiros imediatos.

### Como compilar

Requer compilador compatível com C++17.

No Linux / WSL / macOS:

```bash
g++ -std=c++17 -O2 -o tomasulo_sim tomasulo_sim.cpp
```

### Como executar

```bash
./tomasulo_sim entrada.txt
```

Opcionalmente você pode passar parâmetros via linha de comando para configurar latências e recursos:

```
./tomasulo_sim entrada.txt [issue_width] [num_add_rs] [num_mul_rs] [num_loadbuf] [lat_add] [lat_mul] [lat_load]
```

Exemplo:

```
./tomasulo_sim exemplo.txt 2 3 2 2 2 10 2
```

onde:

* `issue_width` = 2 instruções por ciclo
* `num_add_rs` = 3 estações de reserva para adder
* `num_mul_rs` = 2 estações de reserva para multiplier
* `num_loadbuf` = 2 entradas no buffer de load/store
* `lat_add` = latência de adder (ciclos)
* `lat_mul` = latência do multiplicador
* `lat_load` = latência de load

Se omitidos, valores padrões são usados.

### Saída

O programa imprime no stdout o comportamento ciclo-a-ciclo. Ao final, imprime o estado final dos registradores e da memória utilizada.

---
