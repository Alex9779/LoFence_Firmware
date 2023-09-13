/*!
@file	LA66.c
@author	Alexander Leisentritt (alexander.leisentritt@alitecs.de)
@date	August 2023
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

//! read a single byte from via UART from the LA66
// while there is data, return null char if not
static char read(const bool wait)
{
	if (USART_0_is_rx_ready() || wait)
	{
		return USART_0_read();
	}
	else
	{
		return '\0';
	}
}

//! write a string of bytes via UART to the LA66
static void write(const char *buff)
{
	for (uint8_t i = 0; i < strlen(buff); i++)
	{
		while (!USART_0_is_tx_ready()) {}
		USART_0_write(buff[i]);
	}
	while (USART_0_is_tx_busy()) {}
}

static void clear_read()
{
	while(USART_0_is_rx_ready()) {
		while(USART_0_is_rx_ready()) {
			USART_0_read();
		}
		_delay_ms(100);
	}
}

// Reads from the RX buffer into line until '\n' or '\r' or EOB
// '\n' or '\r' are not always returned in the same order
// line does not contain '\n' or '\r'
// returns length of line
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
		snprintf(debug, "DBG line: %s\r\n", line);
		log_serial(debug);
	}
	#endif

	return i;
}

// Sends a command to the LA66.
// No response is read.
static uint8_t send_command(const char *command)
{
	uint8_t end = strlen(command);

	// check command ends with \r\n (easy to forget)
	if (command[end - 2] != '\r' || command[end - 1] != '\n')
	{
		return LA66_ERR_PARAM;
	}
	
	#ifdef DEBUG
	snprintf(debug, "DBG Sending command: %s", command);
	log_serial(debug);
	#endif
	
	// clear the UART buffer just in case
	clear_read();
	
	// send command
	write(command);
	
	_delay_ms(10);
	
	return LA66_SUCCESS;
}

// Sends a command to the LA66 and sets the response in buffer
uint8_t LA66_query_command(const char *command, char *response)
{
	uint8_t ret = LA66_ERR_PANIC;
	char response2[LA66_MAX_BUFF];
	
	ret = send_command(command);
	
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
		
		if (strcmp(response, AT_ERROR) == 0)
		{
			return LA66_ERROR;
		}
		else if (strcmp(response, AT_PARAM_ERROR) == 0)
		{
			return LA66_ERR_PARAM;
		}
		else if (strcmp(response, AT_BUSY_ERROR) == 0)
		{
			return LA66_ERR_BUSY;
		}
		else if (strcmp(response, AT_NO_NET_JOINED) == 0)
		{
			return LA66_ERR_JOIN;
		}
		
		bool stop = false;

		// receive second command response
		for (uint32_t i = 0; i < LA66_COMMAND_TIMEOUT * 100L; i++)
		{
			while (read_line(response2) > 0)
			{
				if (strcmp(response2, AT_OK) == 0)
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

// PUBLIC
// system
// Resets the LA66 by toggling the RESET pin
void LA66_reset()
{
	LA66_deactivate();
	
	LA66_activate();
}

void LA66_activate()
{
	LA_RESET_set_level(true);
	
	_delay_ms(1000);
}

void LA66_deactivate()
{
	LA_RESET_set_level(false);
	
	_delay_ms(100);
}

// LoRa
//  Initialises all the LA66 MAC settings required to run LoRa commands (join, tx, etc).
uint8_t LA66_waitForJoin()
{
	uint8_t ret = LA66_ERR_PANIC;
	char response[LA66_MAX_BUFF];
	bool joined = false;
	
	for (uint32_t i = 0; i < LA66_JOIN_TIMEOUT * 100L; i++)
	{
		while (read_line(response) > 0)
		{
			if (strcmp(response, "JOINED") == 0)
			{
				log_serial("Joined network!\r\n");
				joined = true;
				
				break;
			}
		}
		
		if (joined)
		{
			ret = LA66_SUCCESS;
			
			break;
		}

		_delay_ms(10);
	}
	
	if (!joined)
	{
		ret = LA66_ERR_JOIN;
		log_serial("Unable to join network, timeout reached!\r\n");
	}
	
	return ret;
}

uint8_t LA66_getDr()
{
	char response[LA66_MAX_BUFF];
	
	if (LA66_query_command("AT+DR=?\r\n", response) == LA66_SUCCESS)
	{
		return response[0] - '0';
	}
	
	return 0;
}

uint16_t LA66_getRx1Dl()
{
	char response[LA66_MAX_BUFF];
	
	if (LA66_query_command("AT+RX1DL=?\r\n", response) == LA66_SUCCESS)
	{
		return atoi(response);
	}
	
	return 0;
}

uint16_t LA66_getRx2Dl()
{
	char response[LA66_MAX_BUFF];
	
	if (LA66_query_command("AT+RX2DL=?\r\n", response) == LA66_SUCCESS)
	{
		return atoi(response);
	}
	
	return 0;
}

// Sends a confirmed/unconfirmed frame with an application payload of buff.
uint8_t LA66_transmitB(uint8_t *fPort, const bool confirm, char *payload, uint8_t *rxSize)
{
	uint8_t ret = LA66_ERR_PANIC;
	
	//// figure out max payload length based on data rate, currently only for EU868
	//uint8_t dr = LA66_getDr();
	//uint8_t max_len = 0;
	//
	//if (dr == 0 || dr == 1 || dr == 2)
	//{
	//max_len = 64;
	//}
	//else if (dr == 3)
	//{
	//max_len = 128;
	//}
	//else if (dr == 4 || dr == 5 || dr == 6)
	//{
	//max_len = 235;
	//}
	//else
	//{
	//max_len = 235;
	//}
	
	// send command
	// Command format: AT+SENDB=<confirm>,<fPort>,<data_len>,<data>, example AT+SENDB=0,2,8,05820802581ea0a5
	char buffer[32 + LA66_MAX_BUFF];
	
	snprintf(buffer, "AT+SENDB=0%d,%u,%u,%s\r\n", confirm, *fPort, strlen(payload) / 2, payload);
	
	send_command(buffer);
	
	// receive
	uint8_t stage = WAIT_FOR_OK;
	
	for (uint32_t i = 0; i < LA66_RX_TIMEOUT * 100L; i++)
	{
		if (read_line(buffer) > 0)
		{
			if (stage == WAIT_FOR_OK)
			{
				if (strcmp(buffer, AT_ERROR) == 0)
				{
					ret = LA66_ERROR;
					break;
				}
				else if (strcmp(buffer, AT_PARAM_ERROR) == 0)
				{
					ret = LA66_ERR_PARAM;
					break;
				}
				else if (strcmp(buffer, AT_BUSY_ERROR) == 0)
				{
					ret = LA66_ERR_BUSY;
					break;
				}
				else if (strcmp(buffer, AT_NO_NET_JOINED) == 0)
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
				if (strcmp(buffer, "txDone") == 0)
				{
					stage = WAIT_FOR_RX;
					
					_delay_100ms(10);
				}
			}
			else if (stage == WAIT_FOR_RX || stage == WAIT_FOR_RX2)
			{
				if (strcmp(buffer, "rxDone") == 0)
				{
					ret = LA66_SUCCESS;
					
					_delay_ms(100);
					break;
				}
				else if (strcmp(buffer, "rxTimeout") == 0)
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
	
	if (ret == LA66_SUCCESS)
	{
		ret = LA66_query_command("AT+RECVB=?\r\n", buffer);
		
		char *tmp = buffer;
		
		*fPort = atoi(strsep(&tmp, ":"));
		char *_payload = strsep(&tmp, ":");		
		*rxSize = strlen(_payload) / 2;
						
		memset(payload, 0, LA66_MAX_BUFF);
		
		strcpy(payload, _payload);
		
		readHex(payload, _payload);
	}

	return ret;
}
