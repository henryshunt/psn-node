/**
 * Deals with communicating with and configuring the device over serial.
 */

/**
 * Waits for a short time and if any data is received on the serial port, permanently
 * switches into serial mode to respond to serial commands.
 */
void trySerialMode();