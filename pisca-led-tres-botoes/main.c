/**
 * Exercício: pisca-led-tres-botoes
 *
 * Descrição:
 *   Três LEDs controlados por três botões, cada um com comportamento diferente:
 *     - AMARELO (GP6): toggle ao APERTAR o botão (borda de descida)
 *     - AZUL (GP10): toggle ao SOLTAR o botão (borda de subida)
 *     - VERDE (GP14): aceso enquanto MANTIDO pressionado
 *
 *   "Ativar" = LED piscando a cada 200ms
 *   "Desativar" = LED apagado
 *   Os LEDs ativos devem piscar sincronizados (mesma frequência e fase).
 *
 * Periféricos:
 *   - GPIO (saída para 3 LEDs, entrada para 3 botões)
 *   - Interrupções externas (bordas de subida e descida)
 *
 * Restrições:
 *   - NÃO usar timers
 *   - NÃO usar gpio_get()
 *   - Usar delay de 200ms (sleep_ms) para sincronizar o piscar
 *
 * Pinagem (do diagram.json):
 *   LED amarelo = GP6    Botão amarelo = GP28
 *   LED azul    = GP10   Botão azul    = GP22
 *   LED verde   = GP14   Botão verde   = GP18
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"

/* ========== Definição dos pinos ========== */
/* LEDs (saída) */
const int LED_PIN_Y = 6;    /* LED amarelo */
const int LED_PIN_B = 10;   /* LED azul */
const int LED_PIN_G = 14;   /* LED verde */

/* Botões (entrada com pull-up) */
const int BTN_PIN_Y = 28;   /* Botão amarelo - toggle ao apertar */
const int BTN_PIN_B = 22;   /* Botão azul    - toggle ao soltar */
const int BTN_PIN_G = 18;   /* Botão verde   - ativo enquanto apertado */

/* ========== Variáveis globais volatile ========== */
/*
 * Flags que controlam se cada LED está habilitado (piscando) ou não.
 * Marcadas como volatile porque são modificadas na ISR do botão
 * e lidas no loop principal (Rule 1.2).
 *
 * g_enable_y: 1 = LED amarelo piscando. Alterna ao APERTAR o botão.
 * g_enable_b: 1 = LED azul piscando. Alterna ao SOLTAR o botão.
 * g_enable_g: 1 = LED verde piscando. Ativo enquanto botão PRESSIONADO.
 */
volatile int g_enable_y = 0;
volatile int g_enable_b = 0;
volatile int g_enable_g = 0;

/**
 * Callback de interrupção para todos os botões (ISR).
 *
 * A Pico permite apenas UM callback para todas as IRQs de GPIO,
 * então diferenciamos os botões pelo argumento 'gpio'.
 *
 * Comportamentos:
 * - Botão amarelo (GP28): Na borda de DESCIDA (0x4 = apertar),
 *   faz toggle de g_enable_y.
 * - Botão azul (GP22): Na borda de SUBIDA (0x8 = soltar),
 *   faz toggle de g_enable_b.
 * - Botão verde (GP18): Na borda de DESCIDA, liga (g_enable_g = 1).
 *                        Na borda de SUBIDA, desliga (g_enable_g = 0).
 *
 * ISR CURTA: apenas modifica flags, sem printf/delay/loops (Rules 3.0-3.3).
 */
void btn_callback(uint gpio, uint32_t events) {
    if (gpio == BTN_PIN_Y) {
        /* Botão amarelo: toggle ao APERTAR (borda de descida) */
        if (events == 0x4) {
            g_enable_y = !g_enable_y;
        }
    } else if (gpio == BTN_PIN_B) {
        /* Botão azul: toggle ao SOLTAR (borda de subida) */
        if (events == 0x8) {
            g_enable_b = !g_enable_b;
        }
    } else if (gpio == BTN_PIN_G) {
        /* Botão verde: ativo enquanto pressionado */
        if (events == 0x4) {
            g_enable_g = 1;   /* Apertou → liga */
        } else if (events == 0x8) {
            g_enable_g = 0;   /* Soltou → desliga */
        }
    }
}

/**
 * Função principal.
 *
 * Fluxo:
 * 1. Inicializa todos os GPIOs (LEDs como saída, botões como entrada com pull-up).
 * 2. Registra IRQ para todos os botões (subida + descida para verde e azul,
 *    descida para amarelo, subida para azul).
 * 3. No loop principal, a cada 200ms:
 *    - Se o LED está habilitado (flag == 1): alterna o LED (pisca).
 *    - Se o LED está desabilitado (flag == 0): apaga o LED.
 *
 * O sleep_ms(200) no loop garante que todos os LEDs piscam sincronizados
 * na mesma frequência de 200ms (5Hz).
 */
int main() {
    stdio_init_all();

    /* === Inicialização dos LEDs (saída digital) === */
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);

    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);

    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);

    /* === Inicialização dos botões (entrada com pull-up + IRQ) === */

    /* Botão amarelo: IRQ na borda de descida (toggle ao apertar) */
    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);
    gpio_set_irq_enabled_with_callback(BTN_PIN_Y,
        GPIO_IRQ_EDGE_FALL,
        true,
        &btn_callback);

    /* Botão azul: IRQ na borda de subida (toggle ao soltar) */
    gpio_init(BTN_PIN_B);
    gpio_set_dir(BTN_PIN_B, GPIO_IN);
    gpio_pull_up(BTN_PIN_B);
    gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_RISE, true);

    /* Botão verde: IRQ nas DUAS bordas (ativo enquanto pressionado) */
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);
    gpio_set_irq_enabled(BTN_PIN_G,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true);

    /*
     * Variável local para alternar o estado dos LEDs a cada ciclo.
     * Todos os LEDs ativos compartilham este toggle para ficarem
     * sincronizados (piscam juntos).
     */
    int led_toggle = 0;

    while (true) {
        /*
         * A cada 200ms, alterna o estado do toggle.
         * LEDs habilitados irão piscar; LEDs desabilitados ficam apagados.
         */
        led_toggle = !led_toggle;

        /* LED amarelo: pisca se habilitado, apaga se desabilitado */
        if (g_enable_y) {
            gpio_put(LED_PIN_Y, led_toggle);
        } else {
            gpio_put(LED_PIN_Y, 0);
        }

        /* LED azul: pisca se habilitado, apaga se desabilitado */
        if (g_enable_b) {
            gpio_put(LED_PIN_B, led_toggle);
        } else {
            gpio_put(LED_PIN_B, 0);
        }

        /* LED verde: pisca se habilitado, apaga se desabilitado */
        if (g_enable_g) {
            gpio_put(LED_PIN_G, led_toggle);
        } else {
            gpio_put(LED_PIN_G, 0);
        }

        /*
         * Delay de 200ms: define o período de piscar dos LEDs.
         * Todos os LEDs compartilham o mesmo ciclo, garantindo sincronismo.
         * O uso de sleep_ms é permitido aqui (estamos no main, não na ISR).
         */
        sleep_ms(200);
    }
}