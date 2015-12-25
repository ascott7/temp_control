/*  \file temp_control.c
 *
 *  \author Andrew Scott
 *  \brief Simple bang-bang feedback loop implementation to keep the temperature
 *         of a 10 ohm resistor right around a specified value
 *
 *  \note The executable created by compiling this file accepts a value between
 *        30 and 70 (degrees Celsius)
 */

#include <math.h>
#include <signal.h>       // for catching ctrl-c
#include <stdio.h>        // for printing to the console
#include "pi_helpers.h"   // for talking to the Pi

#define CONTROLPIN 17

/**
 * \brief Catches the SIGINT signal (sent when the user hits ctrl-c) to make
 *        sure we turn off the heater (if we don't do this and the heater is
 *        on when the user hits ctrl-c, the heater will stay on and heat up
 *        hotter than we intend).
 *
 * \note To actually make this function be called when ctrl-c is pressed, it is
 *       necessary to create a sigaction struct (in this case called act), set
 *       the sa_handler member of struct to be this function, and then call
 *               sigaction(SIGINT, &act, NULL);
 */
void int_handler(int sig)
{
    digital_write(CONTROLPIN, 0);
    exit(0);
}

/**
 * \brief Gets the current temperature of the resistor by getting the voltage
 *        of the LM35 temperature sensor (after being passed through a LM324
 *        with a DC gain of 3.2) as read by the ADC and multiplying the voltage
 *        by 32.25 to convert to temperature in Celsius.
 *
 * \returns The current temperature of the resistor
 * 
 * \remarks The values of 5 and 1024 when converting from the ADC response to
 *          voltage are from the equation from the ADC's datasheet (Vdd is 5V)
 *                       response = \frac{1024 * Vin}{Vdd}
 *          The value of 31.25 comes from the fact that each 10mV increase in
 *          the voltage from the LM35 corresponds with 1 degree Celsius
 *          increase. Combined with the fact that the LM324 has a DC gain of
 *          3.2, that means that the voltage we read will be
 *                voltage = 3.2 * 0.01 * temperature
 *          and thus we have
 *                voltage * 32.25 = temperature
 *
 * \note Datasheet for the LM35 can be found here 
 *       http://www.ti.com/lit/ds/symlink/lm35.pdf
 * \note Datasheet for the MCP3002 (ADC) can be found here
 *       http://www.ee.ic.ac.uk/pcheung/teaching/ee2_digital/MCP3002.pdf
 */
double get_current_temp()
{
    // send formatting data to the ADC and store the responses
    char one = spi_send_receive(0x68);
    char two = spi_send_receive(0x00);
    // shift and or the responses together in the proper order
    int response = 0x00000000;
    response = (response | (one & 0x03)) << 8;
    response |= two;
    // convert response to voltage and then voltage to temperature
    double voltage = (response * 5) / 1024.0;
    return 31.25 * voltage;
}

/**
 * \brief Reads the current temperature via SPI from the ADC and adjusts the
 *        control pin to keep the temperature at the specified target.
 *
 * \param target_temp    The desired temperature to maintain
 * \param last_temp      The temperature measured on the last sample
 * \param overshoot      The maximum temperature beyond the target temperature
 *                       that has been reached
 */
void check_temp(size_t* target_temp, size_t* last_temp, size_t* overshoot)
{
    size_t current_temp;
    current_temp = (size_t)get_current_temp();
    
    // do this check to prevent too many temperature outputs to the console
    if (current_temp != *(last_temp)) {
        printf("current temp: %lu\n", current_temp);
        *(last_temp) = current_temp;
        if (current_temp >= *(target_temp)) {
            printf("overshoot: %lu\n", *(overshoot) - *(target_temp));
        }
    }
    // turn on the heater if we are below the target temperature
    if (current_temp < *(target_temp)) {
        digital_write(CONTROLPIN, 1);
    }
    // turn off the heater if we are above or at the target temperature
    else {
        digital_write(CONTROLPIN, 0);
    }
    // keep track of the maximum temperature we achieve
    *(overshoot) = fmax(current_temp, *(overshoot));
}

int main(int argc, char* argv[])
{
    size_t target_temp, last_temp, overshoot;
    if(argc != 2) {
        printf("Incorrect call to temp_control. The correct format is\n");
        printf("\t./temp_control temperature\n");
        return 1;
    }

    target_temp = strtol(argv[1], NULL, 10);

    if (target_temp > 70 || target_temp < 30) {
        printf("Invalid temperature parameter. Please choose a temperature"
               "between 30 and 70\n");
        return 2;
    }
    
    pio_init();
    spi_init(244000, 0);
    pin_mode(CONTROLPIN, OUTPUT);

    last_temp = 0;
    overshoot = 0;

    // catch SIGINT (signal sent when pressing ctrl-c)
    //signal(SIGINT, int_handler);
    struct sigaction act;
    act.sa_handler = int_handler;
    sigaction(SIGINT, &act, NULL);

    // continuously check on the temperature
    while(1) {
        check_temp(&target_temp, &last_temp, &overshoot);
    }
    return 0;
}
