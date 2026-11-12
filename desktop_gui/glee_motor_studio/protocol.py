from __future__ import annotations
from dataclasses import dataclass, replace
from enum import Enum
import math, re
from typing import Protocol

class Action(str, Enum):
    STATUS='status'; HELP='help'; TEST_SPI='test_spi'
    LEAD_UP_PUCK='lead_up_puck'; LEAD_DOWN_PUCK='lead_down_puck'
    LEAD_UP_MM='lead_up_mm'; LEAD_DOWN_MM='lead_down_mm'; LEAD_HOME='lead_home'
    LEAD_FULL_DOWN='lead_full_down'; BARREL_NEXT='barrel_next'; BARREL_PREVIOUS='barrel_previous'
    SERVO_RESET='servo_reset'; SERVO_ARM='servo_arm'; SERVO_FIRE='servo_fire'
    YAW_ZERO='yaw_zero'; STOP='stop'; DEPLOY_BARREL='deploy_barrel'; DEPLOY_FMS='deploy_fms'

@dataclass(frozen=True)
class Limits:
    yaw_min:float=-30.0
    yaw_max:float=30.0
    barrel_indexes:int=6

@dataclass(frozen=True)
class MotorStatus:
    lead_mm:float|None=None
    lead_limit_mm:float|None=None
    lead_steps:int|None=None
    puck_level:int|None=None
    puck_level_unknown:bool=False
    puck_count:int|None=None
    barrel_index:int|None=None
    barrel_count:int|None=None
    yaw_degrees:float|None=None
    yaw_min:float|None=None
    yaw_max:float|None=None
    bottom_switch:str|None=None
    puck_switch:str|None=None
    updated:bool=False

class MotorProtocol(Protocol):
    limits:Limits
    def encode(self,action:Action)->bytes:...
    def yaw_absolute(self,angle:float)->bytes:...
    def yaw_relative(self,delta:float)->bytes:...
    def barrel_index(self,index:int)->bytes:...
    def confirm_fire(self)->bytes:...
    def cancel_deployment(self)->bytes:...
    def interrupt(self)->bytes:...
    def parse_status_line(self,old:MotorStatus,line:str)->MotorStatus:...

class NanoAsciiProtocol:
    limits=Limits()
    commands={
        Action.STATUS:'C',Action.HELP:'?',Action.TEST_SPI:'T',
        Action.LEAD_UP_PUCK:'W',Action.LEAD_DOWN_PUCK:'S',
        Action.LEAD_UP_MM:'+',Action.LEAD_DOWN_MM:'-',
        Action.LEAD_HOME:'H',Action.LEAD_FULL_DOWN:'B',
        Action.BARREL_NEXT:'N',Action.BARREL_PREVIOUS:'P',
        Action.SERVO_RESET:'w',Action.SERVO_ARM:'e',
        Action.SERVO_FIRE:'M',Action.YAW_ZERO:'Z',
        Action.STOP:'X',Action.DEPLOY_BARREL:'G',Action.DEPLOY_FMS:'Q'}
    @staticmethod
    def _line(text:str)->bytes:
        if '\r' in text or '\n' in text or len(text)>=32:raise ValueError('Invalid serial command')
        return (text+'\n').encode('ascii')
    def encode(self,action:Action)->bytes:return self._line(self.commands[action])
    def yaw_absolute(self,angle:float)->bytes:
        if not math.isfinite(angle) or not self.limits.yaw_min<=angle<=self.limits.yaw_max:
            raise ValueError('Yaw target outside configured limits')
        return self._line('Y'+f'{angle:.3f}'.rstrip('0').rstrip('.'))
    def yaw_relative(self,delta:float)->bytes:
        if not math.isfinite(delta) or abs(delta)>60:raise ValueError('Invalid yaw delta')
        return self._line('R'+f'{delta:.3f}'.rstrip('0').rstrip('.'))
    def barrel_index(self,index:int)->bytes:
        if type(index) is not int or index not in range(self.limits.barrel_indexes):
            raise ValueError('Invalid barrel index')
        return self._line('I'+str(index))
    def confirm_fire(self)->bytes:return b'\n'
    def cancel_deployment(self)->bytes:return b'N\n'
    def interrupt(self)->bytes:return b'x\n'
    def parse_status_line(self,old:MotorStatus,line:str)->MotorStatus:
        s=line.strip()
        m=re.match(r'^Lead (?:screw|position):\s*([-+\d.]+)\s*/\s*([-+\d.]+)\s*mm\s*\(([-+\d]+)\s*steps\)',s,re.I)
        if m:return replace(old,lead_mm=float(m[1]),lead_limit_mm=float(m[2]),lead_steps=int(m[3]),updated=True)
        m=re.match(r'^Lead puck level:\s*(\d+)\s*/\s*(\d+)',s,re.I)
        if m:return replace(old,puck_level=int(m[1]),puck_count=int(m[2]),puck_level_unknown=False,updated=True)
        if s.lower().startswith('lead puck level:') and 'unknown' in s.lower():
            return replace(old,puck_level=None,puck_level_unknown=True,updated=True)
        m=re.match(r'^Barrel index:\s*(\d+)\s+of\s+(\d+)',s,re.I)
        if m:return replace(old,barrel_index=int(m[1]),barrel_count=int(m[2]),updated=True)
        m=re.match(r'^Yaw:\s*([-+\d.]+)\s*degrees',s,re.I)
        if m:return replace(old,yaw_degrees=float(m[1]),updated=True)
        m=re.match(r'^Yaw allowed range:\s*([-+\d.]+)\s*to\s*\+?([-+\d.]+)\s*degrees',s,re.I)
        if m:return replace(old,yaw_min=float(m[1]),yaw_max=float(m[2]),updated=True)
        m=re.match(r'^(?:Elevator\s+)?Bottom\s+Switch:\s*(.+)$',s,re.I)
        if m:return replace(old,bottom_switch=m[1].strip(),updated=True)
        m=re.match(r'^Puck(?:\s+in\s+Barrel)?\s+Switch:\s*(.+)$',s,re.I)
        if m:return replace(old,puck_switch=m[1].strip(),updated=True)
        return old

class SequencePhase(str,Enum):
    IDLE='idle'; RUNNING='running'; AWAIT_FIRE='await_fire'; STOP_REQUESTED='stop_requested'; FINISHED='finished'; UNCERTAIN='uncertain'

class SequenceTracker:
    def __init__(self):
        self.mode=None;self.phase=SequencePhase.IDLE
        self.prompt_count=0;self.confirmed_count=0;self.yaw_refresh=False
    def start(self,mode):
        if mode not in (Action.DEPLOY_BARREL,Action.DEPLOY_FMS):raise ValueError('Not a deployment')
        if self.mode:raise RuntimeError('Deployment already active')
        self.mode=mode;self.phase=SequencePhase.RUNNING
        self.prompt_count=0;self.confirmed_count=0;self.yaw_refresh=False
    def observe(self,line):
        s=line.lower().strip()
        if not self.mode:return None
        if 'press enter to fire' in s:
            if self.phase!=SequencePhase.AWAIT_FIRE and not self.yaw_refresh:self.prompt_count+=1
            self.yaw_refresh=False;self.phase=SequencePhase.AWAIT_FIRE
            return self.phase
        if ('fms deployment complete' in s or 'deployment cancelled' in s or
            'fms stopped' in s or 'deployment stopped' in s or
            'barrel deployment did not complete' in s or 'barrel indexing failed' in s or
            'full deployment finished, but' in s or 'full deployment stopped' in s):
            self.reset()
            return SequencePhase.FINISHED
        return None
    def yaw_adjustment_sent(self):
        if self.phase!=SequencePhase.AWAIT_FIRE:raise RuntimeError('Not at fire prompt')
        self.yaw_refresh=True;self.phase=SequencePhase.RUNNING
    def confirm(self):
        if self.phase!=SequencePhase.AWAIT_FIRE:raise RuntimeError('Firmware has not requested confirmation')
        self.confirmed_count+=1;self.phase=SequencePhase.RUNNING
    def cancel(self):
        if self.phase!=SequencePhase.AWAIT_FIRE:raise RuntimeError('Cancel only at fire prompt')
        self.phase=SequencePhase.RUNNING
    def stop_request(self):
        if self.mode:self.phase=SequencePhase.STOP_REQUESTED
    def mark_uncertain(self):self.phase=SequencePhase.UNCERTAIN
    def reset(self):
        self.mode=None;self.phase=SequencePhase.IDLE
