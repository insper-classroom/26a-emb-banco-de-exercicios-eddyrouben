/**
 * Exercício: seven-seg
 *
 * Descrição:
 *   Contador de 0 a 9 exibido em um display de 7 segmentos.
 *   A cada pressionamento do botão, o valor incrementa.
 *   Ao chegar em 9, volta para 0.
 *
 * Periféricos utilizados:
 *   - GPIO (saída para os 7 segmentos do display, entrada para botão)
 *   - Interrupções externas (detecção de borda de descida no botão)
 *
 * Regras de qualidade:
 *   - Variáveis globais somente para ISR→main (Rule 1.1)
 *   - volatile em variáveis acessadas por ISR (Rule 1.2)
 *   - Sem gpio_get() - usa interrupções
 *
 * NOTA: Neste exercício, a atualização do display é feita dentro da ISR
 * por simplicidade (chamando seven_seg_display). Idealmente, para projetos
 * maiores, apenas uma flag seria setada na ISR e o display atualizado no main.
 */

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"

/*
 * FIRST_GPIO: primeiro GPIO usado pelo display de 7 segmentos.
 * Os segmentos usam GPIOs consecutivos: GP2 até GP8 (7 pinos).
 * BUTTON_GPIO: pino do botão, logo após os segmentos (GP9).
 */
#define FIRST_GPIO 2
#define BUTTON_GPIO (FIRST_GPIO + 7)

/* Pino do botão verde para incrementar o contador */
const int BTN_PIN_G = 28;

/*
 * Contador global volatile: compartilhado entre a ISR do botão e main.
 * Precisa ser volatile porque é modificado na ISR (Rule 1.2).
 * É global porque precisa passar informação da ISR → main (Rule 1.1).
 */
volatile int cnt = 0;

/*
 * Tabela de conversão: mapeia cada dígito (0-9) para o padrão de bits
 * que acende os segmentos corretos no display de 7 segmentos.
 *
 * Cada bit corresponde a um segmento (a-g):
 *   bit 0 = segmento a (topo)
 *   bit 1 = segmento b (direita superior)
 *   bit 2 = segmento c (direita inferior)
 *   bit 3 = segmento d (base)
 *   bit 4 = segmento e (esquerda inferior)
 *   bit 5 = segmento f (esquerda superior)
 *   bit 6 = segmento g (meio)
 *
 * Exemplo: 0x3f = 0b0111111 → segmentos a,b,c,d,e,f acesos = dígito "0"
 */
int bits[10] = {
    0x3f,  /* 0: segmentos a,b,c,d,e,f */
    0x06,  /* 1: segmentos b,c */
    0x5b,  /* 2: segmentos a,b,d,e,g */
    0x4f,  /* 3: segmentos a,b,c,d,g */
    0x66,  /* 4: segmentos b,c,f,g */
    0x6d,  /* 5: segmentos a,c,d,f,g */
    0x7d,  /* 6: segmentos a,c,d,e,f,g */
    0x07,  /* 7: segmentos a,b,c */
    0x7f,  /* 8: todos os segmentos */
    0x67   /* 9: segmentos a,b,c,f,g */
};

/**
 * Inicializa os 7 pinos GPIO usados pelo display de 7 segmentos.
 * Configura cada pino como saída digital.
 * Os pinos vão de FIRST_GPIO (GP2) até FIRST_GPIO+6 (GP8).
 */
void seven_seg_init(void) {
    for (int gpio = FIRST_GPIO; gpio < FIRST_GPIO + 7; gpio++) {
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_OUT);
    }
}

/**
 * Exibe um valor (0-9) no display de 7 segmentos.
 *
 * Primeiro apaga todos os segmentos (evita "fantasmas"),
 * depois cria uma máscara de bits deslocada para a posição
 * dos GPIOs do display e aplica com gpio_set_mask().
 *
 * @param val Valor inteiro de 0 a 9 para exibir
 */
void seven_seg_display(int val) {
    /* Apaga todos os segmentos primeiro */
    for (int gpio = FIRST_GPIO; gpio < FIRST_GPIO + 7; gpio++) {
        gpio_put(gpio, 0);
    }
    /*
     * Cria a máscara: desloca o padrão de bits do dígito
     * para a posição dos GPIOs (começa em FIRST_GPIO).
     * gpio_set_mask() seta todos os bits de uma vez.
     */
    int32_t mask = bits[val] << FIRST_GPIO;
    gpio_set_mask(mask);
}

/**
 * Callback de interrupção do botão (ISR).
 *
 * Chamada automaticamente pelo hardware quando ocorre uma mudança
 * de borda no pino do botão.
 *
 * - Na borda de descida (0x4): botão foi pressionado.
 *   Incrementa o contador e atualiza o display.
 *   Se o contador passar de 9, volta para 0.
 *
 * - Na borda de subida (0x8): botão foi solto. Não faz nada.
 *
 * NOTA: Idealmente, a atualização do display deveria ser feita
 * no main loop via flag, mas como seven_seg_display() é rápida
 * e não usa printf/delay/loops longos, é aceitável aqui.
 */
void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {         /* Borda de descida - botão pressionado */
        if (++cnt > 9) {         /* Incrementa; se passar de 9, zera */
            cnt = 0;
        }
        seven_seg_display(cnt);  /* Atualiza o display imediatamente */
    }
    else if (events == 0x8) {    /* Borda de subida - botão solto */
        /* Não faz nada ao soltar o botão */
    }
}

/**
 * Função principal.
 *
 * 1. Inicializa stdio para comunicação serial (debug).
 * 2. Configura o botão como entrada com pull-up e interrupção.
 * 3. Inicializa o display de 7 segmentos e mostra o valor 0.
 * 4. Entra em loop infinito (toda lógica é tratada pela ISR).
 */
int main() {
    stdio_init_all();

    /* Configura o botão: entrada com pull-up interno */
    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);

    /*
     * Habilita interrupção na borda de descida do botão.
     * gpio_set_irq_enabled_with_callback() registra tanto
     * a IRQ quanto o callback para o pino especificado.
     */
    gpio_set_irq_enabled_with_callback(BTN_PIN_G,
        GPIO_IRQ_EDGE_FALL,
        true,
        &btn_callback);

    /* Inicializa o display e mostra 0 */
    cnt = 0;
    seven_seg_init();
    seven_seg_display(cnt);

    /*
     * Loop principal vazio: toda a lógica de contagem e
     * atualização do display é gerenciada pela ISR do botão.
     */
    while (true) {
    }
}
