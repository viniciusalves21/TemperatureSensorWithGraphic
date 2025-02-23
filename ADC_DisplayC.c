#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define LED_PIN 13
#define BUZZER_PIN 21 // Pino GPIO para o Buzzer

#define GRAPH_WIDTH 100  // Largura do gráfico
#define GRAPH_HEIGHT 40  // Altura do gráfico
#define GRAPH_X 14       // Posição X inicial do gráfico
#define GRAPH_Y 10       // Posição Y inicial do gráfico
float temp_limit = 35.0;

// Buffer de histórico de temperatura
float temperature_values[GRAPH_WIDTH] = {0}; 
int current_index = 0;

// Função para obter a temperatura do sensor interno do RP2040
float get_temperature() {
    adc_select_input(4);  // Seleciona o canal do sensor de temperatura
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read(); // Lê o valor do ADC
    float adc_voltage = result * conversion_factor;
    return 27 - (adc_voltage - 0.706) / 0.001721; // Converte a leitura para temperatura
}

// Função para desenhar o gráfico de temperatura
void draw_temperature_graph(ssd1306_t *ssd) {
    float temp_min = 20.0;  // Temperatura mínima esperada
    float temp_max = 40.0;  // Temperatura máxima esperada
    float temp_range = temp_max - temp_min;

    // Desenha as bordas do gráfico
    ssd1306_line(ssd, GRAPH_X, GRAPH_Y, GRAPH_X + GRAPH_WIDTH, GRAPH_Y, true);
    ssd1306_line(ssd, GRAPH_X, GRAPH_Y + GRAPH_HEIGHT, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, true);
    ssd1306_line(ssd, GRAPH_X, GRAPH_Y, GRAPH_X, GRAPH_Y + GRAPH_HEIGHT, true);
    ssd1306_line(ssd, GRAPH_X + GRAPH_WIDTH, GRAPH_Y, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, true);

    // Plota os valores da temperatura no gráfico
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
        int x1 = GRAPH_X + i;
        int x2 = GRAPH_X + i + 1;

        int y1 = GRAPH_Y + GRAPH_HEIGHT - ((temperature_values[i] - temp_min) / temp_range * GRAPH_HEIGHT);
        int y2 = GRAPH_Y + GRAPH_HEIGHT - ((temperature_values[i + 1] - temp_min) / temp_range * GRAPH_HEIGHT);

        if (y1 < GRAPH_Y) y1 = GRAPH_Y;
        if (y1 > GRAPH_Y + GRAPH_HEIGHT) y1 = GRAPH_Y + GRAPH_HEIGHT;
        if (y2 < GRAPH_Y) y2 = GRAPH_Y;
        if (y2 > GRAPH_Y + GRAPH_HEIGHT) y2 = GRAPH_Y + GRAPH_HEIGHT;

        ssd1306_line(ssd, x1, y1, x2, y2, true);
    }
}

int main() {
    stdio_init_all();
    
    // Inicializa GPIOs
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);

    // Inicializa I2C para comunicação com o display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicializa o display OLED
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
    
    // Inicializa o sensor de temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true) {
        // Obtém a temperatura e adiciona ao histórico
        float temperature = get_temperature();
        temperature_values[current_index] = temperature;
        current_index = (current_index + 1) % GRAPH_WIDTH;

        ssd1306_fill(&ssd, false);  // Limpa o display

        // Desenha o gráfico de temperatura
        draw_temperature_graph(&ssd);

        // Exibe a temperatura numérica abaixo do gráfico
        char str_temp[16];
        sprintf(str_temp, "TEMP %.0f C", temperature);
        ssd1306_draw_string(&ssd, str_temp, 21, 54);

        ssd1306_send_data(&ssd);  // Atualiza o display

        // Controle do LED e do buzzer baseado na temperatura
        if (temperature > 32) {
            gpio_put(LED_PIN, true);
        } else {
            gpio_put(LED_PIN, false);
        }

        if (temperature >= temp_limit) {
            // Exibe mensagem de alerta no OLED
            ssd1306_fill(&ssd, false);
            ssd1306_draw_string(&ssd, "TEMP ELEVADA", 10, 20);
            ssd1306_draw_string(&ssd, "RISCO DE SUPER AQUECI", 5, 35);
            ssd1306_send_data(&ssd);
        
            // Buzzer toca em pulsos para chamar atenção
            for (int i = 0; i < 5; i++) {  // Toca 5 vezes
                gpio_put(BUZZER_PIN, 1);
                sleep_ms(200);
                gpio_put(BUZZER_PIN, 0);
                sleep_ms(200);
            }

            // Mantém alerta até que a temperatura baixe
            while (get_temperature() >= temp_limit) {
                gpio_put(BUZZER_PIN, 1);
                sleep_ms(1000);
                gpio_put(BUZZER_PIN, 0);
                sleep_ms(1000);
            }

            // Após normalizar, volta ao gráfico
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
        }

        sleep_ms(2000); // Aguarda antes da próxima leitura 
    }
}
