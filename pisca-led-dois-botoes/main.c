/**
 * Exercício: pisca-led-dois-botoes
 *
 * Descrição:
 *   Dois LEDs (verde e amarelo) piscam com frequências e durações diferentes,
 *   ativados por botões independentes:
 *     - LED Verde: pisca a cada 200ms, ativo por 1000ms
 *     - LED Amarelo: pisca a cada 500ms, ativo por 2000ms
 *
 *   Regra especial: se os dois LEDs estiverem piscando simultaneamente
 *   e o tempo de um deles terminar, AMBOS param imediatamente.
 *
 * Periféricos:
 *   - GPIO (saída para 3 LEDs, entrada para 2 botões)
 *   - Timer repetitivo (controle da frequência de piscar)
 *   - Alarm (controle da duração de atividade)
 *   - Interrupções externas (detecção de botão)
 *
 * Regras de qualidade:
 *   - volatile em variáveis ISR→main (Rule 1.2)
 *   - ISR curta: apenas seta flags (Rules 3.0-3.3)
 *   - Sem sleep_ms/sleep_us/get_absolute_time
 */

#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

/* Pinos dos botões (entrada com pull-up) */
const int BTN_PIN_G = 28; /* Botão verde */
const int BTN_PIN_Y = 26; /* Botão amarelo */

/* Pinos dos LEDs (saída digital) */
const int LED_PIN_G = 5;  /* LED verde */
const int LED_PIN_Y = 9;  /* LED amarelo */
const int LED_PIN_R = 13; /* LED vermelho (não usado neste exercício) */

/*
 * Variáveis globais volatile - compartilhadas entre ISRs e main.
 *
 * btn_f: armazena qual botão foi pressionado (valor = pino GPIO).
 *         Setado na ISR do botão, processado no main.
 *
 * g_timer_g/y/r: flags dos timer callbacks. Quando o timer dispara,
 *                 a ISR seta a flag e o main a processa.
 *
 * g_fired_g/y: flags dos alarm callbacks. Quando o alarm expira,
 *               sinaliza que o tempo de piscar acabou.
 */
volatile int btn_f = 0;
volatile int g_timer_g = 0;
volatile int g_timer_y = 0;
volatile int g_timer_r = 0;
volatile int g_fired_g = 0;
volatile int g_fired_y = 0;

/**
 * ISR do botão: detecta qual botão foi pressionado.
 *
 * Na borda de descida (0x4 = GPIO_IRQ_EDGE_FALL): botão pressionado.
 * Armazena o número do GPIO em btn_f para o main processar.
 *
 * ISR CURTA: apenas armazena um valor, sem printf/delay/loop.
 */
void btn_callback(uint gpio, uint32_t events) {
  if (events == 0x4) {        /* Borda de descida = botão apertado */
    btn_f = gpio;             /* Salva qual botão foi pressionado */
  } else if (events == 0x8) { /* Borda de subida = botão solto */
                              /* Não faz nada ao soltar */
  }
}

/**
 * Callback do timer repetitivo do LED verde.
 * Seta a flag g_timer_g para o main alternar o LED.
 * Retorna true para continuar repetindo.
 */
bool timer_g_callback(repeating_timer_t *rt) {
  g_timer_g = 1;
  return true; // keep repeating
}

/**
 * Callback do timer repetitivo do LED amarelo.
 */
bool timer_y_callback(repeating_timer_t *rt) {
  g_timer_y = 1;
  return true; // keep repeating
}

/**
 * Callback do timer repetitivo (não usado diretamente, reservado).
 */
bool timer_r_callback(repeating_timer_t *rt) {
  g_timer_r = 1;
  return true; // keep repeating
}

/**
 * Callback do alarm do LED verde.
 * Quando o alarm de 1000ms expira, seta g_fired_g = 1
 * sinalizando que o LED verde deve parar de piscar.
 * Retorna 0 (alarm de disparo único).
 */
int64_t alarm_g_callback(alarm_id_t id, void *user_data) {
  g_fired_g = 1;
  return 0;
}

/**
 * Callback do alarm do LED amarelo (2000ms).
 */
int64_t alarm_y_callback(alarm_id_t id, void *user_data) {
  g_fired_y = 1;
  return 0;
}

/**
 * Função principal.
 *
 * Fluxo:
 * 1. Inicializa botões com IRQ e LEDs como saída.
 * 2. Cria timers repetitivos para cada LED (sempre rodando).
 * 3. No loop principal:
 *    a. Se o timer disparou E o LED está habilitado, alterna o LED.
 *    b. Se um botão foi pressionado, arma o alarm correspondente.
 *    c. Se qualquer alarm expirou, desabilita AMBOS os LEDs (regra especial).
 */
int main() {
  stdio_init_all();

  /* === Configuração dos botões (entrada com pull-up + IRQ) === */
  gpio_init(BTN_PIN_G);
  gpio_set_dir(BTN_PIN_G, GPIO_IN);
  gpio_pull_up(BTN_PIN_G);
  gpio_set_irq_enabled_with_callback(BTN_PIN_G, GPIO_IRQ_EDGE_FALL, true,
                                     &btn_callback);

  gpio_init(BTN_PIN_Y);
  gpio_set_dir(BTN_PIN_Y, GPIO_IN);
  gpio_pull_up(BTN_PIN_Y);
  /* Segunda IRQ reusa o callback já registrado */
  gpio_set_irq_enabled_with_callback(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true,
                                     &btn_callback);

  /* === Configuração dos LEDs (saída digital) === */
  gpio_init(LED_PIN_G);
  gpio_set_dir(LED_PIN_G, GPIO_OUT);

  gpio_init(LED_PIN_Y);
  gpio_set_dir(LED_PIN_Y, GPIO_OUT);

  gpio_init(LED_PIN_R);
  gpio_set_dir(LED_PIN_R, GPIO_OUT);

  /*
   * Timer repetitivo para o LED verde: dispara a cada 200ms.
   * O timer roda continuamente, mas o LED só pisca quando alarm_enable_g == 1.
   */
  repeating_timer_t timer_g;
  if (!add_repeating_timer_ms(200, timer_g_callback, NULL, &timer_g)) {
    printf("Failed to add timer\n");
  }

  /*
   * Timer repetitivo para o LED amarelo: dispara a cada 500ms.
   */
  repeating_timer_t timer_y;
  if (!add_repeating_timer_ms(500, timer_y_callback, NULL, &timer_y)) {
    printf("Failed to add timer\n");
  }

  int led_g = 0; /* Estado atual do LED verde */
  int led_y = 0; /* Estado atual do LED amarelo */

  int alarm_enable_g = 0; /* 1 = LED verde habilitado para piscar */
  int alarm_enable_y = 0; /* 1 = LED amarelo habilitado para piscar */

  alarm_id_t alarm_g = NULL; /* ID do alarm do LED verde */
  alarm_id_t alarm_y = NULL; /* ID do alarm do LED amarelo */

  while (1) {
    /*
     * LED Verde: se o timer disparou E o LED está habilitado,
     * alterna o estado. Caso contrário, mantém apagado.
     */
    if (g_timer_g && alarm_enable_g) {
      led_g = !led_g;
      gpio_put(LED_PIN_G, led_g);
      g_timer_g = 0;
    } else if (alarm_enable_g == 0) {
      gpio_put(LED_PIN_G, 0); /* LED desabilitado → apaga */
    }

    /*
     * LED Amarelo: mesma lógica do verde.
     */
    if (g_timer_y && alarm_enable_y) {
      led_y = !led_y;
      gpio_put(LED_PIN_Y, led_y);
      g_timer_y = 0;
    } else if (alarm_enable_y == 0) {
      gpio_put(LED_PIN_Y, 0);
    }

    /*
     * Processa pressionamento do botão verde.
     * Se o LED verde não está piscando, arma um alarm de 1000ms
     * e habilita o LED verde.
     */
    if (btn_f == BTN_PIN_G) {
      if (alarm_enable_g == 0) {
        alarm_g = add_alarm_in_ms(1000, alarm_g_callback, NULL, false);
        alarm_enable_g = 1;
      }
      btn_f = 0; /* Limpa a flag do botão */
    }

    /*
     * Processa pressionamento do botão amarelo.
     * Se o LED amarelo não está piscando, arma um alarm de 2000ms.
     */
    if (btn_f == BTN_PIN_Y) {
      if (alarm_enable_y == 0) {
        alarm_y = add_alarm_in_ms(2000, alarm_y_callback, NULL, false);
        alarm_enable_y = 1;
      }
      btn_f = 0;
    }

    /*
     * REGRA ESPECIAL: se QUALQUER alarm expirou,
     * desabilita AMBOS os LEDs imediatamente.
     * Cancela os alarms pendentes do outro LED também.
     * Isso garante que se dois LEDs piscam juntos e um termina,
     * o outro para junto.
     */
    if (g_fired_g == 1 || g_fired_y == 1) {
      g_fired_g = 0;
      alarm_enable_g = 0;

      g_fired_y = 0;
      alarm_enable_y = 0;

      cancel_alarm(alarm_y); /* Cancela alarm pendente do amarelo */
      cancel_alarm(alarm_g); /* Cancela alarm pendente do verde */
    }
  }

  return 0;
}
