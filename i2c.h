#ifndef I2C_H
#define I2C_H
#include <inttypes.h>


/** initializes the i2c interface */
void _i2c_init(void);

void _i2c_task(void);


uint8_t __twi_ready(void);
void __twi_wait_ready(void);
void _twi_send_data(uint8_t slaveAddr, uint8_t * data, uint8_t length);
void _twi_read_data(uint8_t slaveAddr, uint8_t * data, uint8_t length);
void _twi_send_data_no_stop(uint8_t slaveAddr, uint8_t * data, uint8_t length);

#endif // I2C_H
