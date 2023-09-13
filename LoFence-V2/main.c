#include <atmel_start.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include "la66.h"
#include "variable_delay.h"
#include "main.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

uint32_t EEMEM tdc = INTERVAL_SECONDS; // transmit duty cycle
uint16_t EEMEM msr_ms = MEASURE_MS;
uint16_t EEMEM max3v3_volt = 12000; // theoretical maximum value

volatile uint32_t seconds = 0;

volatile uint8_t adc_clear = 0;
volatile uint8_t adc_max = 0;
volatile uint8_t adc_min = 0;
uint8_t adc_val = 0;

char buffer_info[LA66_MAX_BUFF];
char buffer_la[LA66_MAX_BUFF];

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

void power_save(uint32_t sec)
{
	seconds = 0;
	sleep_enable();
	while (seconds <= sec)
	{
		sleep_mode();
	}
	sleep_disable();
}

void log_serial(char *msg)
{
	for (uint8_t i = 0; i < strlen(msg); i++)
	{
		while (!USART_1_is_tx_ready()) {}
		USART_1_write(msg[i]);
	}
	while (USART_1_is_tx_busy()) {}
}

// ----------------------------------------------------------------------------------------------

void adc_init() {
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
	
	log_serial("Measuring...\r\n");
	
	// ----------------------------------------------------------------------------------------------

	PRR0 &= ~(1 << PRADC); // Enable ADC
	ADC_POWER_set_level(true);
	_delay_ms(1000);
	
	// ----------------------------------------------------------------------------------------------
	
	log_serial("Measuring battery: ");

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

	sprintf(buffer_info, "%d mV\r\n", volt_bat);
	log_serial(buffer_info);

	// ----------------------------------------------------------------------------------------------

	log_serial("Measuring fence positive: ");
	
	ADMUX = (ADMUX & 0xE0) | (1 << MUX1); // Pin 2
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_100ms(eeprom_read_word(&msr_ms));
	ADCSRA &= ~(1 << ADEN);

	volt_fence_plus = (eeprom_read_word(&max3v3_volt)/255*adc_max);

	sprintf(buffer_info, "%d V\r\n", volt_fence_plus);
	log_serial(buffer_info);

	// ----------------------------------------------------------------------------------------------

	log_serial("Measuring fence negative: ");

	ADMUX = (ADMUX & 0xE0); // Pin 0
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_100ms(eeprom_read_word(&msr_ms));
	ADCSRA &= ~(1 << ADEN);

	volt_fence_minus = (eeprom_read_word(&max3v3_volt)/255*adc_max);

	sprintf(buffer_info, "%d V\r\n", volt_fence_minus);
	log_serial(buffer_info);

	// ----------------------------------------------------------------------------------------------

	ADC_POWER_set_level(false);
	PRR0 |= (1 << PRADC); // Disable ADC

	LED_MSR_set_level(false);
}

void transmit() {
	LED_TX_set_level(true);
	
	uint8_t fPort = 1;
	uint8_t rxSize = 0;
	
	log_serial("Transmitting...\r\n");
	
	sprintf(buffer_la, "%04X%04X%04X", volt_bat, volt_fence_plus, volt_fence_minus);
	
	if (LA66_transmitB(&fPort, false, buffer_la, &rxSize) == LA66_SUCCESS)
	{
		switch (buffer_la[0] & 0xFF)
		{
			case 0x01:
			{
				if (rxSize == 4)
				{
					eeprom_write_dword(&tdc, ((uint32_t)buffer_la[1]<<16 | buffer_la[2]<<8 | buffer_la[3]));
				}
				break;
			}
			case 0x10:
			{
				if (rxSize == 3)
				{
					eeprom_write_word(&msr_ms, (buffer_la[1]<<8 | buffer_la[2]));
				}
				break;
			}
			case 0x11:
			{
				if (rxSize == 3)
				{
					eeprom_write_word(&max3v3_volt, (buffer_la[1]<<8 | buffer_la[2]));
				}
				break;
			}
		}
	}
	
	LED_TX_set_level(false);
}

void pause() {
	LED_IDLE_set_level(true);
	
	sprintf (buffer_info, "Sleeping for %lu seconds\r\n", eeprom_read_dword(&tdc));
	log_serial(buffer_info);
	_delay_ms(500);
	
	power_save(eeprom_read_dword(&tdc) + (rand() % (RANDOMNESS * 2) - RANDOMNESS));
	
	LED_IDLE_set_level(false);
}

// ----------------------------------------------------------------------------------------------

int main(void) {

	atmel_start_init();
	
	LED_IDLE_set_level(true);
	LED_MSR_set_level(true);
	LED_TX_set_level(true);

	log_serial("\r\n");
	log_serial("LoFence-V2 v0.1 by Alex9779\r\n");
	log_serial("https://github.com/alex9779/lofence-v2\r\n");
	log_serial("\r\n");
	
	LED_IDLE_set_level(false);

	log_serial("Initializing ADC...\r\n");
	adc_init();
	LED_MSR_set_level(false);
	
	log_serial("Activating LA66 module...\r\n");
	LA66_reset();
	
	log_serial("Waiting to join network...\r\n");
	if (LA66_waitForJoin() == LA66_ERR_PANIC)
	{
		LA66_deactivate();
		
		while (1)
		{
			LED_TX_toggle_level();
			_delay_ms(100);
		}
	}
	LED_TX_set_level(false);
	
	while (1)
	{
		measure();

		transmit();
		
		pause();
	}
	
}