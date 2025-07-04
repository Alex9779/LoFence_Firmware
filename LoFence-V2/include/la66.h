/*!
@file	la66.h
@author	Alexander Leisentritt (alexander.leisentritt@alitecs.de)
@date	August 2023 - September 2023
@brief	A library for embedded platforms that allows for interaction with a Dragino LA66.
*/

#ifndef LA66_H_
#define LA66_H_

//========
// MACROS
//========
//includes
// custom

// standard
#include <atmel_start.h>
#include <stdio.h>      //fgetc, fprintf
#include <string.h>     //strlen, strcmp
#include <util/delay.h>

//defines
#define LA66_MAX_BUFF 236
#define LA66_JOIN_TIMEOUT 10 * 60 // 10 minutes in seconds
#define LA66_COMMAND_TIMEOUT 10 // 10 seconds
#define LA66_RX_TIMEOUT 10 // 10 seconds
#define LA66_RX_CONF_TIMEOUT 60 // 60 seconds
#define AT_OK "OK"
#define AT_ERROR "AT_ERROR"
#define AT_PARAM_ERROR "AT_PARAM_ERROR"
#define AT_BUSY_ERROR "AT_BUSY_ERROR"
#define AT_NO_NET_JOINED "AT_NO_NET_JOINED"

typedef char LA66_buffer[LA66_MAX_BUFF];

extern void log_serial(const char *msg);
extern void log_serial_P(const char *msg);

//=========
// GLOBALS
//=========
//! Values returned by LA66_* functions
typedef enum LA66_ReturnCode {
	LA66_SUCCESS,                 /**< Success */
	LA66_NODOWN,                  /**< tx succeeded and no downlink was received */
	LA66_ERROR,                   /**< Error */
	LA66_ERR_PARAM,               /**< Error: invalid parameter passed to function or command */
	LA66_ERR_BUSY,                /**< Error: tried to join/tx but all configured frequency channels were busy, wait and try again */
	LA66_ERR_JOIN,                /**< Error: tried to tx data without being joined to a LoRaWAN network */
	LA66_ERR_PANIC,	              /**< Error: SOMETHING(???) went wrong. You found a bug! */
	LA66_EOB = LA66_MAX_BUFF	  /**< Reached end of buffer passed to function */
} LA66_ReturnCode;

typedef enum LA66_Stage {
	WAIT_FOR_OK,
	WAIT_FOR_TX,
	WAIT_FOR_RX,
	WAIT_FOR_RX2,
	WAIT_FOR_SYNCTIMEOK
} LA66_Stage;

//===========
// FUNCTIONS
//===========
//! Resets the LA66 by toggling the RESET pin
/*!
Toogles the reset pin (from HIGH -> LOW -> HIGH).
*/
void LA66_reset();

void LA66_activate();
void LA66_deactivate();


//! Write a command to the LA66 and recieve it's response
/*!
Send a command to the LA66, if the command is valid the LA66's response will be written
to response

@return LA66_ERR_PARAM if the command does not end in "\r\n" (required, see documentation)
@return LA66_SUCCESS command was successful and response was valid

@see LA66 LoRa Technology Module Command Reference User's Guide
*/
LA66_ReturnCode LA66_query_command_P(const char *command, char *response);

//! Waits for the LA66 to join a LoRaWAN network
/*!
@return LA66_SUCCESS The device joined a LoRaWAN network and is ready to transmit data
@return LA66_ERR_JOIN The device was not able to join a LoRaWAN network
*/
LA66_ReturnCode LA66_waitForJoin(void (*led_toggle_func)(void));

uint8_t LA66_getDr();
uint16_t LA66_getRx1Dl();
uint16_t LA66_getRx2Dl();
uint32_t LA66_getTimestamp();

//! Sends a confirmed/unconfirmed frame with an application payload of buff.
/*!
Transmits data over a LoRa network in either confirmed or unconfirmed mode.

@return LA66_NODOWN Transmission was successful but the server sent no downlink data
@return LA66_ERR_PANIC Tx was a success, but the server sent an invalid downlink packet
@return LA66_SUCCESS Transmission was successful and downlink data was read into downlink
@return LA66_ERR_PARAM Invalid LoRaWAN_Port or invalid buff data
@return LA66_ERR_BUSY All channels are currently busy, try sending data less frequently
@return LA66_ERR_JOIN You need to join a LoRaWAN network to TX data over one
*/
LA66_ReturnCode LA66_transmitB(uint8_t *fPort, const bool confirm, char *payload, uint8_t *rxSize);

LA66_ReturnCode LA66_synctime();

#endif /* LA66_H_ */
