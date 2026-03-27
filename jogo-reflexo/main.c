/**
 * Exercício: jogo-reflexo (Genius)
 *
 * Descrição:
 *   Jogo de memória tipo Genius com sequência fixa de 10 cores.
 *   - O jogo exibe uma sequência crescente de LEDs.
 *   - O jogador deve repetir a sequência na mesma ordem.
 *   - A cada acerto, a sequência aumenta em 1.
 *   - Ao errar, o jogo termina e exibe a pontuação.
 *
 * Sequência fixa (10 passos):
 *   🟡(Y), 🟢(G), 🔴(R), 🟡(Y), 🟢(G), 🟡(Y), 🔴(R), 🟡(Y), 🟢(G), 🟡(Y)
 *   Índices: 1, 0, 2, 1, 0, 1, 2, 1, 0, 1
 *   (0=Verde, 1=Amarelo, 2=Vermelho)
 *
 * Periféricos:
 *   - GPIO (saída para 3 LEDs, entrada para 3 botões)
 *   - Interrupções externas (botões)
 *
 * Restrições:
 *   - NÃO usar gpio_get()
 *   - Usar interrupções nos botões
 *   - printf("Points: %d\n", level) ao final
 *
 * Pinagem:
 *   LED verde = GP5    Botão verde = GP28
 *   LED amarelo = GP9  Botão amarelo = GP26
 *   LED vermelho = GP13  Botão vermelho = GP20
 */

#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/* Macros de pinos (não usadas diretamente mas mantidas para referência) */
#define FIRST_GPIO 2
#define BUTTON_GPIO (FIRST_GPIO + 7)

/* Pinos dos botões */
const int BTN_PIN_G = 28;   /* Botão verde */
const int BTN_PIN_Y = 26;   /* Botão amarelo */
const int BTN_PIN_R = 20;   /* Botão vermelho */

/* Pinos dos LEDs */
const int LED_PIN_G = 5;    /* LED verde */
const int LED_PIN_Y = 9;    /* LED amarelo */
const int LED_PIN_R = 13;   /* LED vermelho */

/*
 * Flag volatile do botão pressionado.
 * Armazena o GPIO do último botão pressionado.
 * Volatile pois é modificada na ISR (Rule 1.2).
 */
volatile int btn_f = 0;

/*
 * Sequência fixa do jogo (10 passos).
 * Cada número corresponde a um índice nos arrays led_pin e btn_pin:
 *   0 = Verde, 1 = Amarelo, 2 = Vermelho
 *
 * Corresponde a: 🟡, 🟢, 🔴, 🟡, 🟢, 🟡, 🔴, 🟡, 🟢, 🟡
 */
int seq[] = {1, 0, 2, 1, 0, 1, 2, 1, 0, 1};

/* Arrays de lookup: mapeia índice (0,1,2) para pinos GPIO */
int led_pin[] = {LED_PIN_G, LED_PIN_Y, LED_PIN_R};
int btn_pin[] = {BTN_PIN_G, BTN_PIN_Y, BTN_PIN_R};

/**
 * ISR do botão: salva qual botão foi pressionado.
 * Na borda de descida (0x4): botão apertado → salva o GPIO.
 *
 * ISR CURTA: apenas armazena um valor (Rules 3.0-3.3).
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {      /* Borda de descida */
        btn_f = gpio;         /* Salva qual botão */
    } else if (events == 0x8) {  /* Borda de subida */
        /* Não faz nada ao soltar */
    }
}

/**
 * Função principal.
 *
 * Fluxo do jogo:
 * 1. Aguarda botão verde para iniciar.
 * 2. Loop de rodadas (level 1 a 10):
 *    a. Exibe a sequência acumulada (todos os LEDs da seq até level).
 *    b. Aguarda o jogador repetir a sequência.
 *    c. Se acertou: avança para próxima rodada.
 *    d. Se errou: encerra e mostra pontuação.
 * 3. Após game over, volta a aguardar botão verde.
 */
int main() {
    stdio_init_all();

    /* === Inicializa os 3 botões com pull-up e IRQ === */
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);
    gpio_set_irq_enabled_with_callback(BTN_PIN_G, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);
    /* Reusa callback já registrado */
    gpio_set_irq_enabled_with_callback(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);
    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);

    /* === Inicializa os 3 LEDs === */
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);

    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);

    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);

    int pnts = 0;       /* Pontuação (não usada diretamente) */
    int level = 1;       /* Nível atual (número de cores mostradas) */
    int acertou = 1;     /* Flag: 1 = jogador acertou a sequência */
    int led = -1;        /* Índice do LED correspondente ao botão pressionado */

    while (true) {
        /*
         * AGUARDA INÍCIO: espera o jogador apertar o botão verde.
         * Busy-wait no main é aceitável aqui.
         */
        while (true) {
            if (btn_f == btn_pin[0]) break;  /* btn_pin[0] = BTN_PIN_G */
        }

        /* Inicializa variáveis para nova partida */
        pnts = 0;
        btn_f = 0;
        acertou = 1;
        level = 0;

        /*
         * LOOP DE RODADAS: a cada rodada, aumenta o nível.
         * Continua enquanto o jogador acertar.
         */
        while (acertou == 1) {
            level++;  /* Avança o nível (1, 2, 3, ..., 10) */

            /* Pausa de 300ms antes de mostrar a sequência */
            sleep_ms(300);

            /*
             * EXIBIÇÃO DA SEQUÊNCIA:
             * Mostra os LEDs da posição 0 até level-1.
             * Cada LED fica aceso por 300ms, com intervalo de 300ms.
             */
            for (int i = 0; i < level; i++) {
                gpio_put(led_pin[seq[i]], 1);  /* Acende LED da sequência */
                sleep_ms(300);                  /* Mantém por 300ms */
                gpio_put(led_pin[seq[i]], 0);  /* Apaga */
                sleep_ms(300);                  /* Intervalo de 300ms */
            }

            /* Limpa flag de botão antes de aceitar entrada */
            btn_f = 0;

            /*
             * ENTRADA DO JOGADOR:
             * O jogador deve pressionar os botões na mesma ordem.
             * Para cada posição da sequência, espera um botão.
             */
            for (int i = 0; i < level; i++) {
                /* Espera o jogador apertar um botão */
                while (btn_f == 0) {
                }

                /*
                 * Converte o GPIO do botão pressionado para o
                 * índice correspondente (0=G, 1=Y, 2=R).
                 */
                if (btn_f == BTN_PIN_G)
                    led = 0;
                else if (btn_f == BTN_PIN_Y)
                    led = 1;
                else if (btn_f == BTN_PIN_R)
                    led = 2;
                else
                    led = -1;

                /* Acende o LED correspondente ao botão pressionado */
                gpio_put(led_pin[led], 1);

                /*
                 * Verifica se o botão correto foi pressionado.
                 * btn_pin[seq[i]] é o botão esperado na posição i.
                 */
                if (btn_f != btn_pin[seq[i]]) {
                    acertou = 0;  /* ERROU! Encerra o loop */
                    break;
                }

                /* Feedback: mantém LED aceso por 300ms e depois apaga */
                sleep_ms(300);
                gpio_put(led_pin[led], 0);

                btn_f = 0;  /* Limpa flag para o próximo botão */
            }
            btn_f = 0;  /* Limpa flag ao final da rodada */
        }

        /*
         * FIM DE JOGO: exibe a pontuação.
         * level contém o número da rodada em que o jogador errou.
         * Formato obrigatório para os testes automáticos.
         */
        printf("Points: %d\n", level);

        /* Apaga todos os LEDs */
        gpio_put(led_pin[0], 0);
        gpio_put(led_pin[1], 0);
        gpio_put(led_pin[2], 0);
    }
}
