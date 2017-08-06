// SALT_ext_sensors.cpp
//
//
//

//---------------------------< I N C L U D E S >--------------------------------------------------------------

#include <SALT_ext_sensors.h>


//---------------------------< P I N G E X >------------------------------------------------------------------
//
//
//

uint8_t SALT_ext_sensors::pingex (uint8_t addr, i2c_t3 wire)
	{
	wire.beginTransmission (addr);				// set the device slave address
	return wire.endTransmission();						// send slave address; returns SUCCESS if the address was acked
	}


//---------------------------< S E N S O R _ D I S C O V E R >------------------------------------------------
//
// Scan through the mux[].port[].sensor[] struct and interrogate the external i2c net for sensor node eeproms.
// eeproms on sensor nodes have i2c slave addresses in the range 0x50-0x57.  The low nibble of the eeprom
// slave address usually matches the low nibble of the sensor device on that node.  This code does not contemplate
// the possibility that a sensor may have an address range that is limited with respect to the eeprom - the sensor
// device may only support 1, 2, 4 addresses compared to the eeprom's 8 addresses.  This is presumably handled
// in hardware by limiting the address jumper availability and hardwiring the appropriate eeprom address pins
// to ground.
//
// RULES:
//	1. all muxes, ports, sensors must begin at zero
//	2. no empties: if there are two muxes in a system they shall be mux[0] and mux[1]; not mux[0] and mux[m>1]
//	3. when adding sensors to a mux, begin at port[0]; no empties
//	4. when adding sensors to a port, begin with sensor[0]; no empties
//
// TODO: what about the mux-mounted sensors on port[7]?  Because these are 'different', handle them elsewhere?
// TODO: How to map physical sensor location to the electrical sensor location?  There is no map; we define the
// physical and electrical locations.  See table on sheet 2 of the mux schematics.
//
// eeprom mapping?
// eeprom is 4kx8 and has 128 32-byte pages.  page 0 identifies the assembly
//	page 0:
//	  addr: 0: assembly name; 8 char not null terminated but zero-filled
//			1


uint8_t SALT_ext_sensors::sensor_discover (void)
	{
	uint8_t	m;				// indexer into mux
	uint8_t	p;				// indexer into port
	uint8_t	s;				// indexer into sensor

	uint8_t	mux_addr;
	uint8_t	eep_addr;
	uint8_t	sensor_addr;
	uint8_t	sensor_type;											// temp value storage for type val read from eeprom
	boolean	break_flag=false;	// used when breaking out of switch should cause break out of sensor for loop

	uint32_t	start = millis();

	Serial.printf ("discovering external sensors...\n");

	for (m = 0; m < MAX_MUXES; m++)
		{
		sensor_type = TMP275;								// spoof until eeprom code written

		mux_addr = PCA9548A_BASE_MIN | (m & 7);						// make 9548A slave address from lowest base addr and mux array index
		if (SUCCESS != pingex (mux_addr, Wire1))
			{
			Serial.printf ("\tmux[%d] not detected\n", m);
			break;
			}

		mux[m].imux.setup (mux_addr, Wire1, (char*)"Wire1");		// initialize this instance
		mux[m].imux.begin (I2C_PINS_29_30, I2C_RATE_100);
		mux[m].imux.init ();

		mux[m].exists = true;										// so we can use mux-mounted sensors even when nothing attached to mux[m] ports
		Serial.printf ("\tmux[%d] detected\n", m);
		for (p = 0; p < MAX_PORTS; p++)								// here only when we were able to initialize a mux
			{
			if (SUCCESS != mux[m].imux.control_write (mux[m].imux.port[p]))		// enable access to mux[m].port[p]
				Serial.printf ("mux[%d].imux.control_write (mux[%d].imux.port[%d]) fail (0x%.02X)", m, m, p, mux[m].imux.port[p]);

			for (s = 0; s < MAX_SENSORS; s++)
				{
				eep_addr = EEP_BASE_MIN | (s & 7);					// make eeprom slave address from lowest base addr and sensor array index
				if (SUCCESS != pingex (eep_addr, Wire1))
					{
					Serial.printf ("\tmux[%d].port[%d].sensor[%d] eeprom not detected\n", m, p, s);
//					continue;												// continue to next sensor or break to next port?
					}
				else
					{
					mux[m].port[p].sensor[s].ieep.setup (eep_addr, Wire1, (char*)"Wire1");
					mux[m].port[p].sensor[s].ieep.begin (I2C_PINS_29_30, I2C_RATE_100);
					mux[m].port[p].sensor[s].ieep.init ();
					Serial.printf ("\tmux[%d].port[%d].sensor[%d] eeprom detected\n", m, p, s);
					// here we read eeprom to discover sensor type; switch on that value and attempt to instantiate
					}

				switch (sensor_type)
					{
					case TMP275:
						sensor_addr = TMP275_BASE_MIN + (s & 7);			// make sensor slave address from lowest base addr and sensor array index

						if (SUCCESS != pingex (sensor_addr, Wire1))
							{
							Serial.printf ("\tmux[%d].port[%d].sensor[%d] tmp275 not detected\n", m, p, s);
							break_flag=true;			// flag to break out of switch and then break out of sensor for loop
							break;
							}

						mux[m].port[p].sensor[s].itmp275.setup (sensor_addr, Wire1, (char*)"Wire1");	// initialize this sensor instance
						mux[m].port[p].sensor[s].itmp275.begin (I2C_PINS_29_30, I2C_RATE_100);
						mux[m].port[p].sensor[s].itmp275.init (TMP275_CFG_RES12);

						// set temp sensor pointer register to point at temperature register here or elsewhere?
						mux[m].has_sensors = true;							// flag to indicate that there is a mux[m] that has sensors
						mux[m].port[p].has_sensors = true;					// flag to indicate that port[p] has sensors
						mux[m].port[p].sensor[s].addr = sensor_addr;		// if not 0, then sensor[s] exists
						mux[m].port[p].sensor[s].type = TMP275;				// if not 0, then sensor[s] exists
						Serial.printf ("\tmux[%d].port[%d].sensor[%d] tmp275 detected\n", m, p, s);

						// fill sensor struct parameters (whatever they are) here
//						sensor_type = 0; //undo spoof
						break;

					case 0:		//
						break;
					default:
						Serial.printf ("sensor type not recognized\n");
					}
				if ((break_flag) || (0 == sensor_type))
					{
					break_flag=false;	// used when breaking out of switch should cause break out of sensor for loop
					break;
					}
				}
			if (false == mux[m].port[p].has_sensors)
				break;
			}
		mux[m].imux.control_write (PCA9548A_PORTS_DISABLE);			// disable access to mux[m] ports
		}

	Serial.printf ("discovering mux-mounted sensors...\n");

	for (m = 0; m < MAX_MUXES; m++)												// here we discover mux-mounted sensors
		{
		if (mux[m].exists)														// on muxes that exist
			{
			if (SUCCESS != mux[m].imux.control_write (mux[m].imux.port[7]))		// enable access to mux[m].port[7]
				{
				Serial.printf ("mux[%d].imux.control_write (mux[%d].imux.port[7]) fail (0x%.02X)", m, m, mux[m].imux.port[7]);
				break;															// serious problem if we can't switch the multiplexer  TODO: what to do?
				}

			if (SUCCESS != pingex (MUX_EEP_ADDR, Wire1))
				{
				Serial.printf ("\tmux[%d] eeprom not detected\n", m);
//				continue;
				}
			else
				{
				mux[m].ieep.setup (MUX_EEP_ADDR, Wire1, (char*)"Wire1");			// initialize this sensor instance
				mux[m].ieep.begin (I2C_PINS_29_30, I2C_RATE_100);
				mux[m].ieep.init ();
				Serial.printf ("\tmux[%d] eeprom detected\n", m);
				}

			// TODO: define format of whatever information *what information is that?) will be stored in eeprom.
			// Should the mux eeprom data format be somehow similar to the (undefined) sensor format?  How the
			// same? How different?
			// could include:
			//	number of ports populated
			//	which of the three supported sensors are mounted
			//	????
			//
			// FOR NOW because we haven't specified anything:
			//	address 0x00: mounted sensors bit field is the bitwise or of: TMP275, MS8607, HDC1080 (only one or the other of the last two - they share an address) 
			//
			// for now, this code will write the mounted sensors bit field:

//			mux[m].ieep.set_addr16 (0);
//			mux[m].ieep.control.wr_byte = TMP275 | HDC1080;
//			mux[m].ieep.byte_write();

			// TODO: read eeprom to discover what to do next

			// TODO: if tests on some value(s) stored in eeprom to determine which of the three sensors to use?
			// If we do that just what is it that gets stored in eeprom?
			// that same value will be used by the scanner to fetch sensor data

			mux[m].ieep.set_addr16 (0);				// TODO: establish the true and correct address for the mounted sensors bit field
			mux[m].ieep.byte_read();				// read the value; is it ok to just leave in the control struct?  do we need to save it off somewhere?

			mux[m].installed_sensors = 0;			// init to be safe

			Serial.printf ("eeprom read: 0x%.2X\n", mux[m].ieep.control.rd_byte);

			if (TMP275 & mux[m].ieep.control.rd_byte)									// should we expect a 275?
				{
				if (SUCCESS != pingex (TMP275_SLAVE_ADDR_7, Wire1))						// can we ping it?
					Serial.printf ("\tmux[%d] TMP275 specified but not detected\n", m);
				else
					{
					mux[m].itmp275.setup (TMP275_SLAVE_ADDR_7, Wire1, (char*)"Wire1");	// initialize this sensor instance
					mux[m].itmp275.begin (I2C_PINS_29_30, I2C_RATE_100);
					if (SUCCESS != mux[m].itmp275.init (TMP275_CFG_RES12))
						{
						mux[m].itmp275.~Systronix_TMP275();								// destructor this instance
						Serial.printf ("\tmux[%d] tmp275 init fail\n", m);
						}
					else
						{
						mux[m].installed_sensors = TMP275;								// note that we found and initialized TMP275
						Serial.printf ("\tmux[%d] TMP275 initialized\n", m);
						}
					}
				}

			if (HDC1080 & mux[m].ieep.control.rd_byte)									// should we expect a 1080?
				{
				if ((SUCCESS == pingex (0x40, Wire1)) && (SUCCESS == pingex (0x76, Wire1)))		// make sure we aren't accidentally talking to MS8607 TODO: use #defines for slave addresses
					Serial.printf ("\tmux[%d] MS8607 detected; expected HDC1080\n", m);
				else if (SUCCESS != pingex (0x40, Wire1))
					Serial.printf ("\tmux[%d] HDC1080 specified but not detected\n", m);
				else		// TODO: write enough library support to fill this in
					{
					mux[m].installed_sensors |= HDC1080;
					Serial.printf ("\tmux[%d] HDC1080 detected\n", m);
					}
				}

			if (MS8607 & mux[m].ieep.control.rd_byte)										// should we expect an MS8607?
				{
				if ((SUCCESS == pingex (0x40, Wire1)) && (SUCCESS == pingex (0x76, Wire1)))		//  TODO: use a #define for slave addresses
					Serial.printf ("\tmux[%d] MS8607 specified but not detected\n", m);
				else		// TODO: write enough library support to fill this in
					{
					mux[m].installed_sensors |= HDC1080;
					Serial.printf ("\tmux[%d] MS8607 detected\n", m);
					}
				}
			}
		else
			break;

		mux[m].imux.control_write (PCA9548A_PORTS_DISABLE);			// disable access to mux[m] ports
		}

	Serial.printf ("discovery done (%ldmS)\n", millis() - start);
	return SUCCESS;
	}


//---------------------------< S E N S O R _ S C A N >--------------------------------------------------------
//
// This function scans the sensors and calls each sensor's get_temperature_data() function to fill that sensor's
// data struct.  Scanning begins at mux[0].port[0].sensor[0] and continues until all external sensors have been
// queried.
// TODO: what about the mux-mounted sensors on port[7]?  Because these are 'different', handle them elsewhere?
// TODO: How to map physical sensor location to the electrical sensor location?
//

uint8_t SALT_ext_sensors::sensor_scan (void)
	{
	uint8_t	m;				// indexer into mux
	uint8_t	p;				// indexer into port
	uint8_t	s;				// indexer into sensor

	for (m = 0; m < MAX_MUXES; m++)
		{
//		Serial.printf ("mux[%d]", m);
		if (!mux[m].has_sensors)										// does at least one port have sensors?
			{
//			Serial.printf ("\n");
			break;														// this mux has no sensors, done
			}
		else															// there are sensors attached to this mux
			{
			for (p = 0; p < MAX_PORTS; p++)
				{
//				Serial.printf (".port[%d]", p);
				if (!mux[m].port[p].has_sensors)						// does port[p] have at least one sensor?
					{
//					Serial.printf ("\n");
					break;												// no sensors on this port, do next mux
					}
				else													// there are sensors on this port
					{
					mux[m].imux.control_write (mux[m].imux.port[p]);	// enable access to mux[m].port[p]
					for (s = 0; s < MAX_SENSORS; s++)
						{
//						Serial.printf (".sensor[%d]\n", s);
						if (0 == mux[m].port[p].sensor[s].addr)			// addr is non-zero when there is a sensor
							{
//							Serial.printf ("\n");
							break;										// no more sensors on this port, next port
							}
						else											// there are (more) sensors
							{
							if (TMP275 == mux[m].port[p].sensor[s].type)
								mux[m].port[p].sensor[s].itmp275.get_data();	// get the sensor's data
							}
						}
					}
				}
			}
		}

	for (m = 0; m < MAX_MUXES; m++)										// here we scan mux-mounted sensors
		{
		if (mux[m].exists)												// on muxes that exist
			{
			if (SUCCESS != mux[m].imux.control_write (mux[m].imux.port[7]))	// enable access to mux[m].port[7]
				{
				Serial.printf ("mux[%d].imux.control_write (mux[%d].imux.port[7]) fail (0x%.02X)", m, m, mux[m].imux.port[7]);
				break;													// serious problem if we can't switch the multiplexer  TODO: what to do?
				}
			if (mux[m].installed_sensors & TMP275)
				mux[m].itmp275.get_data();								// get the sensor's data

//			if (mux[m].installed_sensors & MS8607)						// only one of these
//				mux[m].ims8607.get_data();								// get the sensor's data
//			else if (mux[m].installed_sensors & HDC1080)
//				mux[m].ihdc1080.get_data();								// get the sensor's data
			}
		}

	return SUCCESS;
	}


//---------------------------< S H O W _ S E N S O R _ T E M P S >--------------------------------------------
//
// development hack to write each ext sensor temperature to habitat A UI; one temperature reading every other second
//

uint8_t SALT_ext_sensors::show_sensor_temps (void)
	{
	static uint8_t	m=0;				// indexer into mux
	static uint8_t	p=0;				// indexer into port
	static uint8_t	s=0;				// indexer into sensor
	static uint8_t	state = 0;

	if (!mux[0].exists)												// if no mux[0] we're done
		{
		sprintf (utils.display_text, "mux[0] missing  ");
		utils.ui_display_update (HABITAT_A);
		return FAIL;
		}

	switch (state)
		{
		case 0:		// mux[m] temp sensor
			if (mux[m].installed_sensors & TMP275)
				{
				sprintf (utils.display_text, "m[%d]            %6.4f\xDF", m, mux[m].itmp275.data.deg_f);
				utils.ui_display_update (HABITAT_A);
				state = 1;
				break;
				}
			else												// no tmp275
				{
				sprintf (utils.display_text, "m[%d] no TMP275", m);
				utils.ui_display_update (HABITAT_A);
				state = 1;
				break;
				}

		case 1:		// mux[m].port[p].sensor[s]
			if (mux[m].port[p].has_sensors)						// does port[p] have at least one sensor?
				{
				if (0 != mux[m].port[p].sensor[s].addr)			// this sensor present?
					{
					sprintf (utils.display_text, "m[%d].p[%d].s[%d]  %6.4f\xDF", m, p, s, mux[m].port[p].sensor[s].itmp275.data.deg_f);
					utils.ui_display_update (HABITAT_A);		// display sensor temp
					s++;										// next sensor
					if (0 == mux[m].port[p].sensor[s].addr)		// does it exist?
						{
						s = 0;									// no, reset
						p++;									// next port
						if (mux[m].port[p].has_sensors)			// does it have sensors?
							break;								// yes
						else
							{
							p = 0;								// no, reset
							m++;								// next mux
							if (!mux[m].exists)					// is there a next mux?
								m=0;							// no, reset
							state = 0;							// start over
							break;
							}
						}
					}
				else											// should never get here; port[p] has sensors but no sensor[0] address
					{
					sprintf (utils.display_text, "m[%d].p[%d].s[%d]  no sensor err", m, p, s);
					utils.ui_display_update (HABITAT_A);		// display error message
					m=0;										// reset and restart
					p=0;
					s=0;
					state=0;
					break;
					}
				}
			else											// port[p] has no sensors
				{
				s = 0;										// reset
				p = 0;
				m++;										// next mux
				if (!mux[m].exists)
					m = 0;									// no next mux, reset
				state = 0;
				break;
				}
		}

	return SUCCESS;
	}


//---------------------------< T M P 2 7 5 _ D A T A _ P T R _ G E T >----------------------------------------
//
// returns the address of a TMP275 sensor's data struct or NULL
//
// Supports tmp275 sensors only
//

Systronix_TMP275::data_t* SALT_ext_sensors::tmp275_data_ptr_get (uint8_t m, uint8_t p, uint8_t s)
	{
	if (mux[m].port[p].sensor[s].addr)								// if there is a sensor at this location
		return &mux[m].port[p].sensor[s].itmp275.data;				// return a pointer to the data struct
	return NULL;													// NULL pointer else
	}


//---------------------------< M U X _ T M P 2 7 5 _ D A T A _ P T R _ G E T >--------------------------------
//
// returns the address of the mux-mounted TMP275 sensor's data struct or NULL
//
// Supports tmp275 sensors only
//

Systronix_TMP275::data_t* SALT_ext_sensors::mux_tmp275_data_ptr_get (uint8_t m)
	{
	if (mux[m].installed_sensors & TMP275)							// if there is a sensor at this location
		return &mux[m].itmp275.data;								// return a pointer to the data struct
	return NULL;													// NULL pointer else
	}

