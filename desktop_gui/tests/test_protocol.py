import math
import pytest
from glee_motor_studio.protocol import Action,NanoAsciiProtocol,MotorStatus,SequenceTracker,SequencePhase
from glee_motor_studio.coordinator import Coordinator

def test_ascii_commands():
    p=NanoAsciiProtocol()
    assert p.encode(Action.LEAD_HOME)==b'H\n'
    assert p.encode(Action.HELP)==b'?\n'
    assert p.encode(Action.SERVO_RESET)==b'w\n'
    assert p.encode(Action.LEAD_UP_PUCK)==b'W\n'
    assert p.encode(Action.DEPLOY_FMS)==b'Q\n'
    assert p.confirm_fire()==b'\n'
    assert p.cancel_deployment()==b'N\n'
    assert p.interrupt()==b'x\n'
def test_numeric_validation():
    p=NanoAsciiProtocol()
    assert p.yaw_absolute(-12.5)==b'Y-12.5\n'
    assert p.yaw_relative(.25)==b'R0.25\n'
    assert p.barrel_index(5)==b'I5\n'
    for value in [-31,31,math.nan,math.inf]:
        with pytest.raises(ValueError):p.yaw_absolute(value)
    for index in [-1,6,3.5,True]:
        with pytest.raises(ValueError):p.barrel_index(index)
def test_status():
    p=NanoAsciiProtocol();s=MotorStatus()
    for line in ['Lead screw: 28.333 / 256.000 mm (2856 steps)','Lead puck level: 2 / 17',
                 'Barrel index: 5 of 6','Yaw: -12.465340 degrees',
                 'Yaw allowed range: -30.0 to +30.0 degrees']:
        s=p.parse_status_line(s,line)
    assert s.lead_mm==28.333 and s.puck_level==2 and s.barrel_index==5
    assert s.yaw_degrees==-12.46534
def test_fire_confirmation_state():
    t=SequenceTracker();t.start(Action.DEPLOY_FMS)
    assert t.observe('Press Enter to fire, or type N to cancel.')==SequencePhase.AWAIT_FIRE
    t.yaw_adjustment_sent()
    t.observe('Press Enter to fire, or type N to cancel.')
    assert t.prompt_count==1
    t.confirm()
    assert t.observe('FMS deployment complete.')==SequencePhase.FINISHED
def test_guards():
    c=Coordinator()
    with pytest.raises(RuntimeError):c.prepare(Action.STATUS)
    c.set_connected(True)
    with pytest.raises(RuntimeError):c.prepare(Action.LEAD_HOME)
    c.observe('Yaw: 0 degrees')
    assert c.prepare(Action.DEPLOY_BARREL)==b'G\n'
    with pytest.raises(RuntimeError):c.prepare(Action.STATUS)
    c.observe('Press Enter to fire, or type N to cancel.')
    assert c.prepare_confirm()==b'\n'
    assert c.prepare_stop()==b'x\n'
