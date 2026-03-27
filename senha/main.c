/**
 * Exercício: senha
 *
 * Descrição:
 *   Sistema de senha com 4 botões.
 *   Fase 1: O usuário define uma senha pressionando 4 botões em sequência.
 *   Fase 2: O usuário tenta reproduzir a senha.
 *     - LED roxo acende a cada botão pressionado (feedback visual).
 *     - LED verde acende ao ACERTAR a senha.
 *     - LED vermelho acende ao ERRAR a senha.
 *
 * Periféricos:
 *   - GPIO (saída para 3 LEDs, entrada para 4 botões)
 *   - Interrupções externas (detecção de pressionamento dos botões)
 *
 * Restrições:
 *   - NÃO usar gpio_get()
 *   - Usar interrupções nos botões
 *   - LEDs verde/vermelho ficam acesos por 300ms
 *   - Senha tem tamanho 4
 *   - Verificar senha APENAS após os 4 botões serem pressionados
 *
 * Pinagem (do diagram.json):
 *   Botão verde  = GP28   Botão azul    = GP27
 *   Botão amarelo = GP21  Botão branco  = GP17
 *   LED vermelho = GP2    LED roxo = GP26   LED verde = GP9
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"

/* ========== Definição dos pinos ========== */
/* Botões */
const int BTN_PIN_G = 28;   /* Botão verde */
const int BTN_PIN_B = 27;   /* Botão azul */
const int BTN_PIN_Y = 21;   /* Botão amarelo */
const int BTN_PIN_W = 17;   /* Botão branco */

/* LEDs */
const int LED_PIN_R = 2;    /* LED vermelho (senha errada) */
const int LED_PIN_P = 26;   /* LED roxo (feedback de botão pressionado) */
const int LED_PIN_G = 9;    /* LED verde (senha correta / senha configurada) */

/* ========== Constantes ========== */
#define SENHA_LEN 4          /* Tamanho da senha */

/* ========== Variáveis globais volatile ========== */
/*
 * g_btn_pressed: armazena qual botão foi pressionado (valor = pino GPIO).
 * Volatile pois é modificada na ISR e lida no main (Rule 1.2).
 */
volatile int g_btn_pressed = 0;

/**
 * ISR do botão: registra qual botão foi pressionado na borda de descida.
 * ISR CURTA: apenas salva o GPIO, sem printf/delay/loops (Rules 3.0-3.3).
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {    /* Borda de descida = botão apertado */
        g_btn_pressed = gpio;
    }
}

/**
 * Função principal.
 *
 * Máquina de estados:
 *   1. DEFINIÇÃO: Captura 4 botões para definir a senha.
 *      - LED roxo pisca a cada botão pressionado.
 *      - LED verde acende por 300ms ao completar a definição.
 *   2. VERIFICAÇÃO: Captura 4 botões e compara com a senha.
 *      - LED roxo pisca a cada botão pressionado.
 *      - Após o 4º botão, compara:
 *        - Acertou → LED verde por 300ms → volta para verificação
 *        - Errou → LED vermelho por 300ms → volta para verificação
 *   3. Repete a verificação indefinidamente.
 */
int main() {
    stdio_init_all();

    /* === Inicializa LEDs como saída === */
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_init(LED_PIN_P);
    gpio_set_dir(LED_PIN_P, GPIO_OUT);
    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);

    /* === Inicializa botões como entrada com pull-up + IRQ === */
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);
    gpio_set_irq_enabled_with_callback(BTN_PIN_G,
        GPIO_IRQ_EDGE_FALL, true, &btn_callback);

    gpio_init(BTN_PIN_B);
    gpio_set_dir(BTN_PIN_B, GPIO_IN);
    gpio_pull_up(BTN_PIN_B);
    gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_FALL, true);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    gpio_init(BTN_PIN_W);
    gpio_set_dir(BTN_PIN_W, GPIO_IN);
    gpio_pull_up(BTN_PIN_W);
    gpio_set_irq_enabled(BTN_PIN_W, GPIO_IRQ_EDGE_FALL, true);

    /* Arrays para armazenar senha definida e tentativa */
    int senha[SENHA_LEN];       /* Senha definida pelo usuário */
    int tentativa[SENHA_LEN];   /* Tentativa de reproduzir a senha */

    while (true) {
        /*
         * ============================================
         * FASE 1: DEFINIÇÃO DA SENHA
         * O usuário pressiona 4 botões para definir a
         * combinação secreta.
         * ============================================
         */
        int idx = 0;
        g_btn_pressed = 0;

        while (idx < SENHA_LEN) {
            /* Aguarda um botão ser pressionado */
            if (g_btn_pressed != 0) {
                /* Salva o botão na posição atual da senha */
                senha[idx] = g_btn_pressed;
                g_btn_pressed = 0;
                idx++;

                /* LED roxo acende brevemente como feedback */
                gpio_put(LED_PIN_P, 1);
                sleep_ms(150);
                gpio_put(LED_PIN_P, 0);
                sleep_ms(150);
            }
        }

        /*
         * Senha definida! LED verde acende por 300ms
         * para indicar que a configuração foi concluída.
         */
        gpio_put(LED_PIN_G, 1);
        sleep_ms(300);
        gpio_put(LED_PIN_G, 0);

        /*
         * ============================================
         * FASE 2: VERIFICAÇÃO DA SENHA
         * O usuário tenta reproduzir a senha pressionando
         * os mesmos 4 botões na mesma ordem.
         * ============================================
         */
        while (true) {
            idx = 0;
            g_btn_pressed = 0;
            sleep_ms(300);  /* Pequena pausa antes de aceitar entrada */

            /* Captura 4 botões da tentativa */
            while (idx < SENHA_LEN) {
                if (g_btn_pressed != 0) {
                    tentativa[idx] = g_btn_pressed;
                    g_btn_pressed = 0;
                    idx++;

                    /* LED roxo como feedback visual do botão */
                    gpio_put(LED_PIN_P, 1);
                    sleep_ms(150);
                    gpio_put(LED_PIN_P, 0);
                    sleep_ms(150);
                }
            }

            /*
             * Compara a tentativa com a senha definida.
             * Verifica cada posição da sequência.
             */
            int acertou = 1;
            for (int i = 0; i < SENHA_LEN; i++) {
                if (tentativa[i] != senha[i]) {
                    acertou = 0;
                    break;
                }
            }

            if (acertou) {
                /* Senha CORRETA: LED verde por 300ms */
                gpio_put(LED_PIN_G, 1);
                sleep_ms(300);
                gpio_put(LED_PIN_G, 0);
            } else {
                /* Senha ERRADA: LED vermelho por 300ms */
                gpio_put(LED_PIN_R, 1);
                sleep_ms(300);
                gpio_put(LED_PIN_R, 0);
            }
        }
    }
}
