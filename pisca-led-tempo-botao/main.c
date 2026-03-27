/**
 * Exercício: pisca-led-tempo-botao
 *
 * Descrição:
 *   O tempo que o botão vermelho fica pressionado define por quanto tempo
 *   o LED ficará piscando. Enquanto o botão está pressionado, o LED apaga.
 *   Ao soltar, o LED pisca pelo mesmo tempo que o botão foi mantido.
 *
 * Periféricos:
 *   - GPIO (saída para LED vermelho, entrada para botão vermelho)
 *   - Timer repetitivo (controle do piscar do LED)
 *   - Alarm (contagem do tempo de permanência)
 *   - Interrupções externas (detecção de pressão/soltura do botão)
 *
 * Restrições:
 *   - NÃO usar sleep_ms(), sleep_us()
 *   - Usar timer e interrupções
 *   - LED deve apagar enquanto botão pressionado
 *
 * Pinagem (do diagram.json):
 *   LED vermelho = GP5     Botão vermelho = GP28
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

/* ========== Definição dos pinos ========== */
const int LED_PIN = 5;      /* LED vermelho */
const int BTN_PIN = 28;     /* Botão vermelho */

/* ========== Variáveis globais volatile ========== */
/*
 * Todas volatile pois compartilhadas entre ISRs e main (Rule 1.2).
 *
 * g_btn_press_time: timestamp (ms desde boot) de quando o botão foi pressionado
 * g_btn_released: flag indicando que o botão foi solto
 * g_hold_duration: duração em ms que o botão foi mantido pressionado
 * g_timer_flag: flag do timer repetitivo para piscar o LED
 * g_alarm_fired: flag do alarm indicando que o tempo de piscar acabou
 * g_led_active: 1 = LED está no modo piscar, 0 = parado
 * g_btn_held: 1 = botão está sendo segurado agora
 */
volatile uint32_t g_btn_press_time = 0;
volatile int g_btn_released = 0;
volatile uint32_t g_hold_duration = 0;
volatile int g_timer_flag = 0;
volatile int g_alarm_fired = 0;
volatile int g_led_active = 0;
volatile int g_btn_held = 0;

/**
 * ISR do botão: detecta pressão (borda de descida) e soltura (borda de subida).
 *
 * Ao APERTAR (fall edge):
 *   - Salva o timestamp atual (para calcular duração)
 *   - Marca que o botão está sendo segurado
 *   - Desativa o LED (ele deve apagar enquanto pressionado)
 *
 * Ao SOLTAR (rise edge):
 *   - Calcula a duração que o botão ficou pressionado
 *   - Seta a flag g_btn_released para o main processar
 *
 * ISR CURTA: sem printf/delay/loops (Rules 3.0-3.3).
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {    /* Borda de descida = botão pressionado */
        g_btn_press_time = to_ms_since_boot(get_absolute_time());
        g_btn_held = 1;
        g_led_active = 0;   /* Desativa o piscar enquanto segurado */
    } else if (events == 0x8) {  /* Borda de subida = botão solto */
        uint32_t now = to_ms_since_boot(get_absolute_time());
        g_hold_duration = now - g_btn_press_time;
        g_btn_released = 1;
        g_btn_held = 0;
    }
}

/**
 * Callback do timer repetitivo para piscar o LED (~200ms).
 * Seta a flag apenas se o LED está ativo e o botão não está sendo segurado.
 */
bool timer_led_callback(repeating_timer_t *rt) {
    if (g_led_active && !g_btn_held) {
        g_timer_flag = 1;
    }
    return true;
}

/**
 * Callback do alarm que marca o fim do tempo de piscar.
 * A duração deste alarm é igual ao tempo que o botão ficou pressionado.
 */
int64_t alarm_stop_callback(alarm_id_t id, void *user_data) {
    g_alarm_fired = 1;
    return 0;
}

/**
 * Função principal.
 *
 * Fluxo:
 * 1. Configura GPIO e IRQ para o botão (ambas as bordas).
 * 2. Cria um timer repetitivo de 200ms para o LED.
 * 3. No loop principal:
 *    a. Se o botão foi solto (g_btn_released):
 *       - Ativa o LED (g_led_active = 1)
 *       - Arma um alarm com a duração que o botão ficou pressionado
 *    b. Se o timer do LED disparou e o LED está ativo:
 *       - Alterna o estado do LED
 *    c. Se o alarm expirou (tempo acabou):
 *       - Desativa e apaga o LED
 *    d. Se o botão está sendo segurado:
 *       - Apaga o LED imediatamente
 */
int main() {
    stdio_init_all();

    /* Configuração do LED como saída */
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* Configuração do botão como entrada com pull-up + IRQ nas duas bordas */
    gpio_init(BTN_PIN);
    gpio_set_dir(BTN_PIN, GPIO_IN);
    gpio_pull_up(BTN_PIN);
    gpio_set_irq_enabled_with_callback(BTN_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true,
        &btn_callback);

    /* Timer repetitivo para piscar o LED a cada 200ms */
    repeating_timer_t timer_led;
    if (!add_repeating_timer_ms(200, timer_led_callback, NULL, &timer_led)) {
        printf("Failed to add timer\n");
    }

    int led_state = 0;  /* Estado atual do LED (local, não precisa ser global) */

    while (true) {
        /*
         * Processa soltura do botão: inicia o piscar do LED
         * com duração proporcional ao tempo de pressão.
         */
        if (g_btn_released) {
            g_btn_released = 0;
            g_led_active = 1;
            g_alarm_fired = 0;
            led_state = 0;

            /*
             * Arma alarm com a duração que o botão ficou pressionado.
             * Ex: se segurou por 3s, o LED pisca por 3s.
             */
            if (g_hold_duration > 0) {
                add_alarm_in_ms(g_hold_duration, alarm_stop_callback, NULL, false);
            }
        }

        /*
         * Se o alarm expirou: tempo de piscar acabou.
         * Desativa e apaga o LED.
         */
        if (g_alarm_fired) {
            g_alarm_fired = 0;
            g_led_active = 0;
            led_state = 0;
            gpio_put(LED_PIN, 0);
        }

        /*
         * Se o timer disparou e o LED está ativo:
         * alterna o LED entre aceso e apagado.
         */
        if (g_timer_flag) {
            g_timer_flag = 0;
            if (g_led_active) {
                led_state = !led_state;
                gpio_put(LED_PIN, led_state);
            }
        }

        /*
         * Se o botão está sendo segurado: mantém o LED apagado.
         * Isso sobrescreve qualquer estado anterior do LED.
         */
        if (g_btn_held) {
            gpio_put(LED_PIN, 0);
        }
    }

    return 0;
}