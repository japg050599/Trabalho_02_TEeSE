# Implementação Processor-in-the-Loop (PIL) no TMS320F28379D

Este projeto implementa uma estratégia **Processor-in-the-Loop (PIL)** para um inversor monofásico conectado à rede elétrica utilizando o microcontrolador **TMS320F28379D**.

Nesta implementação, a **planta do conversor permanece simulada no ambiente PLECS**, enquanto o **controlador proporcional-ressonante (PR)** é executado diretamente no hardware, utilizando o coprocessador **Control Law Accelerator (CLA)**. A comunicação entre o computador e o microcontrolador é realizada através da interface **SCI (Serial Communication Interface)**.

O objetivo desta implementação é avaliar o comportamento do controlador digital quando executado em hardware real, preservando a planta simulada no computador.

---

# Funcionamento da Implementação

O fluxo completo da implementação é apresentado abaixo.

1. O **PLECS** simula a planta do inversor e calcula:
   - tensão da rede (`vo`);
   - corrente do filtro (`io`).

2. Essas duas variáveis são enviadas ao TMS320F28379D através da interface SCI.

3. A interrupção de recepção (`INT_SCI0_RX_ISR`) da CPU1 recebe os dados e os armazena na memória compartilhada **CpuToCla1MsgRAM**.

4. A CPU1 atualiza a referência senoidal de corrente (`iref`) sincronizada com a frequência da rede (60 Hz).

5. Em seguida, a CPU1 dispara a execução da **Task 1** da CLA através da função

```c
CLA_forceTasks(myCLA0_BASE, CLA_TASKFLAG_1);
```

6. A CLA executa todo o controlador proporcional-ressonante, realizando:
   - cálculo do erro;
   - parcela proporcional;
   - parcela ressonante discretizada;
   - ação feed-forward da tensão da rede;
   - limitação da saída;
   - geração do sinal modulante normalizado (`d`).

7. Ao término da tarefa, a interrupção `cla1Isr1()` envia o valor calculado de `d` novamente ao computador através da SCI.

8. O PLECS recebe o sinal modulante, realiza a modulação PWM e atualiza a planta para o próximo passo de simulação.

Todo o ciclo é repetido a cada período de amostragem do controlador.

---

# Arquitetura da Implementação

```
             +----------------------------+
             |           PLECS            |
             |----------------------------|
             | Planta do Inversor         |
             | PWM                        |
             +-------------+--------------+
                           |
                    vo, io |
                           |
                     SCI (RX)
                           |
                +----------v----------+
                |        CPU1         |
                |---------------------|
                | Recebe vo e io      |
                | Atualiza iref       |
                | Dispara CLA         |
                +----------+----------+
                           |
           CpuToCla1MsgRAM |
                           |
                +----------v----------+
                |        CLA          |
                |---------------------|
                | Controlador PR      |
                | Feed-forward        |
                | Saturação           |
                +----------+----------+
                           |
           ClaToCpu1MsgRAM |
                           |
                +----------v----------+
                |        CPU1         |
                |---------------------|
                | Envia d via SCI     |
                +----------+----------+
                           |
                     SCI (TX)
                           |
                           |
                     +-----v------+
                     |   PLECS    |
                     +------------+
```

---

# Estrutura de Memória Compartilhada

A troca de informações entre CPU1 e CLA é realizada utilizando as **Message RAMs** do dispositivo.

As variáveis compartilhadas são declaradas utilizando os seguintes pragmas:

```c
#pragma DATA_SECTION(vo,"CpuToCla1MsgRAM");
float vo;

#pragma DATA_SECTION(io,"CpuToCla1MsgRAM");
float io;

#pragma DATA_SECTION(iref,"CpuToCla1MsgRAM");
float iref;

#pragma DATA_SECTION(d,"Cla1ToCpuMsgRAM");
float d;

#pragma DATA_SECTION(teste,"Cla1ToCpuMsgRAM");
float teste;
```

Assim,

- CPU → CLA
  - vo
  - io
  - iref

- CLA → CPU
  - d
  - teste

---

# Controlador PR Executado na CLA

Todo o controlador digital foi implementado dentro da função

```c
Cla1Task1()
```

A sequência de processamento consiste em:

1. cálculo do erro

```c
erro = iref - io;
```

2. parcela proporcional

```c
up = KP*erro;
```

3. parcela ressonante discretizada

```c
y_k = K1*(erro-e_k2)
    + K2*y_k1
    - y_k2;
```

4. ação ressonante

```c
ur = KI*y_k;
```

5. ação feed-forward

```c
duty = up + ur + vo;
```

6. normalização pelo barramento CC

```c
duty = duty*(1.0f/400.0f);
```

7. saturação

```c
-0.99 ≤ duty ≤ 0.99
```

Ao final da execução, o sinal modulante é armazenado na variável

```c
d
```

para posterior envio ao computador.

---

# Geração da Referência de Corrente

A referência senoidal é atualizada pela CPU1 através da função

```c
updateIref60Hz()
```

A referência possui:

- frequência da rede: **60 Hz**
- frequência de amostragem: **20 kHz**
- amplitude nominal:

```
12.8565 A
```

Também foi implementado um degrau de potência para os ensaios dinâmicos, reduzindo a amplitude da referência para metade após aproximadamente

```
70,8 ms
```

permitindo reproduzir o cenário de teste especificado no trabalho.

---

# Comunicação Serial (SCI)

A comunicação entre PLECS e TMS320F28379D utiliza o periférico SCI configurado através do SysConfig.

Configuração utilizada:

- Baud Rate: **115200 bps**
- 8 bits
- sem paridade
- 1 stop bit

O bloco C-Script do PLECS estabelece a comunicação através da porta serial utilizando

```c
CreateFile(...)
```

e transmite continuamente dois valores em ponto flutuante:

- tensão da rede;
- corrente do filtro.

Após o processamento realizado pela CLA, o microcontrolador retorna apenas um valor:

- sinal modulante (`d`).

---

# Organização das Tarefas

## CPU1

Responsável por:

- inicialização do dispositivo;
- configuração dos periféricos;
- gerenciamento da SCI;
- atualização da referência de corrente;
- troca de dados com o PLECS;
- disparo das tarefas da CLA.

---

## CLA

Responsável exclusivamente pela execução do controlador digital:

- controlador PR;
- feed-forward;
- cálculo do sinal modulante;
- limitação da saída.

Toda a estratégia de controle é executada na **Task 1**.

---

# Configuração do SysConfig

Os principais módulos utilizados são:

| Periférico | Função |
|------------|---------|
| CLA | execução do controlador PR |
| SCIA | comunicação serial com o PLECS |
| CPU Timer0 | temporização do sistema |
| MEMCFG | configuração das Message RAMs |
| CMD Tool | alocação das memórias do CLA |

As regiões de memória foram configuradas como:

- LS1 → dados do CLA
- LS4 → programa do CLA
- LS5 → programa do CLA

As Message RAMs foram configuradas para:

```
CPU → CLA

CLA → CPU
```

permitindo a troca de variáveis em tempo real entre ambos os núcleos.

---

# Considerações

Nesta implementação, o controlador digital foi executado integralmente em hardware real, enquanto a planta e o modulador PWM permaneceram simulados no ambiente PLECS.

Essa abordagem permitiu validar a implementação do controlador proporcional-ressonante no coprocessador CLA, preservando exatamente o mesmo modelo da planta utilizado nas simulações MIL e SIL. Além disso, a utilização da interface SCI tornou possível integrar o ambiente de simulação ao processador em tempo real, reproduzindo o fluxo de dados que seria encontrado em uma aplicação embarcada.