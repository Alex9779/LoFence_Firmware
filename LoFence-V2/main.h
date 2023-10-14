#ifndef MAIN_H_
#define MAIN_H_

#define DEBUG // enable debug output (transmission fails if deactivated at the moment, no idea why)

#define INTERVAL_SECONDS 5 * 60 // time to sleep between measurements
#define RANDOMNESS 10 // +- time to sleep between measurements
#define MEASURE_MS 6000 // time in ms a measurement should take (per polarity)

void la66_init();
void la66_init_error();
void la66_tx(char buf[]);
void la66_tx_error();
void la66_rx(bool rec);
void la66_rx_clear();

void power_save(uint32_t sec);
void info(char buf[]);

void adc_init();

void measure();
void transmit();

void pause();

int main(void);

#endif /* MAIN_H_ */