#ifndef MAIN_H_
#define MAIN_H_

#define VERSION 11

// time to sleep between measurements
#define INTERVAL_SECONDS 5 * 60

// amount of uplink to send confirmed per day
#define DAILY_CONFIRMED_UPLINKS 1

// +- time to sleep between measurements in s
#define RANDOMNESS 5

// time in ms a measurement should take (per polarity)
#define MEASURE_MS 6000

// maximum voltage of the fence which can
// be measured with the ADC, depends on actual
// resistor values, 12kV is theoretical value
#define MAXIMUM_FENCE_VOLTAGE 11850

// battery low threshold voltage in mV
#define BATTERY_LOW_THRESHOLD 3200

// maximum amount of full measurement cycles
// the battery can be low before deactivation
#define BATTERY_LOW_MAX_CYCLES 5

// absolute minimum battery voltage in mV
// which triggers deactivation right away (next cycle)
#define BATTERY_ABSOLUTE_MINIMUM 3100

void log_serial(const char *msg);
void log_serial_P(const char *msg);

int main(void);

#endif /* MAIN_H_ */
