from .protocol import NanoAsciiProtocol, Action, MotorStatus, SequenceTracker, SequencePhase
class Coordinator:
    def __init__(self,protocol=None):
        self.protocol=protocol or NanoAsciiProtocol()
        self.tracker=SequenceTracker();self.status=MotorStatus()
        self.connected=False;self.ready_for_motion=False;self.stop_latched=False
    def set_connected(self,connected):
        self.connected=connected;self.ready_for_motion=False;self.stop_latched=False
        self.status=MotorStatus();self.tracker.reset()
    def prepare(self,action):
        if not self.connected:raise RuntimeError('Connect a device first')
        if self.tracker.mode:raise RuntimeError('Deployment active; commands locked')
        if self.stop_latched and action not in (Action.STATUS,Action.HELP,Action.TEST_SPI):
            raise RuntimeError('Stop requested; reconnect and verify hardware')
        if not self.ready_for_motion and action not in (Action.STATUS,Action.HELP,Action.TEST_SPI):
            raise RuntimeError('Wait for a status reading before moving')
        if action in (Action.DEPLOY_BARREL,Action.DEPLOY_FMS):self.tracker.start(action)
        return self.protocol.encode(action)
    def prepare_yaw(self,value,relative=False,during_prompt=False):
        if not self.connected or not self.ready_for_motion or self.stop_latched:raise RuntimeError('Device not ready')
        if self.tracker.mode and (not during_prompt or self.tracker.phase!=SequencePhase.AWAIT_FIRE):
            raise RuntimeError('Yaw only at firmware fire prompt')
        return self.protocol.yaw_relative(value) if relative else self.protocol.yaw_absolute(value)
    def prepare_barrel_index(self,index):
        if not self.connected or not self.ready_for_motion or self.tracker.mode or self.stop_latched:
            raise RuntimeError('Device not ready')
        return self.protocol.barrel_index(index)
    def prepare_confirm(self):
        if not self.connected:raise RuntimeError('Device disconnected')
        self.tracker.confirm();return self.protocol.confirm_fire()
    def prepare_cancel(self):
        if not self.connected:raise RuntimeError('Device disconnected')
        self.tracker.cancel();return self.protocol.cancel_deployment()
    def prepare_stop(self):
        if not self.connected:raise RuntimeError('Device disconnected')
        self.stop_latched=True;self.ready_for_motion=False;self.tracker.stop_request()
        return self.protocol.interrupt()
    def observe(self,line):
        self.status=self.protocol.parse_status_line(self.status,line)
        # A single status field is not a complete response. The transport
        # enables movement only after the final sensor line arrives.
        return self.tracker.observe(line)
