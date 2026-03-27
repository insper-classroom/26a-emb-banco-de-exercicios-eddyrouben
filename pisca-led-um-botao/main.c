/**
 * Exercício: pisca-led-um-botao
 *
 * Descrição:
 *   Ao pressionar o botão, dois LEDs começam a piscar por 5 segundos:
 *     - LED amarelo: pisca a cada 500ms
 *     - LED azul: pisca a cada 150ms
 *   Após 5 segundos, ambos param automaticamente (apagados).
 *
 * Periféricos:
 *   - GPIO (saída para 2 LEDs, entrada para 1 botão)
 *   - Timer (3 instâncias independentes):
 *     1. Timer para o LED amarelo (500ms)
 *     2. Timer para o LED azul (150ms)
 *     3. Alarm para contar 5 segundos
 *   - Interrupções externas (botão)
 *
 * Restrições:
 *   - NÃO usar sleep_ms(), sleep_us(), get_absolute_time()
 *   - Usar exatamente 3 timers independentes
 *   - LEDs devem parar APAGADOS
 *
 * Pinagem (do diagram.json):
 *   LED amarelo = GP5    LED azul = GP9    Botão = GP28
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

/* ========== Definição dos pinos ========== */
const int LED_PIN_Y = 5;    /* LED amarelo */
const int LED_PIN_B = 9;    /* LED azul */
const int BTN_PIN = 28;     /* Botão (verde no diagrama) */

/* ========== Variáveis globais volatile ========== */
/*
 * Todas volatile pois são modificadas em ISRs (callbacks de timer/alarm/GPIO)
 * e lidas no loop principal (Rule 1.2).
 *
 * g_btn_pressed: flag do botão pressionado
 * g_timer_y: flag do timer do LED amarelo
 * g_timer_b: flag do timer do LED azul
 * g_alarm_fired: flag do alarm de 5 segundos
 * g_leds_active: controle de estado - 1 = LEDs piscando, 0 = parados
 */
volatile int g_btn_pressed = 0;
volatile int g_timer_y = 0;
volatile int g_timer_b = 0;
volatile int g_alarm_fired = 0;
volatile int g_leds_active = 0;

/**
 * ISR do botão: seta flag quando pressionado (borda de descida).
 * ISR CURTA: apenas seta uma flag (Rules 3.0-3.3).
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {           /* Borda de descida = apertar */
        g_btn_pressed = 1;
    }
}

/**
 * Callback do timer repetitivo do LED amarelo (500ms).
 * Seta flag apenas se os LEDs estão ativos.
 * Retorna true para continuar repetindo.
 */
bool timer_y_callback(repeating_timer_t *rt) {
    if (g_leds_active) {
        g_timer_y = 1;
    }
    return true;
}

/**
 * Callback do timer repetitivo do LED azul (150ms).
 */
bool timer_b_callback(repeating_timer_t *rt) {
    if (g_leds_active) {
        g_timer_b = 1;
    }
    return true;
}

/**
 * Callback do alarm de 5 segundos.
 * Sinaliza que o tempo acabou e os LEDs devem parar.
 * Retorna 0 (alarm de disparo único, não repete).
 */
int64_t alarm_5s_callback(alarm_id_t id, void *user_data) {
    g_alarm_fired = 1;
    return 0;
}

/**
 * Função principal.
 *
 * Fluxo:
 * 1. Inicializa GPIOs e cria os dois timers repetitivos (amarelo e azul).
 * 2. No loop principal:
 *    a. Se o botão foi pressionado e os LEDs estão inativos:
 *       - Ativa os LEDs (g_leds_active = 1)
 *       - Arma o alarm de 5 segundos
 *    b. Se os LEDs estão ativos e o timer disparou:
 *       - Alterna o LED correspondente
 *    c. Se o alarm de 5s expirou:
 *       - Desativa os LEDs e os apaga
 */
int main() {
    stdio_init_all();

    /* === Configuração do botão === */
    gpio_init(BTN_PIN);
    gpio_set_dir(BTN_PIN, GPIO_IN);
    gpio_pull_up(BTN_PIN);
    gpio_set_irq_enabled_with_callback(BTN_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        &btn_callback);

    /* === Configuração dos LEDs === */
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);

    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);

    /* === Timer 1: LED amarelo pisca a cada 500ms === */
    repeating_timer_t timer_y;
    if (!add_repeating_timer_ms(500, timer_y_callback, NULL, &timer_y)) {
        printf("Failed to add timer Y\n");
    }

    /* === Timer 2: LED azul pisca a cada 150ms === */
    repeating_timer_t timer_b;
    if (!add_repeating_timer_ms(150, timer_b_callback, NULL, &timer_b)) {
        printf("Failed to add timer B\n");
    }

    int led_y = 0;  /* Estado atual do LED amarelo */
    int led_b = 0;  /* Estado atual do LED azul */

    while (true) {
        /*
         * Se o botão foi pressionado e os LEDs não estão ativos:
         * - Ativa os LEDs
         * - Arma o alarm de 5 segundos (Timer 3)
         */
        if (g_btn_pressed) {
            g_btn_pressed = 0;
            if (!g_leds_active) {
                g_leds_active = 1;
                g_alarm_fired = 0;
                /* Timer 3: alarm de disparo único em 5000ms */
                add_alarm_in_ms(5000, alarm_5s_callback, NULL, false);
            }
        }

        /*
         * Se o alarm de 5 segundos expirou:
         * Para os LEDs e os apaga (devem terminar apagados).
         */
        if (g_alarm_fired) {
            g_alarm_fired = 0;
            g_leds_active = 0;
            led_y = 0;
            led_b = 0;
            gpio_put(LED_PIN_Y, 0);  /* Apaga LED amarelo */
            gpio_put(LED_PIN_B, 0);  /* Apaga LED azul */
        }

        /*
         * LED amarelo: alterna quando o timer de 500ms dispara
         * e os LEDs estão ativos.
         */
        if (g_timer_y) {
            g_timer_y = 0;
            if (g_leds_active) {
                led_y = !led_y;
                gpio_put(LED_PIN_Y, led_y);
            }
        }

        /*
         * LED azul: alterna quando o timer de 150ms dispara
         * e os LEDs estão ativos.
         */
        if (g_timer_b) {
            g_timer_b = 0;
            if (g_leds_active) {
                led_b = !led_b;
                gpio_put(LED_PIN_B, led_b);
            }
        }
    }
}
