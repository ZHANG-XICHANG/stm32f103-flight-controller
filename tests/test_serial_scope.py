import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('scope', Path(__file__).resolve().parents[1] / 'serial_scope.py')
scope = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scope)


class ScopeTests(unittest.TestCase):
    def test_scaling(self):
        self.assertEqual(scope.parse_line('TEL,4294967295,1234,-1250,100,-123,223,123,-10,-32768,1100,1940,0,65535,2\r\n'),
                         (4294967295, 1234, -12.5, 10.0, -12.3, 22.3, 123, -10, -32768, 1100, 1940, 0, 65535, 2))

    def test_clock_wrap_reset(self):
        clock = scope.SampleClock()
        self.assertEqual(clock.update(0xfffffffe), (0.0, False))
        self.assertEqual(clock.update(4), (0.006, False))
        self.assertEqual(clock.update(4), (0.006, False))
        self.assertEqual(clock.update(0), (0.0, True))
        self.assertEqual(clock.update(6), (0.006, False))

    def test_new_field_validation(self):
        valid = ['TEL', '1', '1234', '100', '200', '300', '-100', '123', '-10', '113', '1100', '1200', '1300', '1400', '0']
        for index, bad in ((1, '-1'), (2, '-1'), (2, '4294967296'),
                           (3, '32768'), (7, '32768'), (7, '-32769'),
                           (7, 'nan'), (9, '-32769'), (10, '-1'), (13, '65536'), (14, '3'), (14, '-1')):
            parts = valid.copy()
            parts[index] = bad
            self.assertIsNone(scope.parse_line(','.join(parts)))
        self.assertIsNone(scope.parse_line('TEL,1,1234,100,200,300,-100,-10,113'))

    def test_old_22_byte_text_rejected(self):
        self.assertIsNone(scope.parse_line('TEL,1,1234,100,200,300,-100,123,-10,113'))

    def test_sequence_only(self):
        self.assertEqual(scope.parse_line('TEL_SEQ,0'), 0)

    def test_invalid_and_debug(self):
        for text in ('', 'debug message', 'TEL,1,2,3', 'TEL,1,2,3,4,5,6',
                     'TEL,-1,0,0,0', 'TEL,4294967296,0,0,0', 'TEL,1,32768,0,0',
                     'TEL,1,0,-32769,0', 'TEL,1,nan,0,0', 'TEL_SEQ,no'):
            self.assertIsNone(scope.parse_line(text), text)

    def test_fragmented_and_combined_lines(self):
        buffer = scope.LineBuffer()
        self.assertEqual(list(buffer.feed(b'TEL,1,-12')), [])
        self.assertEqual(list(buffer.feed(b'3,456,7\r\nTEL_SEQ,2\n')),
                         ['TEL,1,-123,456,7\r', 'TEL_SEQ,2'])

    def test_bad_bytes_and_oversized_line_recovery(self):
        buffer = scope.LineBuffer()
        self.assertEqual(list(buffer.feed(b'TEL,1,0,\xff,0\n')), [])
        self.assertEqual(list(buffer.feed(b'x' * 1000)), [])
        self.assertEqual(list(buffer.feed(b'TEL_SEQ,3\nTEL_SEQ,4\n')), ['TEL_SEQ,4'])


if __name__ == '__main__':
    unittest.main()
