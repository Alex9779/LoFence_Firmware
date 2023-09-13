/*
* variable_delay.c
*
* Created: 13.09.2023 15:28:41
*  Author: alexander
*/

#include <atmel_start.h>
#include <util/delay.h>

void _delay_10ms(int ms)
{
	while (0 < ms)
	{
		_delay_ms(10);
		ms -= 10;
	}
}

void _delay_50ms(int ms)
{
	while (0 < ms)
	{
		_delay_ms(50);
		ms -= 50;
	}
}

void _delay_100ms(int ms)
{
	while (0 < ms)
	{
		_delay_ms(100);
		ms -= 100;
	}
}

void _delay_500ms(int ms)
{
	while (0 < ms)
	{
		_delay_ms(500);
		ms -= 500;
	}
}

void _delay_1000ms(int ms)
{
	while (0 < ms)
	{
		_delay_ms(1000);
		ms -= 1000;
	}
}