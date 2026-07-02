# Implementação Hardware-in-the-Loop (HIL) no TMS320F28379D

Este projeto implementa uma estratégia **Hardware-in-the-Loop (HIL)** para um inversor monofásico conectado à rede elétrica utilizando o microcontrolador **TMS320F28379D**.

Nesta implementação, tanto a **planta do conversor** quanto o **controlador proporcional-ressonante (PR)** são executados diretamente no microcontrolador. A divisão das tarefas foi realizada conforme especificado no Trabalho 2:

- **CPU1:** execução da planta discretizada;
- **CLA:** execução do controlador PR;
- **ePWM:** geração dos pulsos PWM do inversor.

Além disso, foi utilizada uma interface interna baseada nos módulos **DAC** e **ADC** para reproduzir o fluxo de sinais existente em um sistema real, onde o controlador recebe apenas medições provenientes de sensores.

---

# Funcionamento da Implementação

O fluxo completo da implementação é apresentado abaixo.

1. O módulo **ePWM** gera os sinais PWM responsáveis pelo acionamento das quatro chaves do inversor.

2. A **CPU1** lê o estado instantâneo dessas quatro chaves através de pinos GPIO.

3. Utilizando esses estados, a CPU1 executa a planta discretizada do inversor, calculando:
   - corrente do filtro (`iL`);
   - tensão da rede (`vg`).

4. As duas variáveis são convertidas para a faixa dos conversores D/A através da aplicação de um offset correspondente à metade da escala do DAC.

5. Os valores são enviados para dois canais DAC internos:
   - DACA → tensão da rede;
   - DACB → corrente do filtro.

6. As saídas dos DACs são conectadas às entradas do ADC do próprio microcontrolador.

7. A cada interrupção do ADC, a CLA lê as duas grandezas medidas e executa o controlador proporcional-ressonante.

8. O controlador calcula o índice de modulação e atualiza os registradores Compare dos módulos ePWM.

9. Os novos sinais PWM passam a acionar novamente a planta embarcada, fechando completamente a malha de controle dentro do próprio microcontrolador.

Todo esse processo ocorre continuamente durante a execução do sistema.

---

# Arquitetura da Implementação

```
                 +----------------------+
                 |       CPU1           |
                 |----------------------|
                 | Planta Discretizada  |
                 | Rede 60 Hz           |
                 +----------+-----------+
                            |
                      io      vo
                            |
                      DACA / DACB
                            |
                 (Ligação Analógica)
                            |
                      ADCINA2 / ADCINA3
                            |
                 +----------v-----------+
                 |         CLA          |
                 |----------------------|
                 | Controlador PR       |
                 | Feed-forward         |
                 | Referência           |
                 +----------+-----------+
                            |
                      Duty Cycle
                            |
                      ePWM1 / ePWM2
                            |
                 +----------v-----------+
                 |      Ponte H         |
                 +----------+-----------+
                            |
                     GPIO (S1...S4)
                            |
                            |
                 +----------v-----------+
                 |        CPU1          |
                 +----------------------+
```

---

# Divisão das Tarefas

## CPU1

A CPU1 foi responsável por:

- execução da planta discretizada;
- geração da tensão da rede;
- leitura dos estados das chaves do inversor;
- cálculo da corrente do filtro;
- atualização dos DACs;
- armazenamento das formas de onda;
- sincronização da execução da CLA.

Toda a dinâmica da planta foi implementada utilizando exatamente a discretização desenvolvida no Trabalho 1.

---

## CLA

Toda a estratégia de controle foi implementada na **Task 1** da CLA.

O controlador executa:

- leitura das variáveis provenientes do ADC;
- geração da referência senoidal;
- cálculo do erro;
- parcela proporcional;
- parcela ressonante discretizada;
- ação feed-forward;
- saturação do índice de modulação;
- atualização dos módulos ePWM.

Dessa forma, toda a malha de corrente é executada exclusivamente pelo coprocessador.

---

# Interface CPU-CLA

A comunicação entre CPU1 e CLA utiliza as Message RAMs configuradas pelo SysConfig.

Neste projeto apenas a variável

```c
theta_grid
```

é compartilhada entre ambos os núcleos.

```c
#pragma DATA_SECTION(theta_grid,"CpuToCla1MsgRAM")
float theta_grid;
```

A CPU1 atualiza continuamente o ângulo da tensão da rede e a CLA utiliza esse valor para gerar a referência senoidal sincronizada da corrente.

---

# Modelo da Planta

A planta implementada corresponde exatamente ao modelo discreto desenvolvido no Trabalho 1.

A dinâmica da corrente é dada por

\[
i[k]=Ai[k-1]+K(S[k]+S[k-1])-B(v_g[k]+v_g[k-1])
\]

onde

- \(S\) representa o estado de chaveamento da ponte;
- \(v_g\) representa a tensão da rede;
- \(i\) representa a corrente do filtro.

Os coeficientes discretos são calculados a partir dos parâmetros físicos do filtro RL utilizando a discretização de Tustin.

---

# Frequências de Execução

Foram utilizadas duas frequências distintas.

## Planta

A planta foi executada a

```
200 kHz
```

correspondendo a um passo de simulação de

```
5 μs
```

Essa frequência foi escolhida para fornecer **10 pontos de integração para cada período de controle**, aumentando a fidelidade numérica da simulação embarcada.

Além disso, considerando o clock de 200 MHz do TMS320F28379D, cada passo da planta dispõe de aproximadamente

```
1000 ciclos de clock
```

A implementação desenvolvida utiliza aproximadamente

```
132 ciclos
```

restando ampla margem para execução em tempo real.

---

## Controlador

O controlador PR foi executado a

```
20 kHz
```

utilizando um decimador por software implementado na interrupção do ADC.

Assim, a CLA é executada apenas a cada 10 passos da planta.

---

# Interface DAC/ADC

Para reproduzir o funcionamento de um sistema físico, as variáveis simuladas são convertidas para sinais analógicos internos.

## DAC

Dois canais DAC são utilizados.

| DAC | Variável |
|------|----------|
| DACA | tensão da rede |
| DACB | corrente do filtro |

As variáveis recebem offset igual à metade da escala do DAC.

---

## ADC

Os mesmos sinais são novamente adquiridos pela CLA através do ADC.

| Canal | Variável |
|--------|----------|
| ADCINA2 | tensão da rede |
| ADCINA3 | corrente do filtro |

Dessa forma, a CLA recebe exatamente o mesmo tipo de informação que receberia caso existissem sensores físicos conectados ao conversor.

---

# Geração PWM

A geração PWM foi realizada integralmente pelos módulos ePWM do TMS320F28379D.

Características utilizadas:

- modo Up-Down;
- frequência de chaveamento de 20 kHz;
- sinais complementares;
- inserção de dead-time;
- atualização direta dos registradores Compare.

A CLA calcula o índice de modulação e converte esse valor para os registradores CMPA dos módulos:

- ePWM1 → S1/S2
- ePWM2 → S3/S4

---

# Configuração dos Periféricos

Os principais módulos utilizados foram:

| Periférico | Função |
|------------|---------|
| CPU1 | planta discretizada |
| CLA | controlador PR |
| ADC | aquisição das variáveis simuladas |
| DAC | exportação da corrente e tensão |
| ePWM | geração dos pulsos PWM |
| GPIO | leitura das chaves do inversor |
| CPU Timer | disparo das conversões ADC |
| MEMCFG | configuração das Message RAMs |

---

# Buffers de Aquisição

Para facilitar a análise das formas de onda, foram implementados buffers internos para armazenamento das principais variáveis do sistema.

São registrados:

- corrente do filtro;
- tensão da rede;
- estado de chaveamento;
- estados individuais das chaves S1 e S3.

Os buffers permitem visualizar posteriormente o comportamento temporal da implementação HIL.

---

# Considerações

Nesta implementação toda a malha de controle foi executada integralmente no microcontrolador TMS320F28379D, eliminando completamente a necessidade de comunicação com o computador durante a simulação.

A CPU1 foi responsável pela execução da planta discretizada, enquanto a CLA executou exclusivamente o controlador proporcional-ressonante. A utilização dos módulos DAC e ADC permitiu reproduzir o fluxo de sinais presente em um sistema físico, enquanto os módulos ePWM geraram diretamente os pulsos de acionamento do inversor.

Essa arquitetura atende integralmente aos requisitos propostos para a implementação Hardware-in-the-Loop do Trabalho 2, preservando as mesmas frequências de chaveamento e de amostragem utilizadas nas implementações MIL, SIL e PIL.