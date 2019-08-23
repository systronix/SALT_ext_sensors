# SALT_ext_sensors
Library for sensors on the external sensor net which uses J1 on SALT/retrofit  or J2 on PowerDist/new

## MUX bug fix 2019 Aug
This bug only manifested on a SBS system, which is the only configuration to use two MUX boards (one per habitat). This configuration is not common and was never fully tested in early 2018. Apparently exceptions seen at the factory were ignored by employees building the systems and no one called it to our attention.

To read HDC1080, it must first be triggered.  First trigger occurs as part of init().  Thereafter, every read causes a new trigger.  Data are available typically 6.5mS after a trigger.  The device nacks reads that occur before data are ready.

When there are more than one mux board connected to SALT, the PCA9548As are paralleled since they share an I2C net.  When set, the PCA9548A is essentially 'transparent' to the i2c bus.  When two devices connected to same mux channel of two separate muxes share an address (the on-board TMP275 and HDC1080) both of the devices will respond.

We saw temperature falling when we heated one mux independent of the other on a benchtop habitat simulator.  This happened because both TMP275s were being read by the same operation.  Zero always wins on the I2C bus so zeros emitted by one overcame the ones emitted by the other.

In SALT_ext_sensors.cpp, sensor_scan() works through all of the attached channel sensors for each mux then goes mux-to-mux reading the mux-mounted sensors.  What was missing was a single line of code to disable mux[0] before moving on to mux[1].  Because of that, and because there are no channel sensors, both muxes in an SBS system are set to port[7] and left there.  When that happens, both sensors are read when SALT reads mux[0] which triggers HDC1080 on both mux[0] and mux[1].  SALT then tries to read the HDC1080 on mux[1] but because it was triggered by the SALT read of mux[0] both mux[0] and mux[1] nack the read which causes EXT MUX H SENSOR.
