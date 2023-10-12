#ifndef MAIN_H_
#define MAIN_H_

// time to sleep between measurements
#define INTERVAL_SECONDS 5 * 60

// +- time to sleep between measurements in ms
#define RANDOMNESS 10

// time in ms a measurement should take (per polarity)
#define MEASURE_MS 6000

// maximum voltage of the fence which can
// be measured with the ADC, depends on actual
// resistor values, 12kV is theoretical value
#define MAXIMUM_FENCE_VOLTAGE 12000

// battery low threshold voltage in mV
#define BATTERY_LOW_THRESHOLD 3400

// maximum amount of cycle the battery can be low before deactivation
#define BATTERY_LOW_MAX_CYCLES 5

// absolute minimum battery voltage in mV
// which triggers deactivation right away (next cycle)
#define BATTERY_ABSOLUTE_MINIMUM 3100

void power_save(uint32_t sec);
void log_serial(char *msg);

void adc_init();

void measure();
void transmit();
void pause();

int main(void);

#endif /* MAIN_H_ */