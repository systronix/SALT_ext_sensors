// ext_sensors_test

#include <SALT_ext_sensors.h>

SALT_ext_sensors ext_sensors;


//---------------------------< S E T U P >--------------------------------------------------------------------
//
//
//

void setup (void)
	{
	Serial.begin(115200);     // use max baud rate
	while((!Serial) && (millis()<10000));    // wait until serial monitor is open or timeout, which seems to fall through

	Serial.printf("SALT_ext_sensors Test Code\n");

	Serial.printf("Build %s - %s\r\n%s\r\n", __DATE__, __TIME__, __FILE__);

	ext_sensors.sensor_discover ();

	}
	

//---------------------------< L O O P >----------------------------------------------------------------------
//
//
//

void loop (void)
	{
//	Serial.printf (".");
	ext_sensors.sensor_scan ();
	delay (5000);
	}