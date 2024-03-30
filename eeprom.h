#ifndef LCD1602_H
#define LCD1602_H

#define EEPROM_COMPATIBLE_STRING "atmel,at28c64b"


struct eeprom_gpiod_device {
    struct gpio_desc* ce;
    struct gpio_desc* oe;
    struct gpio_desc* we;
    struct gpio_descs* address;
    struct gpio_descs* io;
};

#endif // LCD1602_H
