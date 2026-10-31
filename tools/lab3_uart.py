"""Parse complete timestamp fields from a possibly partial real UART stream."""
import re

def status_times(data):
    # The following space proves that all timestamp digits have arrived.
    return [int(v) for v in re.findall(rb'VG STATUS ms=(\d+) ',data)]

def metrics_complete(data, require_tx=True):
    offset=data.rfind(b'VG METRICS ')
    if offset<0:return False
    block=data[offset:data.rfind(b'\n')+1]
    tasks={b'console',b'customer',b'mechanical',b'light',b'recovery',b'audit',b'telemetry',b'heartbeat'}
    spans=tasks|{b'loop',b'loop_interval',b'pir_latency',b'command_latency',b'command_complete_latency',b'clock_read'}
    measured=set(re.findall(rb'^STAT name=(\w+) n=\d+ min_us=\d+ avg_us=\d+ max_us=\d+\r?$',block,re.M))
    scheduled=set(re.findall(rb'^SCHED name=(\w+) skipped=\d+ deadline_miss=\d+ jitter_min_us=-?\d+ jitter_max_us=-?\d+ release_late_max_us=\d+\r?$',block,re.M))
    jitter=set(re.findall(rb'^JITTER name=(\w+) n=\d+ avg_us=-?\d+ abs_le10=\d+ abs_le100=\d+ abs_le1000=\d+ abs_gt1000=\d+\r?$',block,re.M))
    counter_lines=re.findall(rb'^COUNTERS ((?:\w+=\d+ )*\w+=\d+)\r?$',block,re.M)
    counter_keys=set(re.findall(rb'(\w+)=',counter_lines[-1])) if counter_lines else set()
    expected={b'rx_dropped',b'rx_overrun',b'pir_dropped',b'audit_dropped',b'audit_failed',b'audit_queued'}
    if require_tx:expected|={b'tx_timeouts',b'tx_errors'}
    memory=re.search(rb'^MEM stack_used=\d+ stack_reserve=\d+ heap_arena=\d+ heap_used=\d+ heap_free=\d+\r?$',block,re.M)
    return spans<=measured and tasks<=scheduled and tasks<=jitter and expected<=counter_keys and memory is not None

if __name__=='__main__':
    assert status_times(b'VG STATUS ms=9075 mode=guard\r\nVG STATUS ms=360')==[9075]
    assert status_times(b'VG STATUS ms=9075 mode=guard\r\nVG STATUS ms=3600000 mode=guard')==[9075,3600000]
    assert status_times(b'VG STATUS ms=')==[]
    assert status_times(b'VG STATUS ms=3600000')==[]
    print('PASS HOST UART parser: partial timestamp cannot become a wrapped endurance interval')
