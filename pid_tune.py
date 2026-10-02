"""Integrated Tkinter PID tuning and telemetry; one persistent USB connection."""
import csv
import math
import secrets
import time
from collections import deque
from decimal import Decimal, InvalidOperation
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
from serial.tools import list_ports
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from serial_scope import LineBuffer, SampleClock, parse_line, CSV_FIELDS, PANELS
IDS = {'roll': 0, 'pitch': 1, 'gyro_x': 2, 'gyro_y': 3, 'gyro_z': 4, 'yaw': 5}
def gain_units(text, maximum):
    try:
        value = Decimal(text)

        if not value.is_finite() or not 0 <= value <= maximum:
            raise ValueError(f'gain must be between 0 and {maximum}')

        scaled = value * 10000

        if scaled != scaled.to_integral_value():
            raise ValueError('at most four decimal places')

        return int(scaled)

    except InvalidOperation as exc:
        raise ValueError('invalid gain') from exc



class PIDGui:
    def __init__(self, root):
        self.root = root
        root.title('Flight PID & Live Telemetry')
        root.geometry('1250x900')
        root.minsize(1000, 650)
        self.port = self.pending = self.csv_file = self.writer = None
        self.samples = deque(maxlen=4000)
        self.window = 10.0
        self.reset_stream()
        self.create_widgets()
        self.refresh_ports()
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.after_id = root.after(20, self.poll)

    def reset_stream(self):
        self.framing, self.clock = LineBuffer(), SampleClock()
        self.samples.clear()
        self.started = self.last_flush = time.monotonic()
        self.last_draw = 0
        self.last_sample = self.previous_sequence = self.ack_sequence = None
        self.received = self.duplicates = 0
        self.current_axis = None
        self.pending_kind = "PID"
        self.dirty = True

    def create_widgets(self):
        c = ttk.Frame(self.root, padding=10)
        c.pack(side='left', fill='y')
        plot = ttk.Frame(self.root)
        plot.pack(side='right', fill='both', expand=True)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(c, textvariable=self.port_var, state='readonly')
        self.port_combo.pack(fill='x')
        ttk.Button(c, text='Refresh ports', command=self.refresh_ports).pack(fill='x')
        self.connect_button = ttk.Button(c, text='Connect', command=self.toggle_connection)
        self.connect_button.pack(fill='x', pady=5)
        self.link_var = tk.StringVar(value='Disconnected')
        ttk.Label(c, textvariable=self.link_var, wraplength=270).pack(anchor='w')
        ttk.Label(c, text='PID to modify (RAM only)').pack(anchor='w', pady=(15, 0))
        self.pid_var = tk.StringVar(value='roll')
        ttk.Combobox(c, textvariable=self.pid_var, values=list(IDS), state='readonly').pack(fill='x')
        self.pid_note = tk.StringVar()
        ttk.Label(c, textvariable=self.pid_note, wraplength=270).pack(anchor='w')
        self.pid_var.trace_add('write', self.update_pid_note)
        self.update_pid_note()
        self.kp_var, self.ki_var, self.kd_var = [tk.StringVar(value=v) for v in ('1.0000','0.0000','0.0000')]
        for label, var in zip(('Kp (0..20)', 'Ki (0..10)', 'Kd (0..2)'), (self.kp_var,self.ki_var,self.kd_var)):
            ttk.Label(c, text=label).pack(anchor='w', pady=(5,0))
            ttk.Entry(c, textvariable=var).pack(fill='x')
        self.apply_button = ttk.Button(c, text='Apply PID', command=self.apply_pid, state='disabled')
        self.apply_button.pack(fill='x', pady=8)
        self.status_var = tk.StringVar(value='Entry values are not read from the flight controller.')
        ttk.Label(c, textvariable=self.status_var, wraplength=270).pack(anchor='w')
        ttk.Label(c, text='Telemetry axis (independent of PID)').pack(anchor='w', pady=(8,0))
        self.axis_var = tk.StringVar(value='X')
        ttk.Combobox(c, textvariable=self.axis_var, values=('X','Y','Z'), state='readonly').pack(fill='x')
        self.axis_button = ttk.Button(c, text='Switch telemetry axis', command=self.apply_axis, state='disabled')
        self.axis_button.pack(fill='x', pady=3)
        self.axis_status = tk.StringVar(value='Actual telemetry axis: waiting')
        ttk.Label(c, textvariable=self.axis_status, wraplength=270).pack(anchor='w')
        self.csv_button = ttk.Button(c, text='Start CSV recording', command=self.toggle_csv, state='disabled')
        self.csv_button.pack(fill='x')
        self.csv_var = tk.StringVar(value='Not recording')
        ttk.Label(c, textvariable=self.csv_var, wraplength=270).pack(anchor='w')
        self.follow_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(c, text='Follow latest samples', variable=self.follow_var, command=self.mark_dirty).pack(anchor='w', pady=8)
        self.window_var = tk.StringVar(value='10')
        ttk.Label(c, text='Time window (seconds)').pack(anchor='w')
        ttk.Entry(c, textvariable=self.window_var).pack(fill='x')
        ttk.Label(c, text='Y ranges: min / max').pack(anchor='w', pady=(8,0))
        self.range_vars = []
        limits = [(-30,30),(-200,200),(-200,200),(-500,500),(1000,2000)]
        for (label,_), pair in zip(PANELS, limits):
            row = ttk.Frame(c)
            row.pack(fill='x')
            ttk.Label(row, text=label, width=18).pack(side='left')
            variables = [tk.StringVar(value=str(v)) for v in pair]
            for var in variables:
                ttk.Entry(row, textvariable=var, width=7).pack(side='left')
            self.range_vars.append(variables)
        ttk.Button(c, text='Apply plot ranges', command=self.apply_ranges).pack(fill='x', pady=5)
        self.figure = Figure(figsize=(9,9), dpi=100)
        self.axes = self.figure.subplots(5,1,sharex=True)
        self.lines = []
        for ax, (label, curves), pair in zip(self.axes,PANELS,limits):
            for column, name in curves:
                line, = ax.plot([],[],linewidth=1,label=name)
                self.lines.append((line,column))
            ax.set_ylabel(label)
            ax.set_ylim(*pair)
            ax.grid(True)
            ax.legend(loc='upper right',fontsize=8)
        self.axes[-1].set_xlim(0,self.window)
        self.axes[-1].set_xlabel('Flight sample elapsed time (s)')
        self.title = self.figure.suptitle('PID - waiting for telemetry')
        self.figure.tight_layout(rect=(0,0,1,0.96))
        self.canvas = FigureCanvasTkAgg(self.figure,master=plot)
        toolbar = NavigationToolbar2Tk(self.canvas,plot,pack_toolbar=False)
        toolbar.pack(side='bottom',fill='x')
        self.canvas.get_tk_widget().pack(fill='both',expand=True)

    def update_pid_note(self, *args):
        self.pid_note.set('yaw: angle outer loop parameters only. Current firmware runs yaw rate control; this outer loop is inactive.' if self.pid_var.get() == 'yaw' else '')

    def mark_dirty(self):
        self.dirty = True

    def refresh_ports(self):
        if self.port:
            return
        ports = [p.device for p in list_ports.comports()]
        self.port_combo['values'] = ports
        if self.port_var.get() not in ports:
            self.port_var.set(ports[0] if ports else '')

    def toggle_connection(self):
        if self.port:
            self.disconnect()
            return
        if not self.port_var.get():
            messagebox.showerror('Connection','Select a COM port.')
            return
        try:
            self.port = serial.Serial(self.port_var.get(),115200,timeout=0,write_timeout=0.1)
        except (serial.SerialException,OSError) as exc:
            messagebox.showerror('Connection',str(exc))
            return
        self.reset_stream()
        self.axis_status.set('Actual telemetry axis: waiting')
        self.port_combo.configure(state='disabled')
        self.connect_button.configure(text='Disconnect')
        self.command_buttons('normal')
        self.csv_button.configure(state='normal')
        self.link_var.set(f'Connected: {self.port_var.get()}')

    def disconnect(self, reason='Disconnected'):
        if self.pending:
            self.status_var.set('Connection lost: command outcome unknown.')
        self.pending = None
        port, self.port = self.port, None
        if port:
            try:
                port.close()
            except (serial.SerialException,OSError):
                pass
        self.stop_csv()
        self.port_combo.configure(state='readonly')
        self.connect_button.configure(text='Connect')
        self.command_buttons('disabled')
        self.csv_button.configure(state='disabled')
        self.link_var.set(reason)

    def apply_pid(self):
        if not self.port or self.pending:
            return
        try:
            gains = [gain_units(v.get(),m) for v,m in zip((self.kp_var,self.ki_var,self.kd_var),(20,10,2))]
        except ValueError as exc:
            messagebox.showerror('Invalid PID',str(exc))
            return
        name = self.pid_var.get()
        seq, pid = secrets.randbits(32), IDS[name]
        command = f'PID,{seq},{pid},{gains[0]},{gains[1]},{gains[2]}\n'.encode('ascii')
        self.pending_kind = "PID"
        self.pending = (seq,pid,gains,name,time.monotonic()+4)
        self.command_buttons('disabled')
        self.status_var.set(f'Waiting for flight controller: {name}, seq={seq}')
        try:
            if self.port.write(command) != len(command):
                raise serial.SerialTimeoutException('Partial command write')
        except (serial.SerialException,OSError) as exc:
            self.disconnect(f'USB error: {exc}')

    def command_buttons(self, state):
        self.apply_button.configure(state=state)
        self.axis_button.configure(state=state)

    def apply_axis(self):
        if not self.port or self.pending:
            return
        axis = 'XYZ'.index(self.axis_var.get())
        seq = secrets.randbits(32)
        command = f'AXIS,{seq},{axis}\n'.encode('ascii')
        self.pending_kind = 'AXIS'
        self.pending = (seq, axis, [0,0,0], self.axis_var.get(), time.monotonic()+4)
        self.command_buttons('disabled')
        self.status_var.set(f'Requesting telemetry axis {self.axis_var.get()}...')
        try:
            if self.port.write(command) != len(command):
                raise serial.SerialTimeoutException('Partial axis command write')
        except (serial.SerialException,OSError) as exc:
            self.disconnect(f'USB error: {exc}')

    def handle_line(self, text, now):
        if text.startswith(('PID_', 'AXIS_')):
            fields = text.strip().split(',')
            if not self.pending or len(fields)!=7:
                return
            try:
                values = list(map(int,fields[1:]))
            except ValueError:
                return
            seq,pid,gains,name,_ = self.pending
            if values[:2]!=[seq,pid] or values[3:]!=gains:
                return
            if fields[0] not in tuple(self.pending_kind + '_' + kind for kind in ('APPLIED','REJECTED','BUSY','TIMEOUT')):
                return
            if fields[0]=='AXIS_APPLIED' and values[2]==0:
                self.status_var.set(f'Axis command applied: {name}. Plot follows tagged telemetry packets.')
            elif fields[0]=='PID_APPLIED' and values[2]==0:
                self.status_var.set(f'Applied {name}: Kp={gains[0]/10000:g}, Ki={gains[1]/10000:g}, Kd={gains[2]/10000:g} (RAM only)' + ('; yaw outer loop inactive' if name == 'yaw' else ''))
            else:
                self.status_var.set(f'{fields[0]}: not confirmed. Timeout means outcome unknown.')
            self.pending = None
            self.command_buttons('normal')
            return
        result = parse_line(text)
        if result is None:
            return
        if isinstance(result,int):
            self.ack_sequence = result
            return
        elapsed,reset = self.clock.update(result[1])
        if reset or result[-1] != self.current_axis:
            self.samples.clear()
            self.previous_sequence = None
        self.current_axis = result[-1]
        self.axis_status.set(f'Actual telemetry axis: {"XYZ"[self.current_axis]}')
        row = (now-self.started,elapsed,*result)
        duplicate = result[0]==self.previous_sequence
        if not duplicate:
            self.samples.append(row)
        if self.writer:
            try:
                self.writer.writerow(row)
            except (OSError,ValueError) as exc:
                self.stop_csv(f'CSV error: {exc}')
        self.received += 1
        self.duplicates += duplicate
        self.previous_sequence = result[0]
        self.last_sample = now
        self.dirty = True

    def poll(self):
        now = time.monotonic()
        if self.port:
            try:
                chunk = self.port.read(min(self.port.in_waiting,8192))
                for text in self.framing.feed(chunk):
                    self.handle_line(text,now)
            except (serial.SerialException,OSError) as exc:
                self.disconnect(f'USB error: {exc}')
        if self.pending and now>=self.pending[4]:
            self.pending = None
            self.status_var.set('No application confirmation. Command outcome unknown.')
            self.command_buttons('normal' if self.port else 'disabled')
        if self.csv_file and now-self.last_flush>=1:
            try:
                self.csv_file.flush()
                self.last_flush = now
            except OSError as exc:
                self.stop_csv(f'CSV error: {exc}')
        if now-self.last_draw>=0.1:
            self.draw(now)
            self.last_draw = now
        self.after_id = self.root.after(20,self.poll)

    def draw(self, now):
        if self.dirty:
            times = [r[1] for r in self.samples]
            for line,column in self.lines:
                line.set_data(times,[r[column] for r in self.samples])
            if self.follow_var.get():
                right = max(self.window,times[-1]) if times else self.window
                self.axes[-1].set_xlim(right-self.window,right)
            self.dirty = False
        if self.last_sample is not None:
            self.title.set_text(f'{"XYZ"[self.current_axis]} PID | seq={self.previous_sequence} | received={self.received} | repeated={self.duplicates} | age={now-self.last_sample:.1f}s')
        elif self.ack_sequence is not None:
            self.title.set_text(f'ACK seq={self.ack_sequence}; waiting for PID telemetry')
        else:
            self.title.set_text('PID - waiting for telemetry')
        self.canvas.draw_idle()

    def apply_ranges(self):
        try:
            window = float(self.window_var.get())
            ranges = [tuple(float(v.get()) for v in pair) for pair in self.range_vars]
            if not math.isfinite(window) or window<=0 or any(not all(math.isfinite(v) for v in p) or p[0]>=p[1] for p in ranges):
                raise ValueError('Use finite min < max and a positive time window.')
        except ValueError as exc:
            messagebox.showerror('Plot ranges',str(exc))
            return
        self.window = window
        for ax,pair in zip(self.axes,ranges):
            ax.set_ylim(*pair)
        self.dirty = True

    def toggle_csv(self):
        if self.csv_file:
            self.stop_csv()
            return
        if not self.port:
            return
        path = filedialog.asksaveasfilename(title='Record to a NEW CSV file',defaultextension='.csv',filetypes=[('CSV','*.csv')],initialfile=time.strftime('telemetry_%Y%m%d_%H%M%S.csv'))
        if not path:
            return
        try:
            self.csv_file = open(path,'x',newline='',encoding='utf-8')
            self.writer = csv.writer(self.csv_file)
            self.writer.writerow(CSV_FIELDS)
        except (OSError,ValueError) as exc:
            self.stop_csv(f'CSV error: {exc}')
            messagebox.showerror('CSV',str(exc))
            return
        self.csv_button.configure(text='Stop CSV recording')
        self.csv_var.set(f'Recording: {path}')

    def stop_csv(self, message='Not recording'):
        file,self.csv_file = self.csv_file,None
        self.writer = None
        if file:
            try:
                file.close()
            except OSError as exc:
                message = f'CSV close error: {exc}'
        self.csv_button.configure(text='Start CSV recording')
        self.csv_var.set(message)

    def close(self):
        self.root.after_cancel(self.after_id)
        self.disconnect()
        self.root.destroy()


def main():
    root = tk.Tk()
    PIDGui(root)
    root.mainloop()


if __name__ == '__main__':
    main()
