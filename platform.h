#ifndef PLATFORM_H
#define PLATFORM_H

// Platform
int cc2520_plat_gpio_init(void);
void cc2520_plat_gpio_free(void);
int cc2520_plat_spi_init(void);
void cc2520_plat_spi_free(void);

#endif