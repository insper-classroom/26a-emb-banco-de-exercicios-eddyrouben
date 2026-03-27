/**
 * Exercício: pisca-led-um-depois-o-outro
 *
 * Descrição:
 *   Dois LEDs piscam em sequência, um após o outro:
 *     - Botão amarelo (GP26): LED amarelo pisca por 1s (5Hz),
 *       depois LED azul pisca por 2s (2Hz)
 *     - Botão azul (GP19): LED azul pisca por 2s (2Hz),
 *       depois LED amarelo pisca por 1s (5Hz)
 *
 *   Os LEDs devem parar APAGADOS.
 *
 * Periféricos:
 *   - GPIO (saída para 2 LEDs, entrada para 2 botões)
 *   - Timer repetitivo (controle de frequência)
 *   - Alarm (controle de duração)
 *   - Interrupções externas (botões)
 *
 * Restrições:
 *   - NÃO usar sleep_ms(), sleep_us(), get_absolute_time()
 *   - Usar timers e interrupções
 *
 * Pinagem (do diagram.json):
 *   LED amarelo = GP10   Botão amarelo = GP26
 *   LED azul    = GP14   Botão azul    = GP19
 *
 * Frequências:
 *   LED amarelo: 5Hz → período = 200ms → timer a cada 100ms (toggle)
 *   LED azul:    2Hz → período = 500ms → timer a cada 250ms (toggle)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

/* ========== Definição dos pinos ========== */
const int LED_PIN_Y = 10;   /* LED amarelo */
const int LED_PIN_B = 14;   /* LED azul */
const int BTN_PIN_Y = 26;   /* Botão amarelo */
const int BTN_PIN_B = 19;   /* Botão azul */

/* ========== Variáveis globais volatile ========== */
/*
 * g_btn_pressed: armazena qual botão foi pressionado (pino GPIO)
 * g_timer_y: flag do timer do LED amarelo (5Hz → toggle a cada 100ms)
 * g_timer_b: flag do timer do LED azul (2Hz → toggle a cada 250ms)
 * g_alarm_fired: flag do alarm de duração (1s ou 2s)
 *
 * g_phase: controle de fase da sequência:
 *   0 = inativo (aguardando botão)
 *   1 = primeiro LED piscando
 *   2 = segundo LED piscando
 *
 * g_first_led: qual LED pisca primeiro (0=amarelo, 1=azul)
 */
volatile int g_btn_pressed = 0;
volatile int g_timer_y = 0;
volatile int g_timer_b = 0;
volatile int g_alarm_fired = 0;
volatile int g_phase = 0;
volatile int g_first_led = 0;

/**
 * ISR do botão: registra qual botão foi pressionado.
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {
        g_btn_pressed = gpio;
    }
}

/**
 * Callback do timer do LED amarelo (a cada 100ms para 5Hz).
 */
bool timer_y_callback(repeating_timer_t *rt) {
    g_timer_y = 1;
    return true;
}

/**
 * Callback do timer do LED azul (a cada 250ms para 2Hz).
 */
bool timer_b_callback(repeating_timer_t *rt) {
    g_timer_b = 1;
    return true;
}

/**
 * Callback do alarm de duração.
 * Sinaliza que o tempo do LED atual acabou.
 */
int64_t alarm_duration_callback(alarm_id_t id, void *user_data) {
    g_alarm_fired = 1;
    return 0;
}

/**
 * Função principal.
 *
 * Máquina de estados (g_phase):
 *   0 → Inativo: aguarda botão
 *   1 → Primeiro LED piscando (duração conforme a cor)
 *   2 → Segundo LED piscando (duração conforme a cor)
 *   volta para 0 quando termina
 *
 * Se botão amarelo: fase 1 = amarelo 1s (5Hz), fase 2 = azul 2s (2Hz)
 * Se botão azul:    fase 1 = azul 2s (2Hz), fase 2 = amarelo 1s (5Hz)
 */
int main() {
    stdio_init_all();

    /* === Configuração dos LEDs === */
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);

    /* === Configuração dos botões === */
    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);
    gpio_set_irq_enabled_with_callback(BTN_PIN_Y,
        GPIO_IRQ_EDGE_FALL, true, &btn_callback);

    gpio_init(BTN_PIN_B);
    gpio_set_dir(BTN_PIN_B, GPIO_IN);
    gpio_pull_up(BTN_PIN_B);
    gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_FALL, true);

    /*
     * Timer do LED amarelo: toggle a cada 100ms (período = 200ms = 5Hz).
     * Timer do LED azul: toggle a cada 250ms (período = 500ms = 2Hz).
     * Ambos rodam continuamente, mas só atualizam o LED quando g_phase > 0.
     */
    repeating_timer_t timer_y;
    add_repeating_timer_ms(100, timer_y_callback, NULL, &timer_y);

    repeating_timer_t timer_b;
    add_repeating_timer_ms(250, timer_b_callback, NULL, &timer_b);

    int led_y = 0;  /* Estado do LED amarelo */
    int led_b = 0;  /* Estado do LED azul */

    while (1) {
        /*
         * FASE 0: Aguardando botão.
         * Se um botão foi pressionado, inicia a sequência.
         */
        if (g_btn_pressed && g_phase == 0) {
            if (g_btn_pressed == BTN_PIN_Y) {
                /*
                 * Botão amarelo: fase 1 = amarelo (1s, 5Hz),
                 * depois fase 2 = azul (2s, 2Hz)
                 */
                g_first_led = 0;        /* 0 = amarelo primeiro */
                g_phase = 1;
                g_alarm_fired = 0;
                add_alarm_in_ms(1000, alarm_duration_callback, NULL, false);
            } else if (g_btn_pressed == BTN_PIN_B) {
                /*
                 * Botão azul: fase 1 = azul (2s, 2Hz),
                 * depois fase 2 = amarelo (1s, 5Hz)
                 */
                g_first_led = 1;        /* 1 = azul primeiro */
                g_phase = 1;
                g_alarm_fired = 0;
                add_alarm_in_ms(2000, alarm_duration_callback, NULL, false);
            }
            g_btn_pressed = 0;
        }

        /*
         * FASE 1: Primeiro LED piscando.
         */
        if (g_phase == 1) {
            if (g_first_led == 0) {
                /* Amarelo piscando a 5Hz */
                if (g_timer_y) {
                    g_timer_y = 0;
                    led_y = !led_y;
                    gpio_put(LED_PIN_Y, led_y);
                }
            } else {
                /* Azul piscando a 2Hz */
                if (g_timer_b) {
                    g_timer_b = 0;
                    led_b = !led_b;
                    gpio_put(LED_PIN_B, led_b);
                }
            }

            /* Alarm expirou: transição para fase 2 */
            if (g_alarm_fired) {
                g_alarm_fired = 0;
                /* Apaga o LED atual */
                gpio_put(LED_PIN_Y, 0);
                gpio_put(LED_PIN_B, 0);
                led_y = 0;
                led_b = 0;

                g_phase = 2;  /* Avança para o segundo LED */

                /* Arma alarm para o segundo LED */
                if (g_first_led == 0) {
                    /* Primeiro era amarelo → segundo é azul (2s) */
                    add_alarm_in_ms(2000, alarm_duration_callback, NULL, false);
                } else {
                    /* Primeiro era azul → segundo é amarelo (1s) */
                    add_alarm_in_ms(1000, alarm_duration_callback, NULL, false);
                }
            }
        }

        /*
         * FASE 2: Segundo LED piscando.
         */
        if (g_phase == 2) {
            if (g_first_led == 0) {
                /* Primeiro foi amarelo → agora azul pisca a 2Hz */
                if (g_timer_b) {
                    g_timer_b = 0;
                    led_b = !led_b;
                    gpio_put(LED_PIN_B, led_b);
                }
            } else {
                /* Primeiro foi azul → agora amarelo pisca a 5Hz */
                if (g_timer_y) {
                    g_timer_y = 0;
                    led_y = !led_y;
                    gpio_put(LED_PIN_Y, led_y);
                }
            }

            /* Alarm expirou: volta ao estado inativo */
            if (g_alarm_fired) {
                g_alarm_fired = 0;
                gpio_put(LED_PIN_Y, 0);
                gpio_put(LED_PIN_B, 0);
                led_y = 0;
                led_b = 0;
                g_phase = 0;  /* Volta ao estado inativo */
            }
        }
    }

    return 0;
}