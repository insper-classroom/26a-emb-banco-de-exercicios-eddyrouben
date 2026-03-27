/**
 * Exercício: barra-de-leds
 *
 * Descrição:
 *   Controle de uma barra de 5 LEDs com contador.
 *   Uma chave (slide switch) define o modo:
 *     - SW = 0 (nível baixo) → incrementar
 *     - SW = 1 (nível alto) → decrementar
 *   Cada pressionamento do botão verde atualiza o contador
 *   e exibe o novo valor na barra de LEDs.
 *
 * Funções obrigatórias:
 *   - bar_init(): inicializa os pinos da barra de LEDs
 *   - bar_display(int val): exibe um valor (0-5) na barra
 *
 * Periféricos:
 *   - GPIO (saída para 5 LEDs da barra, entrada para botão e chave)
 *   - Interrupções externas (botão e chave)
 *
 * Restrições:
 *   - NÃO usar gpio_get()
 *   - Usar interrupções nos botões/chave
 *
 * Pinagem (do diagram.json):
 *   Barra de LEDs: GP2, GP3, GP4, GP5, GP6 (5 LEDs)
 *   Botão verde = GP22
 *   Chave (slide switch) = GP28
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"

/* ========== Definição dos pinos ========== */
/* Primeiro GPIO da barra de LEDs */
#define FIRST_LED_GPIO 2
/* Número de LEDs na barra */
#define NUM_LEDS 5

/* Pino do botão verde (incrementa/decrementa) */
const int BTN_PIN = 22;
/* Pino da chave slide switch (define modo inc/dec) */
const int SW_PIN = 28;

/* ========== Variáveis globais volatile ========== */
/*
 * g_btn_pressed: flag de botão pressionado (ISR → main) (Rule 1.2)
 * g_sw_state: estado da chave: 0 = incrementar, 1 = decrementar
 *             Atualizado na ISR quando a chave muda de posição.
 *             Usamos a flag para evitar gpio_get().
 */
volatile int g_btn_pressed = 0;
volatile int g_sw_state = 0;

/**
 * Inicializa os pinos da barra de LEDs como saída digital.
 * Os LEDs estão conectados nos GPIOs consecutivos:
 * GP2, GP3, GP4, GP5, GP6
 */
void bar_init(void) {
    for (int i = 0; i < NUM_LEDS; i++) {
        gpio_init(FIRST_LED_GPIO + i);
        gpio_set_dir(FIRST_LED_GPIO + i, GPIO_OUT);
        gpio_put(FIRST_LED_GPIO + i, 0);  /* Inicia apagado */
    }
}

/**
 * Exibe um valor (0 a 5) na barra de LEDs.
 *
 * val = 0: todos apagados
 * val = 1: LED 1 aceso
 * val = 2: LEDs 1-2 acesos
 * val = 5: todos acesos
 *
 * Os LEDs são preenchidos da esquerda para a direita,
 * como uma barra de progresso.
 *
 * @param val Valor de 0 a NUM_LEDS (5)
 */
void bar_display(int val) {
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < val) {
            gpio_put(FIRST_LED_GPIO + i, 1);  /* Acende */
        } else {
            gpio_put(FIRST_LED_GPIO + i, 0);  /* Apaga */
        }
    }
}

/**
 * Callback de interrupção unificado para botão e chave (ISR).
 *
 * - Se o GPIO é o botão (GP22):
 *   Na borda de descida → seta flag g_btn_pressed.
 *
 * - Se o GPIO é a chave (GP28):
 *   Na borda de descida (SW=0) → modo incrementar.
 *   Na borda de subida (SW=1) → modo decrementar.
 *
 * ISR CURTA: apenas modifica flags (Rules 3.0-3.3).
 */
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BTN_PIN) {
        if (events == 0x4) {   /* Borda de descida = botão pressionado */
            g_btn_pressed = 1;
        }
    } else if (gpio == SW_PIN) {
        if (events == 0x4) {   /* Borda de descida = chave em LOW */
            g_sw_state = 0;    /* Modo: incrementar */
        } else if (events == 0x8) {  /* Borda de subida = chave em HIGH */
            g_sw_state = 1;    /* Modo: decrementar */
        }
    }
}

/**
 * Função principal.
 *
 * Fluxo:
 * 1. Inicializa barra de LEDs, botão e chave.
 * 2. No loop principal, quando o botão é pressionado:
 *    - Se SW=0: incrementa o contador (máx 5)
 *    - Se SW=1: decrementa o contador (mín 0)
 *    - Atualiza a barra de LEDs
 */
int main() {
    stdio_init_all();

    /* Inicializa a barra de LEDs (5 pinos como saída) */
    bar_init();

    /* === Configuração do botão (GP22) === */
    gpio_init(BTN_PIN);
    gpio_set_dir(BTN_PIN, GPIO_IN);
    gpio_pull_up(BTN_PIN);
    gpio_set_irq_enabled_with_callback(BTN_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_callback);

    /*
     * === Configuração da chave slide switch (GP28) ===
     * Monitora ambas as bordas para detectar mudança de posição.
     * A chave conecta o pino ao GND quando ativada.
     */
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);
    gpio_set_irq_enabled(SW_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true);

    /* Contador da barra: varia de 0 a NUM_LEDS (5) */
    int counter = 0;
    bar_display(counter);  /* Exibe valor inicial (0 = tudo apagado) */

    while (true) {
        /*
         * Quando o botão é pressionado:
         * - Verifica o estado da chave (g_sw_state)
         * - Incrementa ou decrementa o contador
         * - Limita entre 0 e NUM_LEDS
         * - Atualiza a barra de LEDs
         */
        if (g_btn_pressed) {
            g_btn_pressed = 0;

            if (g_sw_state == 0) {
                /* Chave em LOW: INCREMENTAR */
                if (counter < NUM_LEDS) {
                    counter++;
                }
            } else {
                /* Chave em HIGH: DECREMENTAR */
                if (counter > 0) {
                    counter--;
                }
            }

            bar_display(counter);
        }
    }
}
