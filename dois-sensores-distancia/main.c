/**
 * Exercício: dois-sensores-distancia
 *
 * Descrição:
 *   Leitura de dois sensores ultrassônicos SR-04 simultaneamente.
 *   Imprime a distância medida por cada sensor no terminal.
 *   Detecta e reporta falhas quando um sensor não responde.
 *
 * Funcionamento do HC-SR04:
 *   1. Envia um pulso de 10µs no pino TRIGGER.
 *   2. O sensor emite ondas ultrassônicas e espera o eco.
 *   3. O pino ECHO fica em HIGH durante o tempo proporcional à distância.
 *   4. distância = (tempo_echo_us * 0.0343) / 2  (em cm)
 *      - 0.0343 cm/µs é a velocidade do som
 *      - Divide por 2 porque o som vai e volta
 *
 * Periféricos:
 *   - GPIO (trigger como saída, echo como entrada com IRQ)
 *   - Timer/Alarm (timeout para detectar falha de sensor)
 *   - UART (printf para imprimir distâncias)
 *
 * Regras de qualidade:
 *   - volatile em variáveis acessadas pela ISR (Rule 1.2)
 *   - ISR curta: apenas salva timestamps e cancela alarm (Rules 3.0-3.3)
 *   - Sem gpio_get() - usa interrupções nas bordas do ECHO
 */

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

/* Pinos do Sensor 1 */
const int ECHO_PIN_1 = 12;    /* Entrada: recebe o pulso de eco */
const int TRIGGER_PIN_1 = 13; /* Saída: envia o pulso de trigger */

/*
 * Variáveis volatile para o Sensor 1.
 * Compartilhadas entre ISR e main (Rule 1.1 e 1.2).
 * - alarm_1: flag de timeout (sensor não respondeu)
 * - echo_start_time_1: timestamp da borda de subida do echo
 * - echo_end_time_1: timestamp da borda de descida do echo
 */
volatile int alarm_1 = 0;
volatile int echo_start_time_1 = 0;
volatile int echo_end_time_1 = 0;

/* Pinos do Sensor 2 */
const int ECHO_PIN_2 = 18;    /* Entrada: recebe o pulso de eco */
const int TRIGGER_PIN_2 = 19; /* Saída: envia o pulso de trigger */

/* Variáveis volatile para o Sensor 2 (mesma lógica do Sensor 1) */
volatile int alarm_2 = 0;
volatile int echo_start_time_2 = 0;
volatile int echo_end_time_2 = 0;

/*
 * IDs dos alarmes de timeout.
 * Usados para cancelar o alarme quando o echo é recebido a tempo.
 * Marcados como volatile pois são escritos no main e lidos/cancelados na ISR.
 */
volatile alarm_id_t echo_timeout_alarm_1;
volatile alarm_id_t echo_timeout_alarm_2;

/**
 * Calcula a distância em cm a partir da duração do pulso echo.
 *
 * @param duracao_us Duração do pulso echo em microssegundos
 * @return Distância em centímetros (float)
 *
 * Fórmula: distância = (duração × velocidade_som) / 2
 * Velocidade do som = 343 m/s = 0.0343 cm/µs
 */
float calcula_distancia_cm(uint64_t duracao_us) {
  // distancia = (duracao * velocidade_som) / 2
  // v_som = 343 m/s = 34300 cm / 1000000 us = 0.0343 cm/us
  return (duracao_us * 0.0343) / 2.0;
}

/**
 * Callback de timeout do Sensor 1.
 * Chamado pelo alarm se o echo não for recebido em 20ms.
 * Seta a flag alarm_1 para indicar falha na leitura.
 * Retorna 0 (alarm de disparo único, não repete).
 */
int64_t echo_timeout_callback_1(alarm_id_t id, void *user_data) {
  alarm_1 = 1;
  return 0;
}

/**
 * Callback de timeout do Sensor 2 (mesma lógica do Sensor 1).
 */
int64_t echo_timeout_callback_2(alarm_id_t id, void *user_data) {
  alarm_2 = 1;
  return 0;
}

/**
 * ISR (Interrupt Service Routine) para os pinos ECHO dos dois sensores.
 *
 * Um único callback trata ambos os sensores, diferenciando pelo
 * argumento gpio (qual pino gerou a interrupção).
 *
 * Para cada sensor:
 * - Borda de SUBIDA (RISE): o echo começou → salva o timestamp inicial
 * - Borda de DESCIDA (FALL): o echo terminou → salva o timestamp final
 *   e cancela o alarm de timeout (o sensor respondeu a tempo)
 *
 * A diferença (end - start) dá a duração do pulso em µs.
 *
 * IMPORTANTE: A ISR é curta - apenas salva valores e cancela alarm.
 * Não faz cálculos, printf ou delays (Rules 3.0-3.3).
 */
void echo_isr(uint gpio, uint32_t events) {
  /* Trata eventos do Sensor 1 */
  if (gpio == ECHO_PIN_1) {
    if (events & GPIO_IRQ_EDGE_RISE) {
      /* Echo começou: marca o instante de início */
      echo_start_time_1 = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
      /* Echo terminou: marca o instante de fim */
      echo_end_time_1 = get_absolute_time();
      /* Cancela o timeout - o sensor respondeu com sucesso */
      cancel_alarm(echo_timeout_alarm_1);
    }
  }

  /* Trata eventos do Sensor 2 (mesma lógica) */
  if (gpio == ECHO_PIN_2) {
    if (events & GPIO_IRQ_EDGE_RISE) {
      echo_start_time_2 = get_absolute_time();
    } else if (events & GPIO_IRQ_EDGE_FALL) {
      echo_end_time_2 = get_absolute_time();
      cancel_alarm(echo_timeout_alarm_2);
    }
  }
}

/**
 * Função principal.
 *
 * Fluxo de operação (loop infinito):
 *   1. Zera todas as variáveis de medição
 *   2. Envia pulso de trigger de 10µs para ambos os sensores
 *   3. Arma alarms de timeout de 20ms para ambos os sensores
 *   4. Aguarda resposta (echo ou timeout) de cada sensor
 *   5. Imprime resultado: distância em cm ou "erro"
 *   6. Repete
 */
int main() {
  stdio_init_all();

  printf("oi \n"); /* Mensagem de inicialização */

  /* === Configuração do Sensor 1 === */
  gpio_init(ECHO_PIN_1);
  gpio_init(TRIGGER_PIN_1);
  gpio_set_dir(ECHO_PIN_1, GPIO_IN);     /* ECHO = entrada */
  gpio_set_dir(TRIGGER_PIN_1, GPIO_OUT); /* TRIGGER = saída */
  gpio_put(TRIGGER_PIN_1, 0);            /* Trigger inicia em LOW */

  /*
   * Configura IRQ nas duas bordas do ECHO (subida e descida).
   * A primeira chamada registra o callback para todos os GPIOs.
   */
  gpio_set_irq_enabled_with_callback(
      ECHO_PIN_1, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_isr);

  /* === Configuração do Sensor 2 === */
  gpio_init(ECHO_PIN_2);
  gpio_init(TRIGGER_PIN_2);
  gpio_set_dir(ECHO_PIN_2, GPIO_IN);
  gpio_set_dir(TRIGGER_PIN_2, GPIO_OUT);
  gpio_put(TRIGGER_PIN_2, 0);

  /*
   * Segunda IRQ reusa o callback já registrado.
   * Na Pico, só existe um callback global para GPIO IRQ,
   * diferenciamos pelo argumento gpio na função.
   */
  gpio_set_irq_enabled_with_callback(
      ECHO_PIN_2, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_isr);

  while (true) {
    /* Reseta variáveis para nova medição */
    alarm_1 = 0;
    echo_start_time_1 = 0;
    echo_end_time_1 = 0;

    alarm_2 = 0;
    echo_start_time_2 = 0;
    echo_end_time_2 = 0;

    /*
     * Envia pulso de trigger para os dois sensores.
     * O SR-04 requer um pulso HIGH de pelo menos 10µs.
     * sleep_us() aqui é aceitável pois estamos no main (não numa ISR).
     */
    sleep_us(100);              /* Intervalo entre medições */
    gpio_put(TRIGGER_PIN_1, 1); /* Trigger HIGH */
    gpio_put(TRIGGER_PIN_2, 1);
    sleep_us(10);               /* Mantém por 10µs */
    gpio_put(TRIGGER_PIN_1, 0); /* Trigger LOW */
    gpio_put(TRIGGER_PIN_2, 0);

    /*
     * Programa alarmes de timeout (20ms cada).
     * Se o echo não chegar em 20ms, o sensor falhou ou
     * o objeto está fora do alcance (~3.4m).
     */
    echo_timeout_alarm_1 =
        add_alarm_in_ms(20, echo_timeout_callback_1, NULL, false);
    echo_timeout_alarm_2 =
        add_alarm_in_ms(20, echo_timeout_callback_2, NULL, false);

    /*
     * Aguarda Sensor 1: espera até receber echo ou timeout.
     * Loop busy-wait é aceitável aqui pois estamos no main.
     */
    while ((alarm_1 == 0) && (echo_end_time_1 == 0)) {
    }

    /* Aguarda Sensor 2 da mesma forma */
    while ((alarm_2 == 0) && (echo_end_time_2 == 0)) {
    }

    /* === Processa resultado do Sensor 1 === */
    if (alarm_1 == 1) {
      /* Timeout: sensor não respondeu → falha */
      printf("Sensor 1 - erro\n");
    } else {
      /* Calcula duração do pulso echo e converte para distância */
      int dt_1 = echo_end_time_1 - echo_start_time_1;
      int distancia_1 = (int)((dt_1 * 0.0343) / 2.0);
      printf("Sensor 1 - %d cm\n", dt_1);
    }

    /* === Processa resultado do Sensor 2 === */
    if (alarm_2 == 1) {
      printf("Sensor 2 - erro\n");
    } else {
      int dt_2 = echo_end_time_2 - echo_start_time_2;
      int distancia_2 = (int)((dt_2 * 0.0343) / 2.0);
      printf("Sensor 2 - %d cm\n", distancia_2);
    }

    /* Cancela qualquer alarm pendente (limpeza) */
    cancel_alarm(echo_timeout_alarm_1);
    cancel_alarm(echo_timeout_alarm_2);
  }

  return 0;
}