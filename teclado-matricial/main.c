/**
 * Exercício: teclado-matricial
 *
 * Descrição:
 *   Leitura de um teclado matricial 3x2 (3 linhas, 2 colunas).
 *   O firmware faz varredura por colunas e detecta qual tecla
 *   foi pressionada usando interrupções nas linhas.
 *
 * Funcionamento do teclado matricial:
 *   Em vez de usar 1 pino por botão (6 pinos para 6 teclas),
 *   organizamos em uma matriz de linhas × colunas:
 *     - 3 linhas + 2 colunas = 5 pinos para 6 teclas
 *
 *   Varredura:
 *   1. Coloca uma coluna em LOW, a outra em HIGH
 *   2. Verifica qual linha detectou borda de descida (IRQ)
 *   3. O cruzamento linha×coluna identifica a tecla
 *   4. Alterna as colunas e repete
 *
 *   Layout do teclado:
 *        C1(GP14)  C2(GP15)
 *   L1(GP3)   [1]      [2]
 *   L2(GP8)   [3]      [4]
 *   L3(GP13)  [5]      [6]
 *
 * Periféricos:
 *   - GPIO (saída para colunas, entrada com pull-up e IRQ para linhas)
 *   - Interrupções externas (detecção de tecla nas linhas)
 *
 * Restrições:
 *   - NÃO usar gpio_get()
 *   - Usar interrupções
 *
 * Pinagem (diagram.json):
 *   Linhas: GP3, GP8, GP13 (entrada com pull-up)
 *   Colunas: GP14, GP15 (saída)
 */

#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/* Pinos das linhas (entrada - detectam pressionamento via IRQ) */
const int LINE_1 = 3;
const int LINE_2 = 8;
const int LINE_3 = 13;

/* Pinos das colunas (saída - controlam a varredura) */
const int COL_1 = 14;
const int COL_2 = 15;

/*
 * Flag volatile do botão pressionado.
 * Armazena o GPIO da linha que detectou a tecla.
 * Na borda de descida: salva qual linha foi ativada.
 * Na borda de subida: limpa a flag (botão solto).
 * (Rule 1.2 - volatile pois modificada na ISR)
 */
volatile int btn_f = 0;

/**
 * Callback de interrupção para as linhas do teclado (ISR).
 *
 * Quando uma tecla é pressionada, ela conecta eletricamente
 * a coluna ativa (LOW) à linha correspondente, puxando a
 * linha de HIGH (pull-up) para LOW → gera borda de descida.
 *
 * Na borda de descida (0x4): salva o GPIO da linha.
 * Na borda de subida (0x8): limpa a flag (tecla solta).
 *
 * ISR CURTA (Rules 3.0-3.3).
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {       /* Borda de descida = tecla pressionada */
        btn_f = gpio;          /* Salva qual linha detectou */
    } else if (events == 0x8) { /* Borda de subida = tecla solta */
        btn_f = 0;             /* Limpa a flag */
    }
}

/**
 * Função principal.
 *
 * Fluxo de varredura:
 * 1. Coloca COL_1 em LOW e COL_2 em HIGH
 *    - Se btn_f != 0: tecla na coluna 1 foi pressionada
 *    - Identifica pela linha: L1→Btn2, L2→Btn4, L3→Btn6
 * 2. Coloca COL_1 em HIGH e COL_2 em LOW
 *    - Se btn_f != 0: tecla na coluna 2 foi pressionada
 *    - Identifica pela linha: L1→Btn1, L2→Btn3, L3→Btn5
 * 3. Repete continuamente
 */
int main() {
    stdio_init_all();

    /* === Configura as 3 linhas (entrada com pull-up + IRQ) === */

    /* Linha 1 (GP3) - registra callback (primeiro pino a chamar _with_callback) */
    gpio_init(LINE_1);
    gpio_set_dir(LINE_1, GPIO_IN);
    gpio_pull_up(LINE_1);
    gpio_set_irq_enabled_with_callback(LINE_1,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    /* Linha 2 (GP8) - reusa callback já registrado */
    gpio_init(LINE_2);
    gpio_set_dir(LINE_2, GPIO_IN);
    gpio_pull_up(LINE_2);
    gpio_set_irq_enabled_with_callback(LINE_2,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    /* Linha 3 (GP13) - reusa callback */
    gpio_init(LINE_3);
    gpio_set_dir(LINE_3, GPIO_IN);
    gpio_pull_up(LINE_3);
    gpio_set_irq_enabled_with_callback(LINE_3,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    /* === Configura as 2 colunas (saída) === */
    gpio_init(COL_1);
    gpio_set_dir(COL_1, GPIO_OUT);
    gpio_init(COL_2);
    gpio_set_dir(COL_2, GPIO_OUT);

    while (true) {
        /*
         * VARREDURA COLUNA 1:
         * COL_1 = LOW (ativa), COL_2 = HIGH (desativada)
         * Se uma tecla da coluna 1 for pressionada,
         * a linha correspondente vai de HIGH para LOW → IRQ.
         */
        gpio_put(COL_1, 0);
        gpio_put(COL_2, 1);
        sleep_ms(10);  /* Tempo para estabilizar e detectar IRQ */

        if (btn_f != 0) {
            /* Identifica a tecla pela linha ativada */
            if (btn_f == 3)         /* Linha 1 + Coluna 1 */
                printf("Btn: 2\n");
            else if (btn_f == 8)    /* Linha 2 + Coluna 1 */
                printf("Btn: 4\n");
            else                    /* Linha 3 + Coluna 1 */
                printf("Btn: 6\n");

            /* Espera a tecla ser solta (btn_f volta a 0 na borda de subida) */
            while (btn_f != 0) {}
        }

        /*
         * VARREDURA COLUNA 2:
         * COL_1 = HIGH (desativada), COL_2 = LOW (ativa)
         */
        gpio_put(COL_1, 1);
        gpio_put(COL_2, 0);
        sleep_ms(10);

        if (btn_f != 0) {
            if (btn_f == 3)         /* Linha 1 + Coluna 2 */
                printf("Btn: 1\n");
            else if (btn_f == 8)    /* Linha 2 + Coluna 2 */
                printf("Btn: 3\n");
            else                    /* Linha 3 + Coluna 2 */
                printf("Btn: 5\n");

            while (btn_f != 0) {}
        }
    }
}
