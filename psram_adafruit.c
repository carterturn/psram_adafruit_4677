#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "psram_adafruit.pio.h"

/**
   Pinout:

   GPIO 18: SI/SIO0
   GPIO 19: SO/SIO1
   GPIO 20: SIO2
   GPIO 21: SIO3
   GPIO 26: SCLK
   GPIO 27: CS_n
      CS_n NEEDS a 4.7k pull-up in order for the chip to initialize correctly.
 */

const uint LED_PIN = 25;
const uint SCLK_PIN = 26;
const uint SIO_BASE_PIN = 18;

const PIO pio = pio0;
int state_machine = -1;
int qspi_write_program_offset = -1;
int qspi_read_program_offset = -1;
int qspi_write_dma_channel = -1;
int qspi_read_dma_channel = -1;

const uint8_t DATA[] = "Hello Carter!\0\0\0";
const uint data_len = 16;
uint8_t data_buffer[16];

inline uint32_t round_len(uint32_t len){
	return len/4 + ((len % 4) > 0);
}

int setup_qspi_write(){
	if(state_machine < 0){
		state_machine = pio_claim_unused_sm(pio, true);
	}
	else{
		pio_sm_set_enabled(pio, state_machine, false);
	}
	if(qspi_write_program_offset < 0){
		qspi_write_program_offset = pio_add_program(pio, &qspi_write_program);
	}
	qspi_write_program_init(pio, state_machine, qspi_write_program_offset, SCLK_PIN, SIO_BASE_PIN);
	pio_sm_set_enabled(pio, state_machine, true);

	return 0;
}

int exec_qspi_write(uint32_t * data, uint32_t len){
	pio_sm_put_blocking(pio, state_machine, 2*len-1);
	for(int i = 0; i < round_len(len); i++){
		pio_sm_put_blocking(pio, state_machine, data[i]);
	}
	return 0;
}

int setup_qspi_read(){
	if(state_machine < 0){
		state_machine = pio_claim_unused_sm(pio, true);
	}
	else{
		pio_sm_set_enabled(pio, state_machine, false);
	}
	if(qspi_read_program_offset < 0){
		qspi_read_program_offset = pio_add_program(pio, &qspi_read_program);
	}
	qspi_read_program_init(pio, state_machine, qspi_read_program_offset, SCLK_PIN, SIO_BASE_PIN);
	pio_sm_set_enabled(pio, state_machine, true);

	return 0;
}

int qspi_read_psram(uint32_t addr, uint32_t * data, uint32_t len){
	pio_sm_put_blocking(pio, state_machine, addr | 0xEB000000);
	pio_sm_put_blocking(pio, state_machine, 2*len-1);
	for(int i = 0; i < round_len(len); i++){
		data[i] = pio_sm_get_blocking(pio, state_machine);
	}
	return 0;
}

int spi_reset(){
	uint32_t reset_enable_data = 0x0FF00FF0;
	exec_qspi_write(&reset_enable_data, 4);
	uint32_t reset_data = 0xF00FF00F;
	exec_qspi_write(&reset_data, 4);
	return 0;
}

int spi_enter_quad_mode(){
	uint32_t enter_quad_data = 0x00FF0F0F;
	exec_qspi_write(&enter_quad_data, 4);
	return 0;
}

int qspi_reset(){
	uint32_t reset_enable_data = 0x66000000;
	exec_qspi_write(&reset_enable_data, 1);
	sleep_us(50);
	uint32_t reset_data = 0x99000000;
	exec_qspi_write(&reset_data, 1);
	sleep_us(100);
	return 0;
}

int qspi_write_psram(uint32_t addr, uint8_t * data, uint32_t len){
	uint32_t len_full = 1 + round_len(len); // Command byte + address + data
	uint32_t * buffer = (uint32_t *) malloc(len_full * sizeof(uint32_t));
	buffer[0] = 0x38000000 | (addr & 0x00FFFFFF);
	memcpy(&(buffer[1]), data, len);
	exec_qspi_write(buffer, 4 + len);
	free(buffer);
}

int main(){
	// Set CS high on startup
	gpio_init(SCLK_PIN+1);
	gpio_set_dir(SCLK_PIN+1, GPIO_OUT);
	gpio_put(SCLK_PIN+1, 1);

	stdio_init_all();
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	sleep_ms(2000);
	gpio_put(LED_PIN, 1);

	setup_qspi_write();
	spi_reset();
	spi_enter_quad_mode();
	sleep_ms(500);

	while(1){
		printf("Reset\n");
		printf("Write state: %d %d\n", state_machine, qspi_write_program_offset);
		setup_qspi_write();
		memcpy(data_buffer, DATA, data_len);
		qspi_write_psram(0x00000000, data_buffer, data_len);
		printf("Write complete\n");
		bzero(data_buffer, data_len);
		setup_qspi_read();
		printf("Read state: %d %d\n", state_machine, qspi_read_program_offset);
		sleep_ms(500);
		qspi_read_psram(0x00000000, (uint32_t *) data_buffer, data_len);
		printf(":%s:\n", data_buffer);
		sleep_ms(500);
	}


	return 0;
}
