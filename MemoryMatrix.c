#include <stdio.h>
#include <stdlib.h> // Para rand() e srand()
#include <time.h>   // Para time() (seed para rand)
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"

// Definições de pinos
#define LED_COUNT 25
#define LED_PIN 7
#define JOYSTICK_VRX 27
#define JOYSTICK_VRY 26
#define JOYSTICK_SW 22
#define BUTTON_COR_1 5  // Botão para cor 1
#define BUTTON_COR_2 6  // Botão para cor 2
#define BUZZER_ACERTO 10
#define BUZZER_ERRO 21

// Definições de cores
#define COR_1_R 255
#define COR_1_G 0
#define COR_1_B 0   // Vermelho
#define COR_2_R 0
#define COR_2_G 255
#define COR_2_B 0   // Verde
#define COR_OFF_R 0
#define COR_OFF_G 0
#define COR_OFF_B 0   // Desligado

// Definições de direção (Mapear as direções do Joystick)
typedef enum {
    CIMA,
    BAIXO,
    ESQUERDA,
    DIREITA,
    CENTRO // Para quando o joystick está no centro
} Direcao;

// Definição de pixel GRB
struct pixel_t {
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Protótipos das Funções (Importante para organizar o código)
void npInit(uint pin);
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
void npWrite();
int getIndex(int x, int y);
Direcao lerJoystick();
bool lerBotaoCor1();
bool lerBotaoCor2();
void tocarBuzzerAcerto(int duracao_ms);
void tocarBuzzerErro(int duracao_ms);
void mostrarSequencia(const Direcao *sequencia, const bool *cores, int tamanho);
bool verificarSequencia(const Direcao *sequencia_correta, const bool *cores_corretas, int tamanho);
void desenharSeta(Direcao direcao, bool cor1, bool cor2);
void mapearDirecaoNaMatriz(Direcao direcao, int r, int g, int b);

/**
 * Inicializa a máquina PIO para controle da matriz de LEDs.
 */
void npInit(uint pin) {

  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0) {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }

  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Limpa buffer de pixels.
  for (uint i = 0; i < LED_COUNT; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

/**
 * Limpa o buffer de pixels.
 */
void npClear() {
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

/**
 * Escreve os dados do buffer nos LEDs.
 */
void npWrite() {
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < LED_COUNT; ++i) {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return 24-(y * 5 + x); // Linha par (esquerda para direita).
    } else {
        return 24-(y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
    }
}

Direcao lerJoystick() {
    int vrx_value = gpio_get(JOYSTICK_VRX); // 0 ou 1 (digital)
    int vry_value = gpio_get(JOYSTICK_VRY); // 0 ou 1 (digital)

    // Mapeamento básico (ajuste conforme necessário)
    if (vrx_value == 0 && vry_value == 1) {
        return ESQUERDA;
    } else if (vrx_value == 1 && vry_value == 0) {
        return DIREITA;
    } else if (vry_value == 1 && vrx_value == 1) {
        return CIMA;
    } else if (vry_value == 0 && vrx_value == 0) {
        return BAIXO;
    } else {
        return CENTRO; // Joystick no centro
    }
}

bool lerBotaoCor1() {
    return !gpio_get(BUTTON_COR_1); // Retorna true se o botão estiver pressionado (pull-up)
}

bool lerBotaoCor2() {
    return !gpio_get(BUTTON_COR_2); // Retorna true se o botão estiver pressionado (pull-up)
}

void tocarBuzzerAcerto(int duracao_ms) {
    gpio_put(BUZZER_ACERTO, 1);
    sleep_ms(duracao_ms/2);
    gpio_put(BUZZER_ACERTO, 0);
    sleep_ms(duracao_ms/2);
}

void tocarBuzzerErro(int duracao_ms) {
    gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
     gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
     gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
     gpio_put(BUZZER_ERRO, 1);
    sleep_ms(duracao_ms/4);
    gpio_put(BUZZER_ERRO, 0);
}

void mapearDirecaoNaMatriz(Direcao direcao, int r, int g, int b){
    npClear();
    switch (direcao) {
        case CIMA:
            // Define os LEDs para representar a seta para cima
            npSetLED(getIndex(2, 0), r, g, b);   // LED superior central
            npSetLED(getIndex(1, 1), r, g, b);  // LEDs laterais superiores
            npSetLED(getIndex(3, 1), r, g, b);
            npSetLED(getIndex(2, 1), r, g, b);  // LED central
            npSetLED(getIndex(2, 2), r, g, b);  // LED central
            npSetLED(getIndex(2, 3), r, g, b);  // LED central
            npSetLED(getIndex(2, 4), r, g, b);  // LED central

            break;
        case BAIXO:
            // Define os LEDs para representar a seta para baixo
             npSetLED(getIndex(2, 4), r, g, b);   // LED inferior central
            npSetLED(getIndex(1, 3), r, g, b);  // LEDs laterais inferiores
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(2, 3), r, g, b);  // LED central
            npSetLED(getIndex(2, 2), r, g, b);  // LED central
            npSetLED(getIndex(2, 1), r, g, b);  // LED central
            npSetLED(getIndex(2, 0), r, g, b);  // LED central
            break;
        case ESQUERDA:
            // Define os LEDs para representar a seta para a esquerda
            npSetLED(getIndex(0, 2), r, g, b);   // LED esquerdo central
            npSetLED(getIndex(1, 1), r, g, b);  // LEDs superior e inferior
            npSetLED(getIndex(1, 3), r, g, b);
            npSetLED(getIndex(1, 2), r, g, b);  // LED central
            npSetLED(getIndex(2, 2), r, g, b);  // LED central
            npSetLED(getIndex(3, 2), r, g, b);  // LED central
            npSetLED(getIndex(4, 2), r, g, b);  // LED central
            break;
        case DIREITA:
            // Define os LEDs para representar a seta para a direita
            npSetLED(getIndex(4, 2), r, g, b);   // LED direito central
            npSetLED(getIndex(3, 1), r, g, b);  // LEDs superior e inferior
            npSetLED(getIndex(3, 3), r, g, b);
            npSetLED(getIndex(3, 2), r, g, b);  // LED central
            npSetLED(getIndex(2, 2), r, g, b);  // LED central
            npSetLED(getIndex(1, 2), r, g, b);  // LED central
            npSetLED(getIndex(0, 2), r, g, b);  // LED central
            break;
        default:
            // Limpa todos os LEDs se a direção não for válida
            npClear();
            break;
    }
    npWrite();
}

void mostrarSequencia(const Direcao *sequencia, const bool *cores, int tamanho) {
    for (int i = 0; i < tamanho; i++) {
        // Mostra a seta e a cor correspondente
        if (cores[i]) {
            // Cor 1
            mapearDirecaoNaMatriz(sequencia[i], COR_1_R, COR_1_G, COR_1_B);
        } else {
            // Cor 2
            mapearDirecaoNaMatriz(sequencia[i], COR_2_R, COR_2_G, COR_2_B);
        }
        sleep_ms(500); // Tempo que a seta fica visível
        npClear();     // Apaga a seta
        npWrite();
        sleep_ms(250); // Pequena pausa entre as setas
    }
}

bool verificarSequencia(const Direcao *sequencia_correta, const bool *cores_corretas, int tamanho) {
     printf("Prepare-se! 5 segundos de pausa...\n");
    sleep_ms(5000); // Pausa de 5 segundos antes de ler a entrada do jogador

    for (int i = 0; i < tamanho; i++) {
        Direcao input_direcao = CENTRO; // Inicializa com CENTRO
        bool input_cor1 = false;
        bool input_cor2 = false;

        // Aguarda o jogador inserir a direção e a cor
        while (input_direcao == CENTRO && !input_cor1 && !input_cor2) {
            input_direcao = lerJoystick();
            input_cor1 = lerBotaoCor1();
            input_cor2 = lerBotaoCor2();
            sleep_ms(50); // Pequeno delay para evitar leituras múltiplas
        }

        // Verifica se a direção está correta
        if (input_direcao != sequencia_correta[i]) {
            printf("Direcao incorreta!\n");
            return false;
        }

        // Determina a cor selecionada pelo jogador
        bool cor_selecionada = input_cor1; // Se o botão 1 foi pressionado, considera a cor 1

        // Se ambos os botões foram pressionados, considera um erro
        if (input_cor1 && input_cor2) {
            printf("Ambos os botões de cor pressionados!\n");
            return false;
        }

        // Verifica se a cor está correta
        if (cor_selecionada != cores_corretas[i]) {
            printf("Cor incorreta!\n");
            return false;
        }
    }

    return true; // Sequência correta
}

int main() {
    // Inicialização
    stdio_init_all();
    gpio_init(JOYSTICK_VRX);
    gpio_init(JOYSTICK_VRY);
    gpio_init(JOYSTICK_SW);
    gpio_init(BUTTON_COR_1);
    gpio_init(BUTTON_COR_2);
    gpio_init(BUZZER_ACERTO);
    gpio_init(BUZZER_ERRO);

    gpio_set_dir(JOYSTICK_VRX, GPIO_IN);
    gpio_set_dir(JOYSTICK_VRY, GPIO_IN);
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_set_dir(BUTTON_COR_1, GPIO_IN);
    gpio_set_dir(BUTTON_COR_2, GPIO_IN);
    gpio_set_dir(BUZZER_ACERTO, GPIO_OUT);
    gpio_set_dir(BUZZER_ERRO, GPIO_OUT);

    gpio_pull_up(JOYSTICK_SW);
    gpio_pull_up(BUTTON_COR_1);
    gpio_pull_up(BUTTON_COR_2);

    npInit(LED_PIN);
    npClear();
    npWrite();

    srand(time(NULL)); // Inicializa a semente do gerador de números aleatórios

    int nivel = 1; // Começa no nível 1
    const int MAX_SEQUENCIA = 10; // Máximo tamanho da sequência

    while (true) {
        printf("Nível: %d\n", nivel);

        // 1. Gerar Sequência Aleatória
        Direcao sequencia[MAX_SEQUENCIA];
        bool cores[MAX_SEQUENCIA];
        for (int i = 0; i < nivel; i++) {
            sequencia[i] = (Direcao)(rand() % 4); // 0 a 3 (CIMA, BAIXO, ESQUERDA, DIREITA)
            cores[i] = rand() % 2; // 0 ou 1 (Cor 1 ou Cor 2)
        }

        // 2. Mostrar Sequência
        mostrarSequencia(sequencia, cores, nivel);

        // 3. Jogador Tenta Repetir
        printf("Sua vez!\n");
        bool acertou = verificarSequencia(sequencia, cores, nivel);

        // 4. Verificar Resultado
        if (acertou) {
            printf("Parabéns! Próximo nível.\n");
            tocarBuzzerAcerto(200);
            nivel++;
            sleep_ms(1000);
            if (nivel > MAX_SEQUENCIA){
                printf("Você venceu o jogo!\n");
                while(true){
                    npClear();
                    npWrite();
                }
            }
        } else {
            printf("Errou! Game Over.\n");
            tocarBuzzerErro(500);
            sleep_ms(2000);
            nivel = 1; // Reinicia o nível
        }
    }

    return 0;
}