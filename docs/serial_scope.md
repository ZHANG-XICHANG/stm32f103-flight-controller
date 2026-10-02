# USB telemetry scope

Run from P01_flight_hal with Python 3. Connect the **remote controller's USB**.

```powershell
python -m pip install pyserial matplotlib
python serial_scope.py --list-ports
python serial_scope.py --port COM3 --csv telemetry.csv
```

Replace COM3 with the listed remote port. If exactly one port exists, --port is
optional. Close other applications using that port. CSV files are created
exclusively; choose a new filename for each recording. Close the plot window or
press Ctrl+C to stop and close the serial port/file.

The scope accepts TEL,sequence,gyro_x_x10,error_x_x10,pid_output_x,remote_throttle. The first two
measurement columns are divided by 10 and plotted in deg/s; output remains in
native PID units. Four separate plots retain the latest 2000 received samples
(--points changes this). CSV records all accepted TEL samples, including repeated
sequences. TEL_SEQ only updates startup status; it is not a PID measurement.
Other debug lines, malformed lines and out-of-range values are ignored.
Remote throttle uses a fixed 0..1000 axis and a CSV column. Legacy TEL lines
without throttle remain supported; the missing value is blank in CSV and absent
from the throttle curve. Throttle is sampled locally on the remote at publish
time, not at the flight PID sample time.

Automatic scaling is disabled. Default Y limits are -200..200 deg/s for gyro
and error, and -500..500 for PID output. X scrolls with a fixed 10-second span.
Values outside the display limits remain in CSV. Customize with:

```powershell
python serial_scope.py --port COM3 --gyro-range -100 100 --error-range -100 100 --output-range -300 300 --window 10 --points 2000
```

The point limit still bounds retained history; increase it to cover the desired
time window at higher data rates.

The time axis and CSV timestamps are PC receive elapsed seconds, not flight
sample timestamps. USB buffering can group samples. Sequence repeats and gaps
are not a direct measurement of RF packet loss. The age indicator measures time
since the last received TEL line, not sensor sample freshness.

```powershell
python -m unittest discover -s tests -p test_serial_scope.py
```
