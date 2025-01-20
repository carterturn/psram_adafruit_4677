#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "psram_adafruit.pio.h"

/**
   Pinout:

   GPIO 18: SI/SIO0
   GPIO 19: SO/SIO1
   GPIO 20: SIO2
   GPIO 21: SIO3
   GPIO 26: SCLK
   GPIO 27: CS_n
 */

const uint LED_PIN = 25;
const uint SCLK_PIN = 26;
const uint SIO_BASE_PIN = 18;

const PIO pio = pio0;
int state_machine = -1;
int qspi_write_program_offset = -1;
int qspi_read_program_offset = -1;

uint8_t data_buffer[] = "Hello Carter!\0";
uint data_len = 14;

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

int exec_qspi_write(uint8_t * data, uint32_t len){
	pio_sm_put_blocking(pio, state_machine, 2*len-1);
	for(int i = 0; i < len; i++){
		pio_sm_put_blocking(pio, state_machine, data[i] << 24);
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

int qspi_read_psram(uint32_t addr, uint8_t * data, uint32_t len){
	pio_sm_put_blocking(pio, state_machine, addr | 0xEB000000);
	pio_sm_put_blocking(pio, state_machine, 2*len-1);
	for(int i = 0; i < len; i++){
		data[i] = pio_sm_get_blocking(pio, state_machine);
	}
	return 0;
}

int spi_reset(){
	uint8_t reset_enable_data[] = {0x0F, 0xF0, 0x0F, 0xF0};
	exec_qspi_write(reset_enable_data, 4);
	uint8_t reset_data[] = {0xF0, 0x0F, 0xF0, 0x0F};
	exec_qspi_write(reset_data, 4);
	return 0;
}

int spi_enter_quad_mode(){
	uint8_t enter_quad_data[] = {0x00, 0xFF, 0x0F, 0x0F};
	exec_qspi_write(enter_quad_data, 4);
	return 0;
}

int qspi_reset(){
	uint8_t reset_enable_data[] = {0x66};
	exec_qspi_write(reset_enable_data, 1);
	uint8_t reset_data[] = {0x99};
	exec_qspi_write(reset_data, 1);
	return 0;
}

int qspi_write_psram(uint32_t addr, uint8_t * data, uint32_t len){
	uint32_t len_full = 1 + 3 + len; // Command byte + address + data
	uint8_t * buffer = (uint8_t *) malloc(len_full);
	buffer[0] = 0x38;
	memcpy(&(buffer[1]), &addr, 3);
	memcpy(&(buffer[4]), data, len);
	exec_qspi_write(buffer, len_full);
	free(buffer);
}

int main(){
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
		qspi_write_psram(0x00000000, data_buffer, data_len);
		printf("Write complete\n");
		bzero(data_buffer, data_len);
		setup_qspi_read();
		printf("Read state: %d %d\n", state_machine, qspi_read_program_offset);
		sleep_ms(500);
		qspi_read_psram(0x00000000, data_buffer, data_len);
		printf(":%s:\n", data_buffer);
		sleep_ms(500);
	}


	return 0;
}
