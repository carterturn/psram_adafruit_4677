#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"

// #include "spi.pio.h"
#include "psram_adafruit.pio.h"

/**
   Pinout:

   GPIO 0: SO/SIO1
   GPIO 1: SI/SIO[0]
   GPIO 2: SCLK
   GPIO 3: CS_n
      CS_n NEEDS a 4.7k pull-up in order for the chip to initialize correctly.
   GPIO 4: passthru SCLK
   GPIO 5: passthru CS_n
   GPIO 6: output update
 */

const uint LED_PIN = 25;
const uint SO_PIN = 0;
const uint SI_PIN = 1;
const uint SCLK_PIN = 2;
const uint CS_n_PIN = 3;
const uint LDAC_PIN = 6;

const PIO pio = pio0;
int state_machine = -1;
int spi_write_program_offset = -1;
int spi_read_program_offset = -1;

void setup_write(){
	if(state_machine < 0){
		state_machine = pio_claim_unused_sm(pio, true);
	}
	else{
		pio_sm_set_enabled(pio, state_machine, false);
	}
	if(spi_write_program_offset < 0){
		spi_write_program_offset = pio_add_program(pio, &spi_write_program);
	}
	spi_write_program_init(pio, state_machine, spi_write_program_offset, SCLK_PIN, SI_PIN);
	pio_sm_set_enabled(pio, state_machine, true);	
}

void setup_read(){
	if(state_machine < 0){
		state_machine = pio_claim_unused_sm(pio, true);
	}
	else{
		pio_sm_set_enabled(pio, state_machine, false);
	}
	if(spi_read_program_offset < 0){
		spi_read_program_offset = pio_add_program(pio, &spi_read_passthru_program);
	}
	spi_read_passthru_program_init(pio, state_machine, spi_read_program_offset,
								   SCLK_PIN, SO_PIN, SI_PIN);
	pio_sm_set_enabled(pio, state_machine, true);	
}

void reset(){
	setup_write();
	pio_sm_put_blocking(pio0, state_machine, 0x0000000F);
	pio_sm_put_blocking(pio0, state_machine, 0x66990000);
}

void write_uint32(uint32_t addr, uint32_t data){
	setup_write();
	pio_sm_put_blocking(pio0, state_machine, 0x0000003F);
	pio_sm_put_blocking(pio0, state_machine, 0x02000000 | addr);
	pio_sm_put_blocking(pio0, state_machine, data);
}

uint32_t read_uint32(uint32_t addr){
	setup_read();
	pio_sm_put_blocking(pio0, state_machine, 0x03000000 | addr);
	pio_sm_put_blocking(pio0, state_machine, 0x0000001F);
	return pio_sm_get_blocking(pio0, state_machine);
}

const float e2_24 = 16777216.0;
const float V_MAX = 4.096 * 2.5;
const float DAC_SCALE = e2_24 / V_MAX;
void write_voltage(uint32_t addr, float V){
	uint32_t data = (((uint32_t) (DAC_SCALE * V)) & 0x00FFFFFF) << 8;
	pio_sm_put_blocking(pio0, state_machine, 0x00000037);
	pio_sm_put_blocking(pio0, state_machine, 0x02000000 | addr);
	pio_sm_put_blocking(pio0, state_machine, data);
}

uint32_t load_dac(uint32_t addr){
	pio_sm_put_blocking(pio0, state_machine, 0x03000000 | addr);
	pio_sm_put_blocking(pio0, state_machine, 0x00000017);
	return pio_sm_get_blocking(pio0, state_machine);
}

uint32_t read_id(){
	setup_read();
	pio_sm_put_blocking(pio0, state_machine, 0x9F000000);
	pio_sm_put_blocking(pio0, state_machine, 0x0000001F);
	return pio_sm_get_blocking(pio0, state_machine);
}

int main(){
	// Set CS high on startup
	gpio_init(CS_n_PIN);
	gpio_set_dir(CS_n_PIN, GPIO_OUT);
	gpio_put(CS_n_PIN, 1);

	stdio_init_all();
	gpio_init(LED_PIN);
	gpio_set_dir(LED_PIN, GPIO_OUT);
	sleep_ms(2000);
	gpio_put(LED_PIN, 1);

	printf("Reset\n");
	reset();
	sleep_us(200);

	while(1){
		printf("Loading data to RAM\n");
		setup_write();
		for(int i = 0; i < 1000; i++){
			write_voltage(i*3, (float) (999 - i) / 1000.0 * V_MAX);
		}
		for(int i = 0; i < 1000; i++){
			write_voltage(i*3 + 1000*3, (float) i / 1000.0 * V_MAX);
		}

		printf("Sending data to output\n");
		setup_read();
		for(int i = 0; i < 2000; i++){
			load_dac(i*3);
		}

		sleep_ms(500);
	}

	return 0;
}
