//////////////////////////////////////////////////////////////////////
// gp.h -- GPIO header
// Date: Sun Sep  2 22:01:29 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum IO {   // GPIO Input or Output:
	Input=0,    // GPIO is to become an input pin
	Output=1,   // GPIO is to become an output pin
	Alt0=4,
	Alt1=5,
	Alt2=6,
	Alt3=7,
	Alt4=3,
	Alt5=2
} IO;

typedef enum Pull { // GPIO Input Pullup resistor:
	None,       // No pull up or down resistor
	Up,         // Activate pullup resistor
	Down        // Activate pulldown resistor
} Pull;

int gpio_configure_io(int gpio,IO io);
int gpio_alt_function(int gpio,IO *io);
int gpio_set_alt_function(int gpio,IO io);
int gpio_get_drive_strength(int gpio,bool *slew_limited,bool *hysteresis,int *drive);
int gpio_set_drive_strength(int gpio,bool slew_limited,bool hysteresis,int drive);
int gpio_configure_pullup(int gpio,Pull pull);
int gpio_read(int gpio);
int gpio_write(int gpio,int bit);
uint32_t gpio_read32();

bool gpio_open(void);
bool gpio_close(void);

#ifdef __cplusplus
}
#endif

// end gp.h
