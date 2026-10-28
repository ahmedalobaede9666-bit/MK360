import importlib.util
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("verify_xex",ROOT/"tools/verify_xex.py")
verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)

class XexChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=(ROOT/"build/MK360.xex").read_bytes()
    def test_real_build_is_system_dll(self):
        result=verifier.verify(self.data)
        self.assertEqual(result["format"],"XEX2 system DLL")
        self.assertGreater(result["import_records"],20)
    def test_corrupt_image_is_rejected(self):
        bad=bytearray(self.data);bad[-100]^=1
        with self.assertRaisesRegex(ValueError,"page SHA-1"):verifier.verify(bad)
    def test_corrupt_header_is_rejected(self):
        bad=bytearray(self.data);bad[1000]^=1
        with self.assertRaises(ValueError):verifier.verify(bad)
    def test_foreground_title_flag_is_rejected(self):
        bad=bytearray(self.data);bad[7]=1
        with self.assertRaisesRegex(ValueError,"DLL"):verifier.verify(bad)
    def test_truncation_is_rejected(self):
        for count in (0,3,23,len(self.data)-1):
            with self.subTest(length=count),self.assertRaises(ValueError):verifier.verify(self.data[:count])

if __name__=="__main__":unittest.main()
