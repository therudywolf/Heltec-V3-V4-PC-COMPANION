"""Unit tests for server/nvidia_smi.py (GPU CSV parser)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
import nvidia_smi as nv  # noqa: E402


class TestParse:
    def test_real_line(self):
        # temp, gpu%, mem%, used MiB, total MiB, power W, clock MHz, fan%
        hw = nv.parse_nvidia_smi_csv("31, 32, 16, 2711, 12282, 19.14, 210, 31")
        assert hw["gt"] == 31 and hw["gh"] == 31
        assert hw["gl"] == 32
        assert hw["gv"] == 16
        assert hw["vu"] == 2.6      # 2711/1024 -> 2.6
        assert hw["vt"] == 12.0     # 12282/1024 -> 12.0
        assert hw["pw"] == 19
        assert hw["gclock"] == 210
        assert hw["gf"] == 31

    def test_first_gpu_only(self):
        two = "31, 32, 16, 2711, 12282, 19, 210, 31\n40, 50, 20, 3000, 12282, 50, 800, 45"
        hw = nv.parse_nvidia_smi_csv(two)
        assert hw["gt"] == 31  # first row wins

    def test_na_fields_omitted(self):
        hw = nv.parse_nvidia_smi_csv("45, [N/A], 10, 2000, 8192, [N/A], 1500, [N/A]")
        assert hw["gt"] == 45
        assert "gl" not in hw and "pw" not in hw and "gf" not in hw
        assert hw["gv"] == 10

    def test_empty_and_garbage(self):
        assert nv.parse_nvidia_smi_csv("") == {}
        assert nv.parse_nvidia_smi_csv("\n\n") == {}
        assert nv.parse_nvidia_smi_csv("only,three,fields") == {}

    def test_never_raises(self):
        for bad in (None, "x", "1,2,3,4,5,6,7,abc"):
            nv.parse_nvidia_smi_csv(bad)


class TestQuery:
    def test_missing_tool_returns_empty(self):
        # a command that doesn't exist -> {} (never raises)
        assert nv.query_gpu_sync(cmd=["definitely_not_a_real_binary_xyz"]) == {}
