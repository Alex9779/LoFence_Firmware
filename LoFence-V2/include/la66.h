/*!
@file	la66.h
@author	Alexander Leisentritt (alexander.leisentritt@alitecs.de)
@date	August 2023
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
#define AT_OK "OK"
#define AT_ERROR "AT_ERROR"
#define AT_PARAM_ERROR "AT_PARAM_ERROR"
#define AT_BUSY_ERROR "AT_BUSY_ERROR"
#define AT_NO_NET_JOINED "AT_NO_NET_JOINED"

extern void log_serial(char msg[]);

//=========
// GLOBALS
//=========
//! Values returned by LA66_* functions
enum LA66_ReturnCode {
	LA66_SUCCESS,                 /**< Success */
	LA66_ERROR,                   /**< Error */
	LA66_ERR_PARAM,               /**< Error: invalid parameter passed to function or command */
	LA66_ERR_BUSY,                /**< Error: tried to join/tx but all configured frequency channels were busy, wait and try again */
	LA66_ERR_JOIN,                /**< Error: tried to tx data without being joined to a LoRaWAN network */
	LA66_NODOWN,                  /**< tx succeeded and no downlink was received */
	LA66_ERR_PANIC,	              /**< Error: SOMETHING(???) went wrong. You found a bug! */
	LA66_EOB = LA66_MAX_BUFF	  /**< Reached end of buffer passed to function */
};

enum LA66_ReceiveStage {
	WAIT_FOR_OK,
	WAIT_FOR_TX,
	WAIT_FOR_RX,
	WAIT_FOR_RX2
};

//===========
// FUNCTIONS
//===========
//system
//! Resets the LA66 by toggling the RESET pin
/*!
Toogles the reset pin (from HIGH -> LOW -> HIGH).

The LA66 module transmits it's firmware version upon being reset, so if the version is successful.

@return LA66_SUCCESS if version was succesfully retrieved after toggling the RESET pin
@return LA66_ERR_PANIC if version was not retrieved after toggling the RESET pin
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
uint8_t LA66_query_command(const char *command, char *response);

//LoRa
//! Initialises all the LA66 MAC settings required to run LoRa commands (join, tx, etc).
/*!
Resets the software LoRaWAN stack and initialises all of the required parameters (set in
config.h) to communicate over a LoRaWAN network.

@return LA66_SUCCESS The function reset & initialised all the required values without failure
@return LA66_ERR_PARAM Likely means memory issue was caused while reading a response from the
LA66
@return LA66_ERR_PANIC If this happens something went really wrong when writing a command
*/
uint8_t LA66_waitForJoin();

uint8_t LA66_getDr();
uint16_t LA66_getRx1Dl();
uint16_t LA66_getRx2Dl();

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
uint8_t LA66_transmitB(uint8_t *fPort, const bool confirm, char *payload, uint8_t *rxSize);

#endif /* LA66_H_ */
