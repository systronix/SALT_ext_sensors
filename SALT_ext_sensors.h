#ifndef SALT_EXT_SENSORS_H_
#define SALT_EXT_SENSORS_H_

// SALT_ext_sensors
//
//


#include <Arduino.h>

#include <Systronix_PCA9548A.h>
#include <Systronix_TMP275.h>
#include <Systronix_HDC1080.h>
#include <Systronix_M24C32.h>
#include <SALT_exceptions.h>
#include <SALT_utilities.h>


//---------------------------< D E F I N E S >----------------------------------------------------------------
//
// From bab:
// One Mux per habitat and EC. So a Max of Three Mux per SALT. (kinda catchy)
//
// One drawer per mux channel, one sensor max per compartment, so max of three sensors per channel. I imagine we
// will go with just one sensor on the middle drawer.
//
// So that's a max of four mux channels active per mux. Plus the "other channel" with mux ambient sensor. That
// makes a max of 5 channels. So why stuff more connectors than 4? Or even place them.
//
// EC: one Mux, two drawers, each has one compartment. So two sensors. The EC is jammed up against the habitat
// so I don't know that a Mux per EC makes sense. I can't see a good place to sense ambient on the EC. Maybe
// just use a channel on the habitat Mux for the two EC sensors. On the EC there is an empty shelf under the
// compartments... that might make a good ambient sensor location. But that still makes only three sensors per EC.
//
// The above superseded.
//
// Endcap is controlled and monitored as part of habitat A.  That means that there are at most two muxes in any
// of the defined configurations


#define	MAX_MUXES		2		// habitat A (with EC if attached) and habitat B
#define	MAX_PORTS		6		// per habitat: B2B (3), SBS & SS (4), B2BWEC (5), SSWEC (6); port[7] mux mounted sensors not handled here
#define	MAX_SENSORS		3		// per port one sensor per compartment

#define	MUX_EEP_ADDR	0x57	// mux-mounted sensor eeprom has fixed address

#define	TMP275			1		// bit fields used in installed_sensors
#define	MS8607			(1<<1)	// these two mutually exclusive because they share an i2c slave address
#define	HDC1080			(1<<2)


//---------------------------< E E P R O M   M E M O R Y   M A P S >------------------------------------------
//
// M24C32 has 128 32-byte pages.  
//
// Each page begins with a 'type'.  Types shall be printable ascii uppercase letters A-Z and digits 0-9. There
// shall be no whitespace and no punctuation except the underscore ('_') character.  Type shall be null terminated
// so that max type length shall be no more than 15 characters.  When type is less than 15 characters, all eeprom
// memory locations from the null terminator to the end of the allotted space shall be filled with null bytes ('\0')
//
// Byte 0x0000 of each page shall hold a printable upper case letter or digit.  There shall be no blank pages
// between occupied pagesWhen reading the eeprom, SALT shall stop reading when byte 0x0000 of the current page
// does not hold an allowed character.
//
//
//----------< C O M M O N >----------
//
// These defines are common to all sensor and mux assemblies
//

//#define ASSEMBLY_TYPE			0x0000	// 16 bytes ascii text (A-Z,0-9, and '_') '\0' filled

//#define	ASSEMBLY_REV			0x0010	// uint16 size board revision; upper byte is major (M), lower is minor (m) so: M.m
//#define	ASSEMBLY_REV_MINOR		0x0010	// minor part (mm) of MM.mm board revision where mm can be 0 to 99 (uint16_t 0xMMmm)
//#define	ASSEMBLY_REV_MAJOR		0x0011	// major part (MM) of MM.mm board revision where MM can be 0 to 99 (uint16_t 0xMMmm)

//#define	ASSEMBLY_MANUF_DATE		0x0012	// uint32_t date of manufacture; a time_t value
//#define	ASSEMBLY_MANUF_DATE_LO	0x0012	// low byte
//#define	ASSEMBLY_MANUF_DATE_ML	0x0013	//
//#define	ASSEMBLY_MANUF_DATE_MH	0x0014	//
//#define	ASSEMBLY_MANUF_DATE_HI	0x0015	// date of manufacture high byte

//#define	ASSEMBLY_SERV_DATE		0x0016	// uint32_t date of last service; a time_t value
//#define	ASSEMBLY_SERV_DATE_LO	0x0016	// low byte
//#define	ASSEMBLY_SERV_DATE_ML	0x0017	//
//#define	ASSEMBLY_SERV_DATE_MH	0x0018	//
//#define	ASSEMBLY_SERV_DATE_HI	0x0019	// date of last service high byte

// 0x001A-0x001F (six bytes) not defined

// mux-mounted eeprom page size

#define		PAGE_SIZE			32

// base mux-mounted eeprom page addresses
#define		ASSY_PAGE_ADDR		0
#define		SENSOR1_PAGE_ADDR	ASSY_PAGE_ADDR+PAGE_SIZE
#define		SENSOR2_PAGE_ADDR	SENSOR1_PAGE_ADDR+PAGE_SIZE


//----------< S E N S O R >----------

#define SENSOR1_BASE			0x0020		// all sensors and TMP275 on mux
#define SENSOR2_BASE			0x0040		// HDC1080 or MS8607 on mux
//#define SENSOR3_BASE			0x0060		// not yet used
//#define SENSOR4_BASE			0x0080		// not yet used

// these are offsets from the sensor base addresses above
//
// <object>.ieep.set_addr16 (SENSOR_BASE + SENSOR_ADDR);
//

#define SENSOR_TYPE				0x0000	// 16 bytes ascii text (A-Z,0-9, and '_') '\0' filled
#define SENSOR_ADDR				0x0010	// i2c addr; msb=1 if addr is absolute; zero if sensor i2c base address that tracks assembly jumper settings (usually 3 lsb)

// 0x0011-0x001F (15 bytes) not defined


//---------------------------< C L A S S >--------------------------------------------------------------------

class SALT_ext_sensors
	{
	private:
	protected:

//----------< M U X - M O U N T E D   E E P R O M   S E T T I N G S >----------
//
// these unions apply to the eeprom mounted on the mux board at port 7.  For now, there is only one of these
// because I don't know how to apply the information that the eeprom contains
//

		union
			{
			struct mux_settings
				{
				char		assembly_type[16];		// string 'MUX7'; perhaps in future MUX if we add an eep to mux on 8548A common net
				uint16_t	assembly_revision;		// 2.0 as a uint16_t = 0x0200
				time_t		manufacture_date;		// not used in this code
				time_t		service_date;			// not used in this code; not part of ini file; how set? when set?
				uint8_t		ports;					// number of ports installed
				uint8_t		unused[5];				// so that the struct totals 32 bytes
				} as_struct;
			uint8_t			as_array[32];
			} assy_page;

		union
			{
			struct sensor_settings
				{
				char		sensor_type[16];		// string one of: 'TMP275', 'HDC1080', 'MS8607PT', 'MS8607H'
				uint8_t		sensor_addr;			// sensor i2c address; if msb set, clear it do not OR with mux index [m]
				uint8_t		unused[15];				// so that the struct totals 32 bytes
				} as_struct;
			uint8_t			as_array[32];
			} sensor1_page, sensor2_page;


	public:
		struct mux_t										// array of multiplexer boards
			{
			boolean							exists;			// set true during discovery
			boolean							has_sensors;	// set true during discovery when sensors are discovered
			uint8_t							installed_sensors;	// bitfield filled by decoding sensor type from eeprom
			Systronix_PCA9548A				imux;			// instance the mux board; we call the destructor for unneeded instances
			Systronix_M24C32				ieep;			// instance the eeprom (this is a place-holder for now)
			Systronix_TMP275				itmp275;		// instance the tmp275 temp sensor
			Systronix_HDC1080				ihdc1080;		// instance the hdc1080 temp and rh sensor
			struct port_t									// array of multiplexer ports
				{
				boolean						has_sensors;	// set true during discovery when sensors are discovered
				struct sensor_t								// array of port sensors; ports are limited in this design to 3 sensors per port
					{
					Systronix_TMP275		itmp275;		// instance the temp sensor board 275; What to do when we have different kinds of sensors?
					Systronix_M24C32		ieep;			// instance the temp sensor board eeprom (this is a place-holder for now)
					uint8_t					type;
					uint8_t					addr;			// read from eep; this value is device min addr + [s] in sensor[s] (the index s)
															// usually not required when low order eep address matches low order sensor address
															// required when the low order addresses do not match (a sensor has only 1, 2, or 4 addresses) TODO: is this correct?
					} sensor[MAX_SENSORS];
				} port[MAX_PORTS];
			} mux[MAX_MUXES];

//	public:
		uint8_t		sensor_discover (void);
		uint8_t		sensor_scan (void);
		
		uint8_t		pingex (uint8_t addr, i2c_t3 wire = Wire);	// pings an i2c address; Wire is default
		uint8_t		show_sensor_temps (void);
		
		Systronix_TMP275::data_t*	tmp275_data_ptr_get (uint8_t m, uint8_t p, uint8_t s);
		Systronix_TMP275::data_t*	mux_tmp275_data_ptr_get (uint8_t m);
		Systronix_HDC1080::data_t*	mux_hdc1080_data_ptr_get (uint8_t m);
	};

// We shall constrain the i2c slave address of each mux to be the 9548A base address + the mux[m] array index m
// (0-7) so mux[0] always refers to i2c slave address 0x70, regardless of where it lies in the daisy chain of
// muxes.  Similarly, we shall constrain the port specifier to be a value returned from a #define or some such
// so that mux[m].port[0] refers to port zero on mux[m].  We are limited in the number of sensors that can hang
// off of an individual port.  In this design, there are three compartments per drawer so three is the max number
// of sensors.
//
// Further, there shall be no empty locations between occupied locations.  All mux[m], port[p], sensor[s] indexes
// begin at [0] and shall be increment by 1 to the next occupied index.  An unoccupied index terminates any
// sequential operation (discovery, scanning) even when that index + 1 is occupied.
//
// Because the eeprom slave address is fixed at 0x50–0x57, and because we don't necessarily know what is out
// there, we don't need to store the eeprom address in this struct but, the fixed portion of the sensor's
// address (usually upper four bits) must be stored in the eeprom so that we can fetch it, modify it (if possible)
// with the three low order eeprom address bits, and store it in our struct at mux[m].port[p].sensor[s].addr for
// later use.  This is perhaps the only nod to future sensor development.
//
// A discovery process takes place during or following system startup where we discover how many muxes are
// attached; then for each of those muxes, we query port-by-port, searching eeprom addresses 0x50–0x57, and
// filling in mux[m].port[p].sensor[s] for those ports that have sensors attached.
//
// The discovery process initializes an instance of the eeprom library for each of the addresses.  Successful
// return from the library init() function indicates the presence of an eeprom.  At this writing, we only support
// the TMP275 external temperature sensor.  This code does not anticipate if or how different sensor type might
// be supported.  When the init() function is not successful, the destructor for that instance is called.
//


extern	SALT_ext_sensors ext_sensors;
#endif	// SALT_EXT_SENSORS_H_
