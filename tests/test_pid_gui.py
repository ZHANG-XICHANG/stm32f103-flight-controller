import csv
import tempfile
import time
import tkinter as tk
import unittest
from pathlib import Path
from unittest.mock import patch
import pid_tune as gui

TEL = 'TEL,1,1000,100,200,300,-100,123,-10,113,1100,1200,1800,1940,0\n'
class Port:
    def __init__(self, *a, **k):
        self.data = b''
        self.writes = []
        self.closed = False
    @property
    def in_waiting(self): return len(self.data)
    def read(self, n):
        data, self.data = self.data[:n], self.data[n:]
        return data
    def write(self, data):
        self.writes.append(data)
        return len(data)
    def close(self): self.closed = True

class GuiTests(unittest.TestCase):
    def setUp(self):
        self.root = tk.Tk()
        self.root.withdraw()
        with patch.object(gui.list_ports, 'comports', return_value=[]):
            self.app = gui.PIDGui(self.root)
        self.port = Port()
        self.app.port_var.set('TEST')
        with patch.object(gui.serial, 'Serial', return_value=self.port) as opening:
            self.app.toggle_connection()
            opening.assert_called_once()
    def tearDown(self): self.app.close()
    def tick(self):
        self.root.after_cancel(self.app.after_id)
        self.app.poll()
    def test_shared_stream_and_csv(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)/'data.csv'
            with patch.object(gui.filedialog,'asksaveasfilename',return_value=str(path)):
                self.app.toggle_csv()
            with patch.object(gui.secrets,'randbits',return_value=123), patch.object(gui.serial,'Serial',side_effect=AssertionError('must reuse connection')):
                self.app.apply_pid()
            self.assertEqual(len(self.port.writes),1)
            self.app.apply_pid()
            self.assertEqual(len(self.port.writes),1)
            self.port.data = (TEL + 'PID_APPLIED,999,0,0,10000,0,0\nPID_APPL').encode()
            self.tick()
            self.assertIsNotNone(self.app.pending)
            self.assertEqual(len(self.app.samples),1)
            self.port.data = ('IED,123,0,0,10000,0,0\n'+TEL).encode()
            self.tick()
            self.assertIsNone(self.app.pending)
            self.assertIn('Applied roll', self.app.status_var.get())
            self.app.draw(time.monotonic())
            self.assertEqual(len(self.app.axes),5)
            self.assertEqual([list(l.get_ydata()) for l in self.app.axes[4].lines],[[1100],[1200],[1800],[1940]])
            self.assertEqual(self.app.duplicates,1)
            self.app.stop_csv()
            with path.open() as file: rows=list(csv.reader(file))
            self.assertEqual(len(rows),3)
            self.assertEqual(rows[0],gui.CSV_FIELDS)
            self.assertEqual(rows[1][-5:-1],['1100','1200','1800','1940'])
    def test_timeout_disconnect_reconnect_and_reset(self):
        self.app.apply_pid()
        p=list(self.app.pending);p[4]=0;self.app.pending=tuple(p)
        self.tick()
        self.assertIsNone(self.app.pending)
        self.assertIn('unknown',self.app.status_var.get())
        self.app.handle_line(TEL,time.monotonic())
        self.app.handle_line(TEL.replace(',1,1000,',',2,0,'),time.monotonic())
        self.assertEqual(len(self.app.samples),1)
        self.app.apply_pid()
        self.app.disconnect()
        self.assertTrue(self.port.closed)
        self.assertIn('unknown',self.app.status_var.get())
        with patch.object(gui.serial,'Serial',return_value=Port()): self.app.toggle_connection()
        self.assertEqual(len(self.app.samples),0)
    def test_axis_switch_keeps_old_packets_correctly_labelled(self):
        self.app.handle_line(TEL,time.monotonic())
        self.app.axis_var.set('Y')
        with patch.object(gui.secrets,'randbits',return_value=88): self.app.apply_axis()
        self.assertEqual(self.port.writes[-1],b'AXIS,88,1\n')
        self.assertEqual(self.app.current_axis,0)
        self.app.apply_pid()
        self.assertEqual(len(self.port.writes),1)
        self.app.handle_line('PID_APPLIED,88,1,0,0,0,0',time.monotonic())
        self.assertIsNotNone(self.app.pending)
        self.app.handle_line('AXIS_APPLIED,88,1,0,0,0,0',time.monotonic())
        self.assertIsNone(self.app.pending)
        self.assertEqual(self.app.current_axis,0)
        y = TEL.replace(',1,1000,',',2,1006,').rsplit(',',1)[0]+',1\n'
        self.app.handle_line(y,time.monotonic())
        self.assertEqual(self.app.current_axis,1)
        self.assertEqual(len(self.app.samples),1)
        self.app.draw(time.monotonic())
        self.assertTrue(self.app.title.get_text().startswith('Y PID'))
        self.assertEqual(self.app.samples[0][-1],1)

    def test_yaw_outer_command_and_inactive_note(self):
        self.app.pid_var.set('yaw')
        self.assertIn('inactive', self.app.pid_note.get())
        with patch.object(gui.secrets, 'randbits', return_value=55):
            self.app.apply_pid()
        self.assertEqual(self.port.writes[-1], b'PID,55,5,10000,0,0\n')
        self.app.handle_line('PID_APPLIED,55,5,0,10000,0,0',time.monotonic())
        self.assertIsNone(self.app.pending)
        self.assertIn('Applied yaw',self.app.status_var.get())
        self.assertIn('inactive',self.app.status_var.get())

    def test_bad_ranges_and_csv_no_overwrite(self):
        self.app.window_var.set('nan')
        with patch.object(gui.messagebox,'showerror') as error:
            self.app.apply_ranges()
            error.assert_called_once()
        self.assertEqual(self.app.window,10)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'existing.csv';path.write_text('keep')
            with patch.object(gui.filedialog,'asksaveasfilename',return_value=str(path)), patch.object(gui.messagebox,'showerror'):
                self.app.toggle_csv()
            self.assertIsNone(self.app.writer)
            self.assertEqual(path.read_text(),'keep')

if __name__ == '__main__': unittest.main()
