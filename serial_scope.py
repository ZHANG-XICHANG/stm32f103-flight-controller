"""Plot TEL telemetry from the remote USB port; run with --help."""
import argparse
import csv
import math
import time
from collections import deque
from contextlib import ExitStack


CSV_FIELDS = ['host_elapsed_s', 'flight_elapsed_s', 'sequence', 'time_ms', 'angle_deg', 'rate_target_deg_s', 'rate_deg_s', 'rate_error_deg_s', 'p_term', 'd_term', 'pid_output', 'esc1_pwm_us', 'esc2_pwm_us', 'esc3_pwm_us', 'esc4_pwm_us', 'axis']
PANELS = [('Angle (deg)', [(4, 'angle')]),
                      ('Rate (deg/s)', [(5, 'target'), (6, 'measured')]),
                      ('Rate error (deg/s)', [(7, 'rate_error')]),
                      ('Correction (us)', [(8, 'P term'), (9, 'D term'), (10, 'PID output')]),
                      ('ESC PWM (us)', [(11, 'M1'), (12, 'M2'), (13, 'M3'), (14, 'M4')])]

def parse_line(text):
    parts = text.strip().split(',')
    if not ((parts[0] == 'TEL' and len(parts) == 15) or
            (parts[0] == 'TEL_SEQ' and len(parts) == 2)):
        return None
    try:
        values = [int(v) for v in parts[1:]]
    except ValueError:
        return None
    if not 0 <= values[0] <= 0xFFFFFFFF:
        return None
    if len(values) == 1:
        return values[0]
    if not 0 <= values[1] <= 0xFFFFFFFF:
        return None
    if any(not -32768 <= v <= 32767 for v in values[2:9]):
        return None
    if any(not 0 <= v <= 65535 for v in values[9:13]):
        return None
    if values[13] not in (0, 1, 2):
        return None
    return (values[0], values[1], values[2] / 100.0,
            values[3] / 10.0, values[4] / 10.0, values[5] / 10.0,
            values[6], values[7], values[8], *values[9:])


class SampleClock:
    """Unwrap uint32 milliseconds; restart the plot on a backward clock jump."""
    def __init__(self):
        self.previous = None
        self.elapsed_ms = 0

    def update(self, milliseconds):
        reset = False
        if self.previous is not None:
            delta = (milliseconds - self.previous) & 0xFFFFFFFF
            if delta >= 0x80000000:
                self.elapsed_ms = 0
                reset = True
            else:
                self.elapsed_ms += delta
        self.previous = milliseconds
        return self.elapsed_ms / 1000.0, reset


class LineBuffer:
    def __init__(self):
        self.pending = bytearray()
        self.discarding = False

    def feed(self, chunk):
        for byte in chunk:
            if byte == 10:
                if not self.discarding:
                    try:
                        yield self.pending.decode('ascii')
                    except UnicodeDecodeError:
                        pass
                self.pending.clear()
                self.discarding = False
            elif not self.discarding:
                self.pending.append(byte)
                if len(self.pending) > 512:
                    self.pending.clear()
                    self.discarding = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Remote USB serial port, e.g. COM3')
    parser.add_argument('--list-ports', action='store_true')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--points', type=int, default=2000)
    parser.add_argument('--window', type=float, default=10.0, help='Fixed time window in seconds')
    parser.add_argument('--angle-range', type=float, nargs=2, default=(-30.0, 30.0), metavar=('MIN', 'MAX'))
    parser.add_argument('--gyro-range', type=float, nargs=2, default=(-200.0, 200.0), metavar=('MIN', 'MAX'))
    parser.add_argument('--error-range', type=float, nargs=2, default=(-200.0, 200.0), metavar=('MIN', 'MAX'))
    parser.add_argument('--output-range', type=float, nargs=2, default=(-500.0, 500.0), metavar=('MIN', 'MAX'))
    parser.add_argument('--csv', help='Save all samples to a NEW CSV file')
    parser.add_argument('--pwm-range', type=float, nargs=2, default=(1000.0, 2000.0), metavar=('MIN', 'MAX'))
    args = parser.parse_args()
    if args.baud <= 0 or args.points <= 0:
        parser.error('--baud and --points must be positive')
    if not math.isfinite(args.window) or args.window <= 0:
        parser.error('--window must be finite and positive')
    ranges = (args.angle_range, args.gyro_range, args.error_range, args.output_range, args.pwm_range)
    if any(not all(math.isfinite(v) for v in limits) or limits[0] >= limits[1] for limits in ranges):
        parser.error('Axis ranges must be finite with MIN < MAX')
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.exit(1, 'Install dependencies: python -m pip install pyserial matplotlib\n')
    ports = list(list_ports.comports())
    if args.list_ports:
        print('\n'.join(f'{p.device}: {p.description}' for p in ports) or 'No serial ports found.')
        return 0
    if not args.port:
        if len(ports) == 1:
            args.port = ports[0].device
        else:
            parser.error('Specify --port COMx; use --list-ports to see ports.')
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        parser.exit(1, 'Install plotting dependency: python -m pip install matplotlib\n')
    samples = deque(maxlen=args.points)
    framing = LineBuffer()
    sample_clock = SampleClock()
    fig = None
    try:
        with ExitStack() as stack:
            ser = stack.enter_context(serial.Serial(args.port, args.baud, timeout=0))
            writer = csv_file = None
            if args.csv:
                csv_file = stack.enter_context(open(args.csv, 'x', newline='', encoding='utf-8'))
                writer = csv.writer(csv_file)
                writer.writerow(CSV_FIELDS)
            print(f'Opened {args.port}. Close the plot or press Ctrl+C to stop.')
            print('Time axis = flight sample time. Sequence gaps are not a radio loss measurement.')
            fig, axes = plt.subplots(5, 1, sharex=True, figsize=(10, 12))
            lines = []
            # Row: host time, flight elapsed, sequence, time_ms, angle, target,
            # gyro, error, P term, D term, total output.
            panels = PANELS
            for ax, (label, curves) in zip(axes, panels):
                for column, name in curves:
                    line, = ax.plot([], [], linewidth=1, label=name)
                    lines.append((line, column))
                ax.set_ylabel(label)
                ax.grid(True)
                ax.legend(loc='upper right')
            for ax, limits in zip(axes, ranges):
                ax.set_ylim(*limits)
                ax.set_autoscale_on(False)
            axes[-1].set_xlim(0.0, args.window)
            axes[-1].set_xlabel('Flight sample elapsed time (s)')
            title = fig.suptitle('Waiting for TEL / TEL_SEQ...')
            fig.tight_layout(rect=(0, 0, 1, 0.94))
            plt.show(block=False)
            started = last_flush = time.monotonic()
            last_sample = previous_sequence = ack_sequence = None
            received = duplicates = 0
            current_axis = None
            while plt.fignum_exists(fig.number):
                dirty = False
                # Bound per-iteration work and retain incomplete USB lines.
                for text in framing.feed(ser.read(min(ser.in_waiting, 8192))):
                    result = parse_line(text)
                    if result is None:
                        continue
                    if isinstance(result, int):
                        ack_sequence = result
                        continue
                    now = time.monotonic()
                    elapsed, reset = sample_clock.update(result[1])
                    if reset or result[-1] != current_axis:
                        samples.clear()
                        previous_sequence = None
                    current_axis = result[-1]
                    row = (now - started, elapsed, *result)
                    if result[0] != previous_sequence:
                        samples.append(row)
                    if writer:
                        writer.writerow(row)
                    received += 1
                    duplicates += result[0] == previous_sequence
                    previous_sequence = result[0]
                    last_sample = now
                    dirty = True
                now = time.monotonic()
                if csv_file and now - last_flush >= 1.0:
                    csv_file.flush()
                    last_flush = now
                if dirty:
                    times = [r[1] for r in samples]
                    for line, column in lines:
                        line.set_data(times, [r[column] for r in samples])
                    right = max(args.window, times[-1])
                    axes[-1].set_xlim(right - args.window, right)
                if last_sample is not None:
                    title.set_text(f'{"XYZ"[current_axis]} PID | seq={previous_sequence} | received={received} | repeated seq={duplicates} | age={now-last_sample:.1f}s')
                elif ack_sequence is not None:
                    title.set_text(f'ACK sequence={ack_sequence}; waiting for valid PID telemetry')
                fig.canvas.draw_idle()
                plt.pause(0.03)
    except KeyboardInterrupt:
        print('\nStopped.')
    except (serial.SerialException, OSError) as exc:
        print(f'Serial/CSV error: {exc}')
        return 1
    finally:
        if fig is not None:
            plt.close(fig)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
