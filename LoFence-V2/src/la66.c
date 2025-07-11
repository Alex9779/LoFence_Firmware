/*!
@file	LA66.c
@author	Alexander Leisentritt (alexander.leisentritt@alitecs.de)
@date	August 2023 - September 2023
@brief	A library for embedded platforms that allows for interaction with a Dragino LA66.

@see LA66.h
*/
//========
// MACROS
//========
// includes
#include "la66.h"
#include "variable_delay.h"

#ifdef DEBUG
char debug[32 + LA66_MAX_BUFF];
#endif

//===========
// FUNCTIONS
//===========
// PRIVATE
//! Convert string with byte values in hex text format to buffer with hex values
// e. g. "0100012C" --> { 0x01, 0x00, 0x01, 0x2C}
static void readHex(char* buf, char* txt)
{
	char b[3] = "00";
	
	for (uint8_t i = 0; i < strlen(txt); i += 2)
	{
		b[0] = *(txt + i);
		b[1] = *(txt + i + 1);
		*(buf + (i >> 1)) = strtoul(b, NULL, 16);
	}
}

//! Read a single byte from via UART from the LA66
// while there is data, return null char if not
static char read(const bool wait)
{
	uint16_t timeout = 100;

	// wait for data to be available
	while (!USART_0_is_rx_ready() && wait && timeout--)
	{
		_delay_ms(1);
	}

	if (USART_0_is_rx_ready())
	{
		return USART_0_read();
	}
	else
	{
		return '\0';
	}
}

//! Write a string of bytes via UART to the LA66
static void write(const char *buff)
{
	for (uint8_t i = 0; i < strlen(buff); i++)
	{
		while (!USART_0_is_tx_ready()) {}
		USART_0_write(buff[i]);
	}
	while (USART_0_is_tx_busy()) {}
}

//! Clear LA66 RX buffer.
static void clear_read()
{
	while(USART_0_is_rx_ready()) {
		while(USART_0_is_rx_ready()) {
			USART_0_read();
		}
		_delay_ms(100);
	}
}

// Reads from the RX buffer into line until '\n' or '\r' or EOB.
// '\n' or '\r' are not always returned in the same order.
// Line does not contain '\n' or '\r'.
// Returns length of line.
static uint8_t read_line(char *line)
{
	// initialize line length counter
	uint8_t i = 0;
	bool wait = false;
	char *p = line;
	
	memset(line, 0, LA66_MAX_BUFF);
	
	while (i < LA66_MAX_BUFF)
	{
		// get char and write it to current line position
		*p = read(wait);

		// increase line length counter
		i++;
		
		// test current char
		if (*p == '\n' || *p == '\r')
		{
			*p = '\0';
			i -= 1;
			break;
		}
		else if (*p == '\0')
		{
			i = 0;
			break;
		}
		
		wait = true;
		
		// increase pointer value
		*p++;
	}
	
	#ifdef DEBUG
	if (i > 0)
	{
		snprintf_P(debug, sizeof(debug), PSTR("DBG line: %s\r\n"), line);
		log_serial(debug);
	}
	#endif

	return i;
}

// Sends a command to the LA66.
// No response is read.
static LA66_ReturnCode send_command(const char *command)
{
	uint8_t end = strlen(command);

	// check command ends with \r\n (easy to forget)
	if (command[end - 2] != '\r' || command[end - 1] != '\n')
	{
		return LA66_ERR_PARAM;
	}
	
	#ifdef DEBUG
	snprintf_P(debug, sizeof(debug), PSTR("DBG Sending command: %s"), command);
	log_serial(debug);
	#endif
	
	// clear the UART buffer just in case
	clear_read();
	
	// send command
	write(command);
	
	_delay_ms(10);
	
	return LA66_SUCCESS;
}

// PUBLIC
// Sends a query command to the LA66 and sets the response.
LA66_ReturnCode LA66_query_command_P(const char *command, char *response)
{
	LA66_ReturnCode ret = LA66_ERR_PANIC;
	char rCommand[strlen_P(command)];
	char response2[LA66_MAX_BUFF];
	
	strcpy_P(rCommand, command);
	
	ret = send_command(rCommand);
	
	if (ret == LA66_SUCCESS)
	{
		// receive first command response
		for (uint32_t i = 0; i < LA66_COMMAND_TIMEOUT * 100L; i++)
		{
			if (read_line(response) > 0)
			{
				break;
			}
			
			_delay_ms(10);
		}
		
		if (strcmp_P(response, PSTR(AT_ERROR)) == 0)
		{
			return LA66_ERROR;
		}
		else if (strcmp_P(response, PSTR(AT_PARAM_ERROR)) == 0)
		{
			return LA66_ERR_PARAM;
		}
		else if (strcmp_P(response, PSTR(AT_BUSY_ERROR)) == 0)
		{
			return LA66_ERR_BUSY;
		}
		else if (strcmp_P(response, PSTR(AT_NO_NET_JOINED)) == 0)
		{
			return LA66_ERR_JOIN;
		}
		
		bool stop = false;

		// receive second command response
		for (uint32_t i = 0; i < LA66_COMMAND_TIMEOUT * 100L; i++)
		{
			while (read_line(response2) > 0)
			{
				if (strcmp_P(response2, PSTR(AT_OK)) == 0)
				{
					ret = LA66_SUCCESS;
					stop = true;
					break;
				}
			}
			
			if (stop)
			{
				break;
			}
			
			_delay_ms(10);
		}
	}
	
	return ret;
}

// Resets the LA66 by toggling the RESET pin
void LA66_reset()
{
	LA66_deactivate();
	
	LA66_activate();
}

// Activates the LA66 by enabling the RESET pin
void LA66_activate()
{
	LA_RESET_set_level(true);
	
	_delay_ms(1000);
}

// Deactivates the LA66 by disabling the RESET pin
void LA66_deactivate()
{
	LA_RESET_set_level(false);
	
	_delay_ms(100);
}


// The LA66 automatically tries to join a network when activated.
// Wait for joined a network.
LA66_ReturnCode LA66_waitForJoin(void (*led_toggle_func)(void))
{
	LA66_ReturnCode ret = LA66_ERR_PANIC;
	char response[LA66_MAX_BUFF];
	bool joined = false;
	uint16_t blink_counter = 0;
	bool blink = false;
	
	for (uint32_t i = 0; i < LA66_JOIN_TIMEOUT * 100L; i++)
	{
		if (read_line(response) > 0)
		{
			blink = true;

			if (strcmp_P(response, PSTR("JOINED")) == 0)
			{
				log_serial_P(PSTR("Joined network!\r\n"));
				joined = true;
				
				break;
			}
		}
		
		if (joined)
		{
			ret = LA66_SUCCESS;
			
			break;
		}

		if (blink)
		{
			// Blink LED every 500ms
			if (++blink_counter >= 50)
			{
				if (led_toggle_func)
					led_toggle_func();
				blink_counter = 0;
			}
		}

		_delay_ms(10);
	}
	
	if (!joined)
	{
		ret = LA66_ERR_JOIN;
		log_serial_P(PSTR("Unable to join network, timeout reached!\r\n"));
	}
	
	return ret;
}

// Get the current DR.
uint8_t LA66_getDr()
{
	LA66_buffer response;
	
	if (LA66_query_command_P(PSTR("AT+DR=?\r\n"), response) == LA66_SUCCESS)
	{
		return response[0] - '0';
	}
	
	return 0;
}

// Get RX1 delay.
uint16_t LA66_getRx1Dl()
{
	LA66_buffer response;
	
	if (LA66_query_command_P(PSTR("AT+RX1DL=?\r\n"), response) == LA66_SUCCESS)
	{
		return atoi(response);
	}
	
	return 0;
}

// Get RX2 delay.
uint16_t LA66_getRx2Dl()
{
	LA66_buffer response;
	
	if (LA66_query_command_P(PSTR("AT+RX2DL=?\r\n"), response) == LA66_SUCCESS)
	{
		return atoi(response);
	}
	
	return 0;
}

// Get timestamp.
uint32_t LA66_getTimestamp()
{
	LA66_buffer response;
	
	if (LA66_query_command_P(PSTR("AT+TIMESTAMP=?\r\n"), response) == LA66_SUCCESS)
	{
		char timestamp[10];
		
		strcpy(timestamp, strchr(response, '(') + 1);
		
		return atol(timestamp);
	}
	
	return 0;
}

// Sends a confirmed/unconfirmed frame with an application payload.
// Sets a recieved payload and its fPort and size.
LA66_ReturnCode LA66_transmitB(uint8_t *fPort, const bool confirm, char *payload, uint8_t *rxSize)
{
	LA66_ReturnCode ret = LA66_ERR_PANIC;
	char buffer[32 + LA66_MAX_BUFF];
	
	// Command format: AT+SENDB=<confirm>,<fPort>,<data_len>,<data>, example AT+SENDB=0,2,8,05820802581ea0a5
	snprintf_P(buffer, sizeof(buffer), PSTR("AT+SENDB=0%d,%u,%u,%s\r\n"), confirm, *fPort, strlen(payload) / 2, payload);
	
	// send command
	ret = send_command(buffer);
	
	// check if command was successful
	if (ret == LA66_SUCCESS)
	{
		// receive
		LA66_Stage stage = WAIT_FOR_OK;
		uint32_t timeout;

		if (confirm)
		{
			timeout = LA66_RX_CONF_TIMEOUT;
		}
		else
		{
			timeout = LA66_RX_TIMEOUT;
		}
		
		for (uint32_t i = 0; i < timeout * 100L; i++)
		{
			if (read_line(buffer) > 0)
			{
				if (stage == WAIT_FOR_OK)
				{
					if (strcmp_P(buffer, PSTR(AT_ERROR)) == 0)
					{
						ret = LA66_ERROR;
						break;
					}
					else if (strcmp_P(buffer, PSTR(AT_PARAM_ERROR)) == 0)
					{
						ret = LA66_ERR_PARAM;
						break;
					}
					else if (strcmp_P(buffer, PSTR(AT_BUSY_ERROR)) == 0)
					{
						ret = LA66_ERR_BUSY;
						break;
					}
					else if (strcmp_P(buffer, PSTR(AT_NO_NET_JOINED)) == 0)
					{
						ret = LA66_ERR_JOIN;
						break;
					}
					else
					{
						if (strcmp(buffer, AT_OK) == 0)
						{
							stage = WAIT_FOR_TX;
						}
					}
				}
				else if (stage == WAIT_FOR_TX)
				{
					if (strcmp_P(buffer, PSTR("txDone")) == 0)
					{
						stage = WAIT_FOR_RX;
						
						_delay_100ms(10);
					}
				}
				else if (stage == WAIT_FOR_RX || stage == WAIT_FOR_RX2)
				{
					if (strcmp_P(buffer, PSTR("rxDone")) == 0)
					{
						ret = LA66_SUCCESS;
						
						_delay_ms(100);
						break;
					}
					else if (!confirm && strcmp_P(buffer, PSTR("rxTimeout")) == 0)
					{
						if (stage == WAIT_FOR_RX)
						{
							stage = WAIT_FOR_RX2;
							
							_delay_100ms(20);
						}
						else
						{
							ret = LA66_NODOWN;
							break;
						}
					}
				}
			}
			
			_delay_ms(10);
		}
		
		// check if return code indicates received downlink
		if (ret == LA66_SUCCESS)
		{
			ret = LA66_query_command_P(PSTR("AT+RECVB=?\r\n"), buffer);
			
			char *tmp = buffer;
			
			*fPort = atoi(strsep_P(&tmp, PSTR(":")));
			char *_payload = strsep_P(&tmp, PSTR(":"));
			*rxSize = strlen(_payload) / 2;
			
			memset(payload, 0, LA66_MAX_BUFF);
			
			strcpy(payload, _payload);
			
			readHex(payload, _payload);
		}
	}

	return ret;
}

LA66_ReturnCode LA66_synctime()
{
	log_serial_P(PSTR("In sync\r\n"));
	
	LA66_ReturnCode ret = LA66_ERR_PANIC;
	char buffer[32 + LA66_MAX_BUFF];
	
	snprintf_P(buffer, sizeof(buffer), PSTR("AT+DEVICETIMEREQ=1\r\n"));
	
	// send command
	ret = send_command(buffer);
	
	// check if command was successful
	if (ret == LA66_SUCCESS)
	{
		// receive
		LA66_Stage stage = WAIT_FOR_OK;
		uint32_t timeout;
		
		timeout = LA66_RX_TIMEOUT;
		
		for (uint32_t i = 0; i < timeout * 100L; i++)
		{
			if (read_line(buffer) > 0)
			{
				if (stage == WAIT_FOR_OK)
				{
					if (strcmp_P(buffer, PSTR(AT_ERROR)) == 0)
					{
						ret = LA66_ERROR;
						break;
					}
					else if (strcmp_P(buffer, PSTR(AT_PARAM_ERROR)) == 0)
					{
						ret = LA66_ERR_PARAM;
						break;
					}
					else if (strcmp_P(buffer, PSTR(AT_BUSY_ERROR)) == 0)
					{
						ret = LA66_ERR_BUSY;
						break;
					}
					else if (strcmp_P(buffer, PSTR(AT_NO_NET_JOINED)) == 0)
					{
						ret = LA66_ERR_JOIN;
						break;
					}
					else
					{
						if (strcmp(buffer, AT_OK) == 0)
						{
							stage = WAIT_FOR_TX;
						}
					}
				}
				else if (stage == WAIT_FOR_TX)
				{
					if (strcmp_P(buffer, PSTR("txDone")) == 0)
					{
						stage = WAIT_FOR_SYNCTIMEOK;
						
						_delay_100ms(10);
					}
				}
				else if (stage == WAIT_FOR_SYNCTIMEOK)
				{
					if (strcmp_P(buffer, PSTR("Sync time ok")) == 0)
					{
						ret = LA66_SUCCESS;
						
						_delay_ms(100);
						break;
					}
				}
			}
			
			_delay_ms(10);
		}
	}

	return ret;
}