#include <atmel_start.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include "la66.h"
#include "variable_delay.h"
#include "main.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

uint32_t EEMEM tdc = INTERVAL_SECONDS;
uint16_t EEMEM msr_ms = MEASURE_MS;
uint16_t EEMEM max_volt = MAXIMUM_FENCE_VOLTAGE;
uint16_t EEMEM bat_low = BATTERY_LOW_THRESHOLD;
uint8_t EEMEM bat_low_count_max = BATTERY_LOW_MAX_CYCLES;
uint16_t EEMEM bat_low_min = BATTERY_ABSOLUTE_MINIMUM;

volatile uint32_t seconds = 0;

volatile uint8_t adc_clear = 0;
volatile uint8_t adc_max = 0;
volatile uint8_t adc_min = 0;
uint8_t adc_val = 0;

char buffer_info[LA66_MAX_BUFF];
LA66_buffer buffer_la;
LA66_ReturnCode last_error = 0;

uint16_t volt_bat = 0;
uint16_t volt_fence_plus = 0;
uint16_t volt_fence_minus = 0;

uint8_t settings = 0;
uint16_t daily_cycle_count = 0;

uint8_t bat_low_count = 0;
uint8_t bisect_pause_count = 0;
bool do_deactivate = false;

// ----------------------------------------------------------------------------------------------

ISR(TIMER2_OVF_vect)
{
	// see https://www.mikrocontroller.net/articles/AVR-GCC-Tutorial/Die_Timer_und_Z%C3%A4hler_des_AVR#Timer2_im_Asynchron_Mode
	TCCR2B = TCCR2B;
	seconds++;
	LED_CLK_toggle_level();
	while (ASSR & ((1 << TCN2UB) | (1 << OCR2AUB) | (1 << OCR2BUB) | (1 << TCR2AUB) | (1 << TCR2BUB)));
}

ISR(ADC_vect)
{
	adc_val = ADCH;
	if (adc_clear == 1)
	{
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

void log_serial(const char *msg)
{
	for (uint8_t i = 0; i < strlen(msg); i++)
	{
		while (!USART_1_is_tx_ready())
		{
		}
		USART_1_write(msg[i]);
	}
	while (USART_1_is_tx_busy())
	{
	}
}

void log_serial_P(const char *msg)
{
	for (uint8_t i = 0; i < strlen_P(msg); i++)
	{
		while (!USART_1_is_tx_ready())
		{
		}
		USART_1_write(pgm_read_byte(&(msg[i])));
	}
	while (USART_1_is_tx_busy())
	{
	}
}

// ----------------------------------------------------------------------------------------------

void adc_init()
{
	PRR0 &= ~(1 << PRADC); // Enable

	DIDR0 = (1 << ADC0D) | (1 << ADC2D) | (1 << ADC4D); // Disable input buffer

	ADMUX = 0x00;
	ADMUX |= (0 << REFS1) | (1 << REFS0); // AVCC with external capacitor at AREF pin
	ADMUX |= (1 << ADLAR);				  // Left Adjust Result: enabled

	ADCSRA = 0x00;
	ADCSRA |= (1 << ADATE); // Auto Trigger: enabled
	ADCSRA |= (1 << ADIE);	// ADC Interrupt: enabled

	// Safe range: 50khz - 200khz
	// ADCSRA |= (1 << ADPS2) | (0 << ADPS1) | (0 << ADPS0); // 100  16 500khz
	ADCSRA |= (1 << ADPS2) | (0 << ADPS1) | (1 << ADPS0); // 101  32 250khz
	// ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); // 110  64 125khz
	// ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 111 128  62khz

	ADCSRB = 0x00;
	ADCSRB |= (0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // Free Running mode
	ADCSRB |= (0 << ACME);								  // Analog Comparator Multiplexer: disabled

	ACSR |= (1 << ACD); // Disable Comparator

	ADCSRA &= ~(1 << ADEN); // Disable ADC
	PRR0 |= (1 << PRADC);	// Disable ADC
}

void measure()
{
	LED_MSR_set_level(true);

	log_serial_P(PSTR("Measuring...\r\n"));

	// ----------------------------------------------------------------------------------------------

	PRR0 &= ~(1 << PRADC); // Enable ADC
	ADC_POWER_set_level(true);
	_delay_ms(1000);

	// ----------------------------------------------------------------------------------------------

	log_serial_P(PSTR("Measuring battery: "));

	BAT_GND_set_level(false);
	_delay_ms(1000);

	ADMUX = (ADMUX & 0xE0) | (1 << MUX2); // Pin 4
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_ms(500);
	ADCSRA &= ~(1 << ADEN);
	BAT_GND_set_level(true);

	volt_bat = (((330000 / 255 * adc_min * 2) - 0)) / 100 + 125;
	// 125mV is the measured voltage dfrop of the Schottky diode

	snprintf_P(buffer_info, sizeof(buffer_info), PSTR("%d mV\r\n"), volt_bat);
	log_serial(buffer_info);

	// ----------------------------------------------------------------------------------------------

	log_serial_P(PSTR("Measuring fence positive: "));

	ADMUX = (ADMUX & 0xE0) | (1 << MUX1); // Pin 2
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_100ms(eeprom_read_word(&msr_ms));
	ADCSRA &= ~(1 << ADEN);

	volt_fence_plus = (eeprom_read_word(&max_volt) / 255 * adc_max);
	
	snprintf_P(buffer_info, sizeof(buffer_info), PSTR("%d V\r\n"), volt_fence_plus);
	log_serial(buffer_info);

	// ----------------------------------------------------------------------------------------------

	log_serial_P(PSTR("Measuring fence negative: "));

	ADMUX = (ADMUX & 0xE0); // Pin 0
	ADCSRA |= (1 << ADEN);
	ADCSRA |= (1 << ADSC);
	adc_clear = 1;
	_delay_100ms(eeprom_read_word(&msr_ms));
	ADCSRA &= ~(1 << ADEN);

	volt_fence_minus = (eeprom_read_word(&max_volt) / 255 * adc_max);

	snprintf_P(buffer_info, sizeof(buffer_info), PSTR("%d V\r\n"), volt_fence_minus);
	log_serial(buffer_info);

	// ----------------------------------------------------------------------------------------------

	ADC_POWER_set_level(false);
	PRR0 |= (1 << PRADC); // Disable ADC

	LED_MSR_set_level(false);
}

void reset_join()
{
	LED_TX_set_level(true);
			
	log_serial_P(PSTR("Resetting LA66 module...\r\n"));
	LA66_reset();

	log_serial_P(PSTR("Waiting to join network...\r\n"));
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
}

void handle_downlink(uint8_t *rxSize)
{
	log_serial_P(PSTR("Downlink received...\r\n"));

	switch (buffer_la[0] & 0xFF)
	{
		case 0x01: // transmit duty cycle
		{
			if (*rxSize == 4)
			{
				eeprom_write_dword(&tdc, ((uint32_t)buffer_la[1] << 16 | buffer_la[2] << 8 | buffer_la[3]));
				
				// reset interval for recurring settings uplinks
				daily_cycle_count = 0;
			}
			break;
		}
		case 0x04: // reset LA66
		{
			if (*rxSize == 1)
			{
				reset_join();
			}
			break;
		}
		case 0x10: // measurement delay for each pole
		{
			if (*rxSize == 3)
			{
				eeprom_write_word(&msr_ms, (buffer_la[1] << 8 | buffer_la[2]));
			}
			break;
		}
		case 0x11: // maximum fence voltage at ADC max, this depends on actual resistor values
		{
			if (*rxSize == 3)
			{
				eeprom_write_word(&max_volt, (buffer_la[1] << 8 | buffer_la[2]));
			}
			break;
		}
		case 0x12: // battery low voltage
		{
			if (*rxSize == 3)
			{
				eeprom_write_word(&bat_low, (buffer_la[1] << 8 | buffer_la[2]));
			}
			break;
		}
		case 0x13: // battery low cycle count
		{
			if (*rxSize == 2)
			{
				eeprom_write_byte(&bat_low_count_max, buffer_la[1]);
			}
			break;
		}
		case 0x14: // battery low minimum voltage
		{
			if (*rxSize == 3)
			{
				eeprom_write_word(&bat_low_min, (buffer_la[1] << 8 | buffer_la[2]));
			}
			break;
		}
		case 0xFF: // transmit settings next cycle
		{
			if (*rxSize == 2)
			{
				settings = buffer_la[1];
				
				if (settings > 0 && settings <= 2)
				{
					if (eeprom_read_dword(&tdc) >= 60)
					{
						// see calc_recurring_settings() comment for this euqal assignement
						bisect_pause_count = 3;
					}
				}
				// discard out of range commands
				else if (settings > 2)
				{
					settings = 0;
				}
				
			}
			break;
		}
	}
}

void handle_error(LA66_ReturnCode ret)
{
	switch (ret)
	{
		case LA66_SUCCESS:
		case LA66_NODOWN:
		{
			break;
		}
		
		case LA66_ERR_PANIC:
		case LA66_ERROR:
		case LA66_ERR_PARAM:
		case LA66_ERR_JOIN:
		case LA66_ERR_BUSY:
		case LA66_EOB:
		{
			last_error = ret;
			break;
		}
	}
}

void transmit_data(const bool confirm)
{
	LED_TX_set_level(true);

	uint8_t fPort = 1;
	uint8_t rxSize = 0;

	log_serial_P(PSTR("Transmitting data...\r\n"));

	snprintf_P(buffer_la, sizeof(buffer_la), PSTR("%04X%04X%04X"), volt_bat, volt_fence_plus, volt_fence_minus);

	LA66_ReturnCode ret = LA66_transmitB(&fPort, confirm, buffer_la, &rxSize);
	
	switch (ret)
	{
		case LA66_SUCCESS:
		{
			handle_downlink(&rxSize);
			LED_TX_set_level(false);
			break;
		}
		
		case LA66_NODOWN:
		{
			LED_TX_set_level(false);
			break;
		}
		
		default:
		{
			handle_error(ret);
			break;
		}
	}
}

void transmit_settings(const bool confirm)
{
	LED_TX_set_level(true);
	
	uint8_t fPort = settings + 1;
	uint8_t rxSize = 0;
	
	log_serial_P(PSTR("Transmitting settings...\r\n"));
	
	switch (settings)
	{
		case 1:
		snprintf_P(buffer_la, sizeof(buffer_la), PSTR("%02X%06lX%04X%04X"), VERSION, eeprom_read_dword(&tdc), eeprom_read_word(&msr_ms), eeprom_read_word(&max_volt));
		break;
		
		case 2:
		snprintf_P(buffer_la, sizeof(buffer_la), PSTR("%02X%04X%02X%04X"), VERSION, eeprom_read_word(&bat_low), eeprom_read_byte(&bat_low_count_max), eeprom_read_word(&bat_low_min));
		break;
	}
	
	settings = 0;
	
	LA66_ReturnCode ret = LA66_transmitB(&fPort, confirm, buffer_la, &rxSize);

	switch (ret)
	{
		case LA66_SUCCESS:
		{
			handle_downlink(&rxSize);
			LED_TX_set_level(false);
			break;
		}
		
		case LA66_NODOWN:
		{
			LED_TX_set_level(false);
			break;
		}
		
		default:
		{
			handle_error(ret);
			break;
		}
	}
}

void transmit_error(const bool confirm)
{
	LED_TX_set_level(true);
	
	uint8_t fPort = 223;
	uint8_t rxSize = 0;
	
	log_serial_P(PSTR("Transmitting error...\r\n"));
	
	snprintf_P(buffer_la, sizeof(buffer_la), PSTR("%02X"), last_error);
	
	LA66_ReturnCode ret = LA66_transmitB(&fPort, confirm, buffer_la, &rxSize);

	switch (ret)
	{
		case LA66_SUCCESS:
		{
			handle_downlink(&rxSize);
			LED_TX_set_level(false);
			break;
		}
		
		case LA66_NODOWN:
		{
			LED_TX_set_level(false);
			break;
		}
		
		default:
		{
			handle_error(ret);
			break;
		}
	}
}

void calc_recurring_settings()
{
	daily_cycle_count++;

	uint16_t max = (uint32_t)24 * 60 * 60 / (eeprom_read_dword(&tdc) + 2 * eeprom_read_word(&msr_ms) / 1000);

	// settings_interval is 1/3 of daily max uplink count
	if (daily_cycle_count == max * 1 / 3)
	{
		settings = 1;
	}
	// settings_interval is 2/3 of daily max uplink count
	else if (daily_cycle_count == max * 2 / 3)
	{
		settings = 2;
	}
	// settings_interval is daily max uplink count
	else if (daily_cycle_count == max)
	{
		daily_cycle_count = 0;
	}
	
	// if settings are schedule for next cycle and TDC is greater than 1 minute bisect pause for 3 cycles
	// in current cycle, which will send normal data, when calling pause count will reduce it to 2
	// next cycle after TDC/2 will send settings, count will be reduced to 1
	// next cycle after TDC/2 will send normal data, count will be 0 and the following cycle will occur after normal TDC
	if (eeprom_read_dword(&tdc) >= 60 && settings > 0)
	{
		bisect_pause_count = 3;
	}
}

void check_battery()
{
	// if maximum cycles the battery has been low is not reached
	// and if the battery is above the absolute minimum of 3100mV
	if (bat_low_count < eeprom_read_byte(&bat_low_count_max) && volt_bat > bat_low_min && volt_bat < eeprom_read_word(&bat_low))
	{
		bat_low_count++;
	}
	// if battery is lower than absolute minimum
	else if (volt_bat <= bat_low_min)
	{
		// set deactivation flag
		do_deactivate = true;
	}
	// if counter reached and battery still low
	else if (bat_low_count >= eeprom_read_byte(&bat_low_count_max) && volt_bat < eeprom_read_word(&bat_low))
	{
		// set deactivation flag
		do_deactivate = true;
	}

	// Deactivation is postponed to next cycle because it triggers an uplink
	// with zero battery voltage and doing so right after a real measurement
	// could be problematic.
	// The uplink will contain the same fence values as the previous
	// to prevent triggering false alarms.
}

void deactivate()
{
	LED_IDLE_set_level(false);
	LED_MSR_set_level(false);
	LED_TX_set_level(false);

	sleep_set_mode(2); // set to power down mode

	ACTIVATE_set_level(false);

	sleep_enable();
	while (1)
	{
		sleep_mode();
	}
}

void pause()
{
	LED_IDLE_set_level(true);
	
	if (bisect_pause_count > 0) bisect_pause_count--;

	snprintf_P(buffer_info, sizeof(buffer_info), PSTR("Sleeping for %lu seconds...\r\n"), eeprom_read_dword(&tdc) / (bisect_pause_count > 0 ? 2 : 1));
	log_serial(buffer_info);
	_delay_ms(500);
	
	power_save((eeprom_read_dword(&tdc) + (rand() % (RANDOMNESS * 2) - RANDOMNESS)) / (bisect_pause_count > 0 ? 2 : 1));

	LED_IDLE_set_level(false);
}

// ----------------------------------------------------------------------------------------------

int main(void)
{

	atmel_start_init();

	LED_IDLE_set_level(true);
	LED_MSR_set_level(true);
	LED_TX_set_level(true);

	log_serial_P(PSTR("\r\n"));
	log_serial_P(PSTR("LoFence-V2 v1.1 by Alex9779\r\n"));
	log_serial_P(PSTR("https://github.com/alex9779/lofence-v2\r\n"));
	log_serial_P(PSTR("\r\n"));

	_delay_ms(1000);

	ACTIVATE_set_level(true);

	LED_IDLE_set_level(false);

	log_serial_P(PSTR("Initializing ADC...\r\n"));
	adc_init();
	LED_MSR_set_level(false);

	reset_join();

	while (1)
	{
		// check for pending deactivation
		if (do_deactivate)
		{
			volt_bat = 0;

			// try to transmit data confirmed
			transmit_data(true);
			
			deactivate();
		}
		
		// if previous cycle threw an error
		if (last_error != 0)
		{
			reset_join();
			
			transmit_error(true);
			
			last_error = 0;
		}
		// normal cycle
		else if (settings == 0)
		{
			calc_recurring_settings();
			
			measure();
			
			transmit_data(false);
		}
		// settings requested
		else
		{
			transmit_settings(false);
		}

		check_battery();
		
		pause();
	}
}