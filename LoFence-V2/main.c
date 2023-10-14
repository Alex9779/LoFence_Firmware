#include <atmel_start.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include "main.h"

#define INTERVAL_SECONDS 5 * 60 // time to sleep between measurements
#define RANDOMNESS 10 // +- time to sleep between measurements
#define MEASURE_MS 6000 // time in ms a measurement should take (per polarity)

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

const uint16_t max3v3_volt = 12000; // theoretical value

volatile uint32_t seconds = 0;

volatile uint8_t adc_clear = 0;
volatile uint8_t adc_max = 0;
volatile uint8_t adc_min = 0;
uint8_t adc_val = 0;

char buffer_info[256];
char buffer_la[256];

uint16_t volt_bat = 0;
uint16_t volt_fence_plus = 0;
uint16_t volt_fence_minus = 0;

// ----------------------------------------------------------------------------------------------

ISR(TIMER2_OVF_vect) {
	// see https://www.mikrocontroller.net/articles/AVR-GCC-Tutorial/Die_Timer_und_Z%C3%A4hler_des_AVR#Timer2_im_Asynchron_Mode
	TCCR2B = TCCR2B;
	seconds++;
	LED_CLK_toggle_level();
	while(ASSR & ((1<<TCN2UB) | (1<<OCR2AUB) | (1<<OCR2BUB) | (1<<TCR2AUB) | (1<<TCR2BUB)));
}

ISR(ADC_vect) {
	adc_val = ADCH;
	if (adc_clear == 1) {
		adc_clear = 0;
		adc_max = 0x00;
		adc_min = 0xFF;
	}
	adc_max = MAX(adc_max, adc_val);
	adc_min = MIN(adc_min, adc_val);
}

// ----------------------------------------------------------------------------------------------

void la66_rx_clear() {
	info("LA66 RX clearing\r\n");
	
	while(USART_0_is_rx_ready()) {
		while(USART_0_is_rx_ready()) {
			USART_0_read();
		}
		_delay_ms(100);
	}
}

void la66_init() {
	info("LA66 initialization\r\n");

	LA_RESET_set_level(false);
	_delay_ms(100);
	LA_RESET_set_level(true);
	_delay_ms(5000);	

	for (uint8_t i = 0; i < 100; i++) {
		if (i == 99) {
			la66_init_error();
		}
		
		la66_rx_clear();
			
		sprintf(buffer_la, "AT+NJS=?\r\n");
		la66_tx(buffer_la);
		
		la66_rx(false);
		if (strcmp(buffer_la, "1") == 0) {
			la66_rx(false);
			break;
		}
		la66_rx(false);

		_delay_ms(500);
	}
}

void la66_init_error() {
	info("Aborting initialization\r\n");

	LA_RESET_set_level(false);
	
	while(1) {
		LED_TX_set_level(true);
		_delay_ms(100);
		LED_TX_set_level(false);
		_delay_ms(100);
	}
}

void la66_tx_error() {
	info("Aborting transmission\r\n");

	LA_RESET_set_level(false);
	
	for (uint8_t i = 0; i < 20; i++) {
		LED_TX_set_level(true);
		_delay_ms(300);
		LED_TX_set_level(false);
		_delay_ms(300);
	}
}

void la66_tx(char buf[]) {
	info("LA66 TX: ");
	info(buf);
	
	for (uint8_t i = 0; i < strlen(buf); i++) {
		while (!USART_0_is_tx_ready()) {}
		USART_0_write(buf[i]);
	}
	while (USART_0_is_tx_busy()) {}
}

void la66_rx(bool rec) {
	char nc = 0x00;
	uint8_t len = 0;

	for (; len < 255; len++) {
		nc = USART_0_read();
		if (nc == '\n' || nc == '\r') {
			break;
		}
		buffer_la[len] = nc;
	}
	buffer_la[len] = '\0';
	
	if (len == 0) {
		la66_rx(true);
	}
	
	if (!rec) {
		info(buffer_la);
		info("\r\n");
	}
}

// ----------------------------------------------------------------------------------------------

void power_save(uint32_t sec) {
	seconds = 0;
	sleep_enable();
	while (seconds <= sec) {
		sleep_mode();
	}
	sleep_disable();
}

void info(char buf[]) {
	for (uint8_t i = 0; i < strlen(buf); i++) {
		while (!USART_1_is_tx_ready()) {}
		USART_1_write(buf[i]);
	}
	while (USART_1_is_tx_busy()) {}
}

// ----------------------------------------------------------------------------------------------

void adc_init() {
	info("ADC initialization\r\n");

	PRR0 &= ~(1 << PRADC); // Enable
	
	DIDR0 = (1 << ADC0D) | (1 << ADC2D) | (1 << ADC4D); // Disable input buffer
	
	ADMUX = 0x00;
	ADMUX |= (0 << REFS1) | (1 << REFS0); // AVCC with external capacitor at AREF pin
	ADMUX |= (1 << ADLAR); // Left Adjust Result: enabled
	
	ADCSRA = 0x00;
	ADCSRA |= (1 << ADATE); // Auto Trigger: enabled
	ADCSRA |= (1 << ADIE); // ADC Interrupt: enabled
	
	// Safe range: 50khz - 200khz
	//ADCSRA |= (1 << ADPS2) | (0 << ADPS1) | (0 << ADPS0); // 100  16 500khz
	ADCSRA |= (1 << ADPS2) | (0 << ADPS1) | (1 << ADPS0); // 101  32 250khz
	//ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); // 110  64 125khz
	//ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 111 128  62khz
	
	ADCSRB = 0x00;
	ADCSRB |= (0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // Free Running mode
	ADCSRB |= (0 << ACME); // Analog Comparator Multiplexer: disabled

	ACSR |= (1 << ACD); // Disable Comparator

	ADCSRA &= ~(1 << ADEN); // Disable ADC
	PRR0 |= (1 << PRADC); // Disable ADC	
}

void measure() {
	LED_MSR_set_level(true);
	
	info("Measuring\r\n");
	
	// ----------------------------------------------------------------------------------------------

	PRR0 &= ~(1 << PRADC); // Enable ADC
	ADC_POWER_set_level(true);
	_delay_ms(1000);
	
	// ----------------------------------------------------------------------------------------------
		
	info("Measuring battery: ");

	BAT_GND_set_level(false);
	_delay_ms(1000);

	ADMUX = (ADMUX & 0xE0) | (1 << MUX2); // Pin 4
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_ms(500);
	ADCSRA &= ~(1 << ADEN);
	BAT_GND_set_level(true);

	volt_bat = (((330000/255*adc_min*2) - 0))/100;

	sprintf (buffer_info, "%d mV\r\n", volt_bat);
	info(buffer_info);

	// ----------------------------------------------------------------------------------------------

	info("Measuring fence positive: ");
	
	ADMUX = (ADMUX & 0xE0) | (1 << MUX1); // Pin 2	
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_ms(MEASURE_MS);
	ADCSRA &= ~(1 << ADEN);

	volt_fence_plus = (max3v3_volt/255*adc_max);

	sprintf (buffer_info, "%d V\r\n", volt_fence_plus);
	info(buffer_info);

	// ----------------------------------------------------------------------------------------------

	info("Measuring fence negative: ");

	ADMUX = (ADMUX & 0xE0); // Pin 0
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_ms(MEASURE_MS);
	ADCSRA &= ~(1 << ADEN);

	volt_fence_minus = (max3v3_volt/255*adc_max);

	sprintf (buffer_info, "%d V\r\n", volt_fence_minus);
	info(buffer_info);

	// ----------------------------------------------------------------------------------------------

	ADC_POWER_set_level(false);
	PRR0 |= (1 << PRADC); // Disable ADC

	LED_MSR_set_level(false);
}

void transmit() {
	LED_TX_set_level(true);
		
	info("Transmitting\r\n");	

	la66_rx_clear();

	sprintf(buffer_la, "AT+SENDB=0,1,6,%04X%04X%04X\r\n", volt_bat, volt_fence_plus, volt_fence_minus);
	la66_tx(buffer_la);
	la66_rx(false);
	la66_rx(false);
	la66_rx(false);
	if (strcmp(buffer_la, "OK") != 0) {
		la66_tx_error();
		return;
	}
	la66_rx(false);
	if (strcmp(buffer_la, "txDone") != 0) {
		la66_tx_error();
		return;
	}
	
	LED_TX_set_level(false);
}

void pause() {
	LED_IDLE_set_level(true);
		
	sprintf (buffer_info, "Sleeping for %d seconds\r\n", INTERVAL_SECONDS);
	info(buffer_info);
	_delay_ms(500);
	
	power_save(INTERVAL_SECONDS +(rand() % (RANDOMNESS * 2) - RANDOMNESS));
	
	LED_IDLE_set_level(false);
}

// ----------------------------------------------------------------------------------------------

int main(void) {

	atmel_start_init();

	LED_IDLE_set_level(true);
	LED_MSR_set_level(true);
	LED_TX_set_level(true);

	info("\r\n");
	info("LoFence-V2 v0.1 by alex9779\r\n");
	info("https://github.com/alex9779/lofence-v2\r\n");
	info("Lets get started!\r\n");
	info("\r\n");
	
	LED_IDLE_set_level(false);

	adc_init();
	LED_MSR_set_level(false);

	la66_init();
	LED_TX_set_level(false);
	
	while (1) {
		
		measure();

		transmit();
		
		pause();
	}
	
}