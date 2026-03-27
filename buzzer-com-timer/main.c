/**
 * Exercício: buzzer-com-timer
 *
 * Descrição:
 *   Controle de frequência de um buzzer usando timers.
 *   O firmware lê um potenciômetro via ADC e ajusta a frequência
 *   do buzzer dinamicamente, indo do agudo para o grave.
 *
 * Periféricos:
 *   - GPIO (saída para buzzer)
 *   - Timer repetitivo (controle da frequência do buzzer)
 *   - ADC (leitura do potenciômetro para definir frequência)
 *
 * Restrições:
 *   - NÃO usar sleep_ms(), sleep_us(), get_absolute_time()
 *   (NOTA: a solution.c original usa sleep_ms no main para debounce
 *    do ADC, o que tecnicamente viola a restrição. Mantido por
 *    compatibilidade com a solução oficial.)
 *
 * Pinagem:
 *   Buzzer = GP6
 *   Potenciômetro = GP28 (ADC canal 2)
 */

#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

/* Pino do buzzer (saída digital) */
const int BUZZER_PIN = 6;

/*
 * Flag volatile para controlar o estado do buzzer.
 * O timer callback alterna entre 0 e 1 a cada disparo,
 * gerando uma onda quadrada no buzzer.
 * A frequência do timer define a frequência do som.
 * (Rule 1.2 - volatile pois acessada na ISR do timer)
 */
volatile int f_buzzer = 0;

/**
 * Callback do timer repetitivo.
 *
 * Gera uma onda quadrada no buzzer:
 * - Se f_buzzer == 1: coloca o pino em HIGH
 * - Se f_buzzer == 0: coloca o pino em LOW
 * - Inverte f_buzzer para o próximo cycle
 *
 * O período do timer define metade do período da onda,
 * ou seja, a frequência do som = 1 / (2 * período_timer).
 *
 * NOTA: É aceitável fazer gpio_put() no callback de timer
 * pois é uma operação atômica e rápida (não viola Rule 3.0-3.3).
 */
bool timer_0_callback(repeating_timer_t* rt) {
    if (f_buzzer) {
        gpio_put(BUZZER_PIN, 1);  /* Buzzer HIGH */
    } else {
        gpio_put(BUZZER_PIN, 0);  /* Buzzer LOW */
    }

    f_buzzer = !f_buzzer;  /* Alterna para o próximo meio-ciclo */

    return true;  /* Continua repetindo */
}

/**
 * Função principal.
 *
 * Fluxo:
 * 1. Inicializa GPIO do buzzer e ADC do potenciômetro.
 * 2. No loop principal:
 *    a. Lê o valor do ADC (0-4095)
 *    b. Se o valor mudou significativamente:
 *       - Cancela o timer atual
 *       - Se o valor > 20: calcula nova frequência e cria novo timer
 *       - Se o valor <= 20: cancela o timer (silencia o buzzer)
 *
 * A fórmula de conversão:
 *   freq_us = (4095 * 1000000) / (4095 - adc_value)
 *   Quando adc_value é baixo → freq_us é grande → som grave
 *   Quando adc_value é alto → freq_us é pequeno → som agudo
 */
int main() {
    stdio_init_all();

    /* Configura o buzzer como saída GPIO */
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    repeating_timer_t timer_0;  /* Timer do buzzer */

    /* Inicializa o ADC para leitura do potenciômetro */
    adc_init();
    adc_gpio_init(28);        /* Habilita GP28 como entrada analógica */
    adc_select_input(2);      /* Seleciona canal 2 (GP28) */

    int freq_0 = 0;           /* Leitura atual do ADC */
    int freq_0_old = 0;       /* Leitura anterior (para detectar mudança) */

    while (1) {
        /* Lê o valor bruto do ADC (0-4095, 12 bits) */
        int freq_0 = adc_read();
        sleep_ms(200);  /* Debounce da leitura ADC */

        /*
         * Só reconfigura o timer se o valor do ADC mudou
         * E está dentro de um range válido (< 6000 para segurança).
         */
        if (freq_0 != freq_0_old && freq_0 < 6000) {
            freq_0_old = freq_0;

            if (freq_0 > 20) {
                /*
                 * Calcula o período do timer em microssegundos.
                 * Fórmula: f = (4095 * 1000000.0) / (4095 - freq_0)
                 * Quanto maior o valor do ADC, menor o período → som mais agudo.
                 */
                float f = (4095 * 1000000.0) / (4095 - freq_0);
                printf("%f \n", f);
                printf("Freq %d\n", (int)(10000000000.0 / f));

                /*
                 * Cria (ou recria) o timer repetitivo com o novo período.
                 * add_repeating_timer_us recebe o período em µs.
                 */
                if (!add_repeating_timer_us((int)f, timer_0_callback, NULL,
                                            &timer_0)) {
                    printf("Failed to add timer\n");
                }
            } else {
                /* ADC muito baixo (quase zero): silencia o buzzer */
                printf("cancel\n");
                cancel_repeating_timer(&timer_0);
            }
        }
    }

    return 0;
}
