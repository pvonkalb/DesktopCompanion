"""PC load for the device's PC screen.

Anti-cheat safe: CPU / RAM / network come from Windows' own performance APIs via
psutil; GPU load and temperature from NVIDIA's official NVML library (user mode,
the same source as nvidia-smi). No kernel drivers, so no CPU temperature.
"""

from __future__ import annotations

import logging
import time

import psutil

log = logging.getLogger("stats")


class StatsCollector:
    def __init__(self):
        psutil.cpu_percent(None)   # prime the delta
        self._net = psutil.net_io_counters()
        self._net_t = time.monotonic()
        self._nvml = None
        self.gpu_name = ""
        try:
            import pynvml
            pynvml.nvmlInit()
            self._nvml = pynvml
            self._gpu = pynvml.nvmlDeviceGetHandleByIndex(0)
            self.gpu_name = str(pynvml.nvmlDeviceGetName(self._gpu))
            log.info("GPU: %s", self.gpu_name)
        except Exception as e:   # no NVIDIA GPU / driver: GPU tile shows "n/a"
            log.info("GPU stats unavailable: %s", e)

    def sample(self) -> dict:
        now = time.monotonic()
        net = psutil.net_io_counters()
        dt = max(0.1, now - self._net_t)
        down = max(0, int((net.bytes_recv - self._net.bytes_recv) / dt))
        up = max(0, int((net.bytes_sent - self._net.bytes_sent) / dt))
        self._net, self._net_t = net, now

        mem = psutil.virtual_memory()
        stats = {
            "cpu": round(psutil.cpu_percent(None)),
            "ram": round(mem.percent),
            "ru": round(mem.used / 2**30, 1),     # GB used
            "rt": round(mem.total / 2**30),       # GB total
            "dn": down,                           # bytes/s
            "up": up,
            "gpu": -1,
            "gt": -1,                             # GPU temperature, deg C
        }
        if self._nvml:
            try:
                stats["gpu"] = int(self._nvml.nvmlDeviceGetUtilizationRates(self._gpu).gpu)
                stats["gt"] = int(self._nvml.nvmlDeviceGetTemperature(self._gpu, self._nvml.NVML_TEMPERATURE_GPU))
            except Exception as e:
                log.debug("NVML read failed: %s", e)
        return stats
