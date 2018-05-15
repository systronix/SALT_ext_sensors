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
// eeprom is 4kx8 and has 128 32-byte pages.  page 0 identifies the assembly?
//
// page 0:	information about the assembly
//		0x0000 - 0x000F:	assembly name; 16 characters null filled; known assemblies are:
//			"TMP275"
//			"MUX"
//		0x0010 - 0x0011:	assembly revision; two byte little endian; MM.mm in the form 0xMMmm
//		0x0010: mm
//		0x0011: MM
//		0x0012 - 0x0015:	assembly manufacture date; four byte time_t little endian
//		0x0016 - 0x0019:	assembly service date; four byte time_t little endian
//		0x001A - 0x001F:	undefined; 6 bytes
//
// page 1:	information about a sensor
//		0x0020 - 0x002F:	sensor name; 16 characters null filled; known sensors are:
//			"TMP275"		- temperature
//			"HDC1080"		- temperature and relative humidity
//			"MS8607PT"		- the pressure and temperature sensors (i2c address 0x76)
//			"MS8607H"		- the relative humidity sensor (i2c address 0x40)
//		0x0030:				sensor i2c address; one byte:
//								bits 6..0: the i2c address
//								bit 7:
//									when 0, the value in bits 6..0 is a base address modified by the assembly's address jumpers
//									when set, the value in bits 6..0 is an absolute address
//		0x0031 - 0x003F:	undefined; 15 bytes
//
// page 2:	information about a sensor (same format a page 1; repeat as often as necessary within reason)
//
//
//	[assembly]
//	# in case we perhaps do the right thing some day and place an eeprom on the same bus as the mux common
//	type = MUX7
//	revision = 2.0
//	manuf date = 2017-08-19
//	[sensor 1]
//	type1 = TMP275
//	# address is two hexadecimal digits
//	address1 = 48
//	absolute1 = no
//	[sensor 2]
//	type2 = HDC1080
//	# address is two hexadecimal digits
//	address2 = 40
//	absolute2 = yes
//


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

																	// perhaps this is a flaw in the design?  The local eeprom is 'hidden'
																	// on port 7.  Shouldn't it be on the same 'bus' as the mux?
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
				continue;														// no eeprom so no sensors here; try next mux
				}
			else
				{
				mux[m].ieep.setup (MUX_EEP_ADDR, Wire1, (char*)"Wire1");		// initialize eeprom instance
				mux[m].ieep.begin (I2C_PINS_29_30, I2C_RATE_100);
				mux[m].ieep.init ();
				Serial.printf ("\tmux[%d] eeprom detected\n", m);
				}

// PROBLEM: Because NAP have put muxes in the field without properly loaded eeproms and because there are also
// previous versions out there that write 0x05 to address 0 in the eeprom (that was done to get this code working),
// we somehow have to support those.  There have never been muxes with MS8607 so we only need to worry about
// systems that have 0xFF or 0x05 in eeprom address 0
			mux[m].ieep.set_addr16 (0);										// point to page 0, address 0
			mux[m].ieep.byte_read();										// read it ('M' is 0x4D
			if ((0x05 == mux[m].ieep.control.rd_byte) || (0xFF == mux[m].ieep.control.rd_byte))		// if address 0 is 'erased' or 0x05, write a value there
				{
				e7n.exception_add (E7N_UNINIT_MUX_IDX);
				Serial.printf ("uninitialized MUX eeprom\n");
				// TODO: change this to an exception
				}

			// here we read eeprom to discover what to do next

			mux[m].installed_sensors = 0;									// init to be safe

			mux[m].ieep.set_addr16 (ASSY_PAGE_ADDR);						// point to page 0, address 0; this is [assembly] page
			mux[m].ieep.control.rd_wr_len = PAGE_SIZE;						// set page size
			mux[m].ieep.control.rd_buf_ptr = assy_page.as_array;			// point to destination buffer
			mux[m].ieep.page_read ();										// read the page

			mux[m].ieep.set_addr16 (SENSOR1_PAGE_ADDR);						// point to page 1, address 0; this is [sensor 1] page
			mux[m].ieep.control.rd_wr_len = PAGE_SIZE;						// set page size
			mux[m].ieep.control.rd_buf_ptr = sensor1_page.as_array;			// point to destination buffer
			mux[m].ieep.page_read ();										// read the page

			if (strcmp (sensor1_page.as_struct.sensor_type, "TMP275"))
				mux[m].installed_sensors = TMP275;
			else if (strcmp (sensor1_page.as_struct.sensor_type, "HDC1080"))
				mux[m].installed_sensors = HDC1080;
			else if (strcmp (sensor1_page.as_struct.sensor_type, "MS8607PT"))
				mux[m].installed_sensors = MS8607;
			else if (strcmp (sensor1_page.as_struct.sensor_type, "MS8607H"))
				mux[m].installed_sensors = MS8607;
			else if (0xFF != *sensor1_page.as_struct.sensor_type)
				Serial.printf ("unknown [sensor 1] type: %s\n", sensor1_page.as_struct.sensor_type);
			else
				Serial.printf ("[sensor 1] type not specified\n");

			if (mux[m].installed_sensors)									// no sensor 2 without sensor 1
				{
				mux[m].ieep.set_addr16 (SENSOR2_PAGE_ADDR);						// point to page 2, address 0; this is [sensor 2] page
				mux[m].ieep.control.rd_wr_len = PAGE_SIZE;						// set page size
				mux[m].ieep.control.rd_buf_ptr = sensor2_page.as_array;			// point to destination buffer
				mux[m].ieep.page_read ();										// read the page

				if (strcmp (sensor2_page.as_struct.sensor_type, "TMP275"))
					mux[m].installed_sensors |= TMP275;
				else if (strcmp (sensor2_page.as_struct.sensor_type, "HDC1080"))
					mux[m].installed_sensors |= HDC1080;
				else if (strcmp (sensor2_page.as_struct.sensor_type, "MS8607PT"))
					mux[m].installed_sensors = MS8607;
				else if (strcmp (sensor2_page.as_struct.sensor_type, "MS8607H"))
					mux[m].installed_sensors = MS8607;
				else if (0xFF != *sensor2_page.as_struct.sensor_type)
					Serial.printf ("unknown [sensor 2] type: %s\n", sensor1_page.as_struct.sensor_type);
				else
					Serial.printf ("[sensor 2] type not specified\n");
				}
// TODO: support for third 'sensor' because MS8607 is really two sensors

			// TODO: if tests on some value(s) stored in eeprom to determine which of the three sensors to use?
			// If we do that just what is it that gets stored in eeprom?
			// that same value will be used by the scanner to fetch sensor data


			Serial.printf ("installed_sensors: 0x%.2X\n", mux[m].installed_sensors);

			if (TMP275 & mux[m].installed_sensors)										// should we expect a 275?
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
						mux[m].installed_sensors &= ~TMP275;							// remove TMP275 from installed sensors
						}
					else
						{
						Serial.printf ("\tmux[%d] TMP275 initialized\n", m);
						}
					}
				}

			if (HDC1080 & mux[m].installed_sensors)									// should we expect a 1080?
				{
				if ((SUCCESS == pingex (0x40, Wire1)) && (SUCCESS == pingex (0x76, Wire1)))		// make sure we aren't accidentally talking to MS8607 TODO: use #defines for slave addresses
					Serial.printf ("\tmux[%d] MS8607 detected; expected HDC1080\n", m);
				else if (SUCCESS != pingex (0x40, Wire1))
					Serial.printf ("\tmux[%d] HDC1080 specified but not detected\n", m);
				else
					{
					mux[m].ihdc1080.setup (Wire1, (char*)"Wire1");							// initialize this sensor instance
					mux[m].ihdc1080.begin (I2C_PINS_29_30, I2C_RATE_100);
					if (SUCCESS != mux[m].ihdc1080.init (MODE_T_AND_H))						// temperature and humidity mode
//					if (SUCCESS != mux[m].ihdc1080.init (0, TRIGGER_H))						// individual mode; humidity only
						{
						mux[m].ihdc1080.~Systronix_HDC1080();								// destructor this instance
						Serial.printf ("\tmux[%d] HDC1080 init fail\n", m);
						mux[m].installed_sensors &= ~HDC1080;								// remove HDC1080 from installed sensors
						}
					else
						{
						Serial.printf ("\tmux[%d] HDC1080 initialized\n", m);
						}
					}
				}

			if (MS8607 & mux[m].installed_sensors)										// should we expect an MS8607?
				{
				if ((SUCCESS != pingex (0x40, Wire1)) || (SUCCESS != pingex (0x76, Wire1)))		//  TODO: use a #define for slave addresses
					Serial.printf ("\tmux[%d] MS8607 specified but not detected\n", m);
				else		// TODO: write enough library support to fill this in
					{
//					 MS8607 initialization code here
//					 mux[m].ims8607.setup (Wire1, (char*)"Wire1");							// initialize this sensor instance
//					 mux[m].ims8607.begin (I2C_PINS_29_30, I2C_RATE_100);
//					 if (SUCCESS != mux[m].ims8607.init ())
//						{
//						mux[m].ims8607.~Systronix_MS8607();									// destructor this instance
						Serial.printf ("\tmux[%d] MS8607 init fail\n", m);
						mux[m].installed_sensors &= ~MS8607;								// remove MS8607 from installed sensors
//						}
//					else
//						{
//						mux[m].installed_sensors |= MS8607;									// note that we found and initialized MS8607
//						Serial.printf ("\tmux[%d] MS8607 initialized\n", m);
//						}
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

	char log_msg[64];

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
								if (SUCCESS != mux[m].port[p].sensor[s].itmp275.get_data())	// attempt to get the sensor's data
									{
									if (!e7n.e7n_msg[E7N_EXT_TEMP_FAULT_IDX].queued)	// if not yet queued
										{												// once any single sensor is queued other sensor faults not logged
										e7n.exception_add (E7N_EXT_TEMP_FAULT_IDX);		// unable to read this sensor
										sprintf (log_msg, "%s @ mux[%d].port[%d].sensor[%d]", (char*)e7n.e7n_msg [E7N_EXT_TEMP_FAULT_IDX].l, m, p, s);
										logs.log_event (log_msg);						// log it
										}
									}
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
				if (!e7n.e7n_msg[E7N_MUX_FAULT_IDX].queued)				// if not yet queued
					{
					e7n.exception_add (E7N_MUX_FAULT_IDX);				// unable to set the multiplexer
					sprintf (log_msg, "%s @ mux[%d]", (char*)e7n.e7n_msg [E7N_MUX_FAULT_IDX].l, m);
					logs.log_event (log_msg);							// log it
					}
				break;													// serious problem if we can't switch the multiplexer  TODO: what to do?
				}
			if (mux[m].installed_sensors & TMP275)
				if (SUCCESS != mux[m].itmp275.get_data())				// attempt to get the sensor's data
					if (!e7n.e7n_msg[E7N_MUX_TSNSR_FAULT_IDX].queued)	// if not yet queued
						{
						e7n.exception_add (E7N_MUX_TSNSR_FAULT_IDX);	// unable to read this sensor
						sprintf (log_msg, "%s @ mux[%d]", (char*)e7n.e7n_msg [E7N_MUX_TSNSR_FAULT_IDX].l, m);
						logs.log_event (log_msg);						// log it
						}

			if (mux[m].installed_sensors & HDC1080)						// only one of these
				if (SUCCESS != mux[m].ihdc1080.get_data())				// attempt to get the sensor's data
					{
					if (!e7n.e7n_msg[E7N_MUX_HSNSR_FAULT_IDX].queued)	// if not yet queued
						{
						e7n.exception_add (E7N_MUX_HSNSR_FAULT_IDX);	// unable to read this sensor
						sprintf (log_msg, "%s @ mux[%d]", (char*)e7n.e7n_msg [E7N_MUX_HSNSR_FAULT_IDX].l, m);
						logs.log_event (log_msg);						// log it
						}
					}
// MS8607 NOT SUPPORTED; EXCEPTION HANDLING NOT SUPPORTED
//			else if (mux[m].installed_sensors & MS8607)
//				if (SUCCESS != mux[m].ims8607.get_data())				// attempt to get the sensor's data
//					{
//					if (!e7n.e7n_msg[E7N_MUX_THSNSR_FAULT_IDX].queued)	// if not yet queued
//						{
//						e7n.exception_add (E7N_MUX_THSNSR_FAULT_IDX);	// unable to read this sensor
//						sprintf (log_msg, "%s @ mux[%d]", (char*)e7n.e7n_msg [E7N_MUX_THSNSR_FAULT_IDX].l, m);
//						logs.log_event (log_msg);						// log it
//						}
//					}
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

	if (!mux[0].exists)											// if no mux[0] we're done
		{
		sprintf (utils.display_text, "mux[0] missing  ");
		utils.ui_display_update (HABITAT_A);
		return FAIL;
		}

	switch (state)
		{
		case 0:													// mux[m] sensors
			if (!(mux[m].installed_sensors & (TMP275|HDC1080)))	// no supported sensors installed
				{
				sprintf (utils.display_text, "m[%d] none", m);
				utils.ui_display_update (HABITAT_A);
				state = 2;
				break;
				}
			else if (mux[m].installed_sensors & TMP275)			// TMP275; if not 275 fall through to state 1
				{												// here when there is a 275 so display
				sprintf (utils.display_text, "m[%d].s[0]       % 3.1f\xDF", m, mux[m].itmp275.data.deg_f);
				utils.ui_display_update (HABITAT_A);
				if (mux[m].installed_sensors & HDC1080)			// when there is also an HDC1080
					state = 1;									// next time state 1 to display temp & rh
				else
					state = 2;									// no HDC1080 so next time port sensors if any
				break;
				}

		case 1:													// HDC1080 temperature and humidity
				sprintf (utils.display_text, "m[%d].s[1]       % 3.1f\xDF  % 3.1f%%rh", m, mux[m].ihdc1080.data.deg_f, mux[m].ihdc1080.data.rh);
				utils.ui_display_update (HABITAT_A);
				state = 2;										// next time start on port sensors if any
				break;

		case 2:		// mux[m].port[p].sensor[s]
			if (mux[m].port[p].has_sensors)						// does port[p] have at least one sensor?
				{
				if (0 != mux[m].port[p].sensor[s].addr)			// this sensor present?
					{
					sprintf (utils.display_text, "m[%d].p[%d].s[%d]  % 3.1f\xDF", m, p, s, mux[m].port[p].sensor[s].itmp275.data.deg_f);
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
			else												// port[p] has no sensors
				{
				s = 0;											// reset
				p = 0;
				m++;											// next mux
				if (!mux[m].exists)
					m = 0;										// no next mux, reset
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

Systronix_TMP275::data_t* SALT_ext_sensors::mux_tmp275_data_ptr_get (uint8_t m)
	{
	if (mux[m].installed_sensors & TMP275)							// if there is a sensor at this location
		return &mux[m].itmp275.data;								// return a pointer to the data struct
	return NULL;													// NULL pointer else
	}


//---------------------------< M U X _ H D C 1 0 8 0 _ D A T A _ P T R _ G E T >------------------------------
//
// returns the address of the mux-mounted HDC1080 sensor's data struct or NULL
//

Systronix_HDC1080::data_t* SALT_ext_sensors::mux_hdc1080_data_ptr_get (uint8_t m)
	{
	if (mux[m].installed_sensors & HDC1080)							// if there is a sensor at this location
		return &mux[m].ihdc1080.data;								// return a pointer to the data struct
	return NULL;													// NULL pointer else
	}

