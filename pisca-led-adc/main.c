/**
 * Exercício: pisca-led-adc
 *
 * Descrição:
 *   LED pisca com frequência controlada por um potenciômetro lido via ADC.
 *   Três zonas de tensão definem o comportamento:
 *     - 0.0V a 1.0V: LED desligado
 *     - 1.0V a 2.0V: LED pisca a cada 300 ms
 *     - 2.0V a 3.3V: LED pisca a cada 500 ms
 *
 * Periféricos utilizados:
 *   - GPIO (saída para LED)
 *   - ADC (leitura do potenciômetro)
 *   - Timer (controle de tempo sem sleep_ms)
 *
 * Regras de qualidade:
 *   - Variáveis globais somente para ISR→main (Rule 1.1)
 *   - volatile em variáveis acessadas por ISR (Rule 1.2)
 *   - Sem delay/printf/loops dentro de ISR (Rules 3.0-3.3)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/adc.h"

/* Pino do LED azul (saída digital) */
const int PIN_LED_B = 4;

/*
 * Fator de conversão: converte o valor bruto do ADC (0–4095, 12 bits)
 * para tensão em volts (0.0–3.3V).
 * Fórmula: tensão = valor_adc * (3.3 / 4096)
 */
const float conversion_factor = 3.3f / (1 << 12);

/*
 * Flag global volatile: sinaliza do timer callback → main.
 * Marcada como volatile porque é modificada dentro da ISR do timer
 * e lida no loop principal. Sem volatile, o compilador poderia
 * otimizar a leitura e nunca perceber a mudança feita pela ISR.
 * (Rule 1.2 - variáveis acessadas por ISR devem ser volatile)
 */
volatile int f_timer_b = 0;

/**
 * Callback do timer repetitivo para o LED azul.
 *
 * Esta função é chamada pelo hardware do timer em intervalos regulares.
 * Ela apenas seta a flag f_timer_b para 1, sinalizando ao loop principal
 * que é hora de alternar o estado do LED.
 *
 * IMPORTANTE: Não fazemos gpio_put() aqui dentro para manter a ISR
 * o mais curta possível (Rule 3.0 - sem delay, Rule 3.3 - sem loops).
 *
 * Retorna true para continuar repetindo o timer.
 */
bool timer_b_callback(repeating_timer_t *rt) {
    f_timer_b = 1;
    return true; /* keep repeating - retorna true para manter o timer ativo */
}

/**
 * Função principal.
 *
 * Lógica:
 * 1. Inicializa GPIO do LED e ADC do potenciômetro.
 * 2. No loop principal, lê o ADC e determina a "zona" de tensão.
 * 3. Quando a zona muda, cancela o timer atual e cria um novo
 *    com o período correspondente à nova zona.
 * 4. Quando o timer dispara (f_timer_b == 1), alterna o LED.
 */
int main() {
    stdio_init_all();     /* Inicializa stdio (USB/UART para debug) */

    /* Configura o pino do LED como saída digital */
    gpio_init(PIN_LED_B);
    gpio_set_dir(PIN_LED_B, GPIO_OUT);

    /* Inicializa o subsistema ADC */
    adc_init();
    adc_gpio_init(28);    /* Habilita o pino GP28 como entrada analógica */
    adc_gpio_init(26);    /* Habilita o pino GP26 como entrada analógica */

    int zone_old_b = 0;   /* Zona anterior - usada para detectar mudança */
    int zone_b = 0;       /* Zona atual (0=desligado, 1=300ms, 2=500ms) */

    repeating_timer_t timer_b;  /* Estrutura do timer repetitivo */

    int led_b = 0;        /* Estado atual do LED (0=apagado, 1=aceso) */

    while (1) {
        /*
         * Verifica se o timer disparou (flag setada pelo callback).
         * Se sim, alterna o estado do LED e limpa a flag.
         */
        if (f_timer_b) {
            f_timer_b = 0;        /* Limpa a flag (reconhece o evento) */
            led_b = !led_b;       /* Inverte o estado do LED */
            gpio_put(PIN_LED_B, led_b);  /* Atualiza o pino fisicamente */
        }

        /*
         * Lê o valor do ADC no canal 2 (pino GP28) e converte para volts.
         * adc_select_input(2) seleciona o canal do potenciômetro.
         * adc_read() retorna um valor de 0 a 4095 (12 bits).
         */
        adc_select_input(2);
        float result_b = adc_read() * conversion_factor;

        /*
         * Determina a zona de tensão:
         *   Zona 0: 0.0V - 1.0V → LED desligado
         *   Zona 1: 1.0V - 2.0V → LED pisca a cada 300 ms
         *   Zona 2: 2.0V - 3.3V → LED pisca a cada 500 ms
         */
        if (result_b <= 1.0)
            zone_b = 0;
        else if (result_b <= 2.0)
            zone_b = 1;
        else
            zone_b = 2;

        /*
         * Se a zona mudou em relação à anterior, reconfigura o timer.
         * Cancela o timer antigo e cria um novo com o período adequado.
         * Isso evita que dois timers rodem simultaneamente.
         */
        if (zone_old_b != zone_b) {
            zone_old_b = zone_b;         /* Atualiza a zona anterior */
            cancel_repeating_timer(&timer_b);  /* Cancela timer ativo */
            printf("Zone: %d \n", zone_b);

            f_timer_b = 0;  /* Limpa flag pendente para evitar piscar residual */

            if (zone_b == 0)
                gpio_put(PIN_LED_B, 0);  /* Zona 0: desliga o LED */
            else if (zone_b == 1)
                /* Zona 1: cria timer de 300ms */
                add_repeating_timer_ms(300, timer_b_callback, NULL, &timer_b);
            else
                /* Zona 2: cria timer de 500ms */
                add_repeating_timer_ms(500, timer_b_callback, NULL, &timer_b);
        }
    }
}
