"""Native Windows Qt controls for GLEE HDMMotors."""
from __future__ import annotations
import datetime, sys
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QKeySequence, QShortcut
from PySide6.QtWidgets import (QApplication,QMainWindow,QWidget,QFrame,QLabel,QPushButton,
 QComboBox,QHBoxLayout,QVBoxLayout,QGridLayout,QScrollArea,QStackedWidget,
 QPlainTextEdit,QMessageBox,QSpinBox,QDoubleSpinBox,QSizePolicy)
from .theme import STYLE
from .protocol import Action, SequencePhase
from .coordinator import Coordinator
from .serial_link import SerialLink

def label(text,kind=None,wrap=False):
    w=QLabel(text)
    if kind:w.setObjectName(kind)
    w.setWordWrap(wrap)
    return w

def button(text,callback,kind=None):
    b=QPushButton(text)
    if kind:b.setObjectName(kind)
    b.clicked.connect(callback)
    b.setCursor(Qt.PointingHandCursor)
    return b

def panel(title,eyebrow):
    p=QFrame();p.setObjectName('panel')
    l=QVBoxLayout(p);l.setContentsMargins(22,19,22,20);l.setSpacing(12)
    l.addWidget(label(eyebrow.upper(),'eyebrow'))
    l.addWidget(label(title,'subheading'))
    return p,l

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('GLEE · HDM Motor Studio')
        self.resize(1300,850);self.setMinimumSize(980,680)
        self.setStyleSheet(STYLE)
        self.ctrl=Coordinator()
        self.link=SerialLink(self)
        self.link.line_received.connect(self.on_line)
        self.link.connection_changed.connect(self.on_connection)
        self.link.fault.connect(self.on_fault)
        self.motion_buttons=[];self.deployment_buttons=[]
        self.make_ui();self.refresh_ports()
        QShortcut(QKeySequence('Escape'),self,activated=self.stop_request)
        self.refresh_ui()
        self.log_message('SYSTEM','Ready. Select the Nano COM port and connect.')

    def make_ui(self):
        base=QWidget();self.setCentralWidget(base)
        layout=QVBoxLayout(base);layout.setContentsMargins(0,0,0,0);layout.setSpacing(0)
        top=QFrame();top.setObjectName('strip')
        header=QHBoxLayout(top);header.setContentsMargins(21,13,21,13);header.setSpacing(13)
        header.addWidget(label('GLEE','brand'))
        header.addWidget(label(' / HDM MOTOR STUDIO','hint'));header.addStretch()
        self.connection_label=label('● OFFLINE','disconnected');header.addWidget(self.connection_label)
        self.port=QComboBox();self.port.setFixedWidth(195);header.addWidget(self.port)
        header.addWidget(button('↻',self.refresh_ports))
        self.connect_button=button('Connect',self.toggle_connection,'primary')
        header.addWidget(self.connect_button)
        self.stop_button=button('STOP REQUEST',self.stop_request,'danger')
        header.addWidget(self.stop_button);layout.addWidget(top)
        body=QHBoxLayout();body.setContentsMargins(0,0,0,0);body.setSpacing(0)
        layout.addLayout(body,1)
        rail=QFrame();rail.setObjectName('rail');rail.setFixedWidth(210)
        nav=QVBoxLayout(rail);nav.setContentsMargins(14,22,14,18);nav.setSpacing(10)
        nav.addWidget(label('OPERATIONS','eyebrow'))
        self.nav_buttons=[]
        for i,name in enumerate(('Overview','Manual control','Deployment','Diagnostics')):
            b=button(name,lambda idx=i:self.switch_page(idx),'nav')
            self.nav_buttons.append(b);nav.addWidget(b)
        nav.addStretch()
        nav.addWidget(label('HARDWARE SAFETY','eyebrow'))
        nav.addWidget(label('Software stop is not a physical emergency stop. Keep a power disconnect accessible.','hint',True))
        body.addWidget(rail)
        right=QWidget();rv=QVBoxLayout(right)
        rv.setContentsMargins(24,22,24,18);rv.setSpacing(13)
        self.pages=QStackedWidget()
        self.pages.addWidget(self.overview_page())
        self.pages.addWidget(self.manual_page())
        self.pages.addWidget(self.deployment_page())
        self.pages.addWidget(self.diagnostics_page())
        rv.addWidget(self.pages,4)
        rv.addWidget(label('ACTIVITY & DEVICE OUTPUT','eyebrow'))
        self.log=QPlainTextEdit();self.log.setReadOnly(True)
        self.log.setMinimumHeight(120);self.log.setMaximumHeight(180)
        self.log.document().setMaximumBlockCount(600)
        rv.addWidget(self.log,1);body.addWidget(right,1)
        self.switch_page(0)

    def new_page(self,title,subtitle):
        page=QScrollArea();page.setWidgetResizable(True)
        inner=QWidget();v=QVBoxLayout(inner)
        v.setContentsMargins(1,0,7,17);v.setSpacing(17)
        v.addWidget(label(title,'heading'));v.addWidget(label(subtitle,'hint',True))
        page.setWidget(inner);return page,v

    def overview_page(self):
        page,v=self.new_page('Mission overview','Live tracked values from the controller, not inferred from button presses.')
        grid=QGridLayout();grid.setSpacing(14)
        self.metrics=[]
        for i,(title,unit) in enumerate((('LEAD SCREW','Tracked position'),('BARREL','Chamber index'),('YAW ANGLE','Tracked degrees'))):
            p,x=panel(title,'axis telemetry')
            number=label('—','metric');x.addWidget(number)
            x.addWidget(label(unit,'hint'))
            grid.addWidget(p,0,i);self.metrics.append(number)
        v.addLayout(grid)
        p,x=panel('System readiness','live status')
        self.readiness=label('Waiting for firmware status','hint');x.addWidget(self.readiness)
        self.puck_level=label('Puck level: —','hint');x.addWidget(self.puck_level)
        self.switches=label('Switches: not reported','hint');x.addWidget(self.switches)
        x.addWidget(label('Positions may be open-loop estimates; verify actual hardware references.','hint',True))
        v.addWidget(p)
        p,x=panel('Quick access','next action')
        r=QHBoxLayout()
        self.status_button=button('Refresh status',lambda:self.command(Action.STATUS),'primary');r.addWidget(self.status_button)
        r.addWidget(button('Manual control',lambda:self.switch_page(1)))
        r.addWidget(button('Deployment',lambda:self.switch_page(2)));r.addStretch()
        x.addLayout(r);v.addWidget(p);v.addStretch()
        return page

    def add_motion_row(self,l,items):
        r=QHBoxLayout()
        for title,callback in items:
            b=button(title,callback);r.addWidget(b);self.motion_buttons.append(b)
        l.addLayout(r)

    def manual_page(self):
        page,v=self.new_page('Manual control','Send individual commands and review the hardware response.')
        grid=QGridLayout();grid.setSpacing(14)
        p,x=panel('Lead screw','axis 01')
        self.add_motion_row(x,[('↑ Up one puck',lambda:self.command(Action.LEAD_UP_PUCK)),
                               ('↓ Down one puck',lambda:self.command(Action.LEAD_DOWN_PUCK))])
        self.add_motion_row(x,[('+1 mm',lambda:self.command(Action.LEAD_UP_MM)),
                               ('−1 mm',lambda:self.command(Action.LEAD_DOWN_MM))])
        b=button('Home to bottom',self.home_lead);x.addWidget(b);self.motion_buttons.append(b)
        x.addWidget(label('Without a configured bottom switch, homing only uses the tracked software reference.','hint',True))
        grid.addWidget(p,0,0)
        p,x=panel('Rotary barrel','axis 02')
        self.add_motion_row(x,[('← Previous',lambda:self.command(Action.BARREL_PREVIOUS)),
                               ('Next →',lambda:self.command(Action.BARREL_NEXT))])
        r=QHBoxLayout();r.addWidget(label('Target index'))
        self.barrel_index=QSpinBox();self.barrel_index.setRange(0,5);r.addWidget(self.barrel_index)
        b=button('Go to index',self.move_barrel,'primary');self.motion_buttons.append(b);r.addWidget(b)
        x.addLayout(r);x.addWidget(label('Verify solenoid release and indexing clearance before moving.','hint',True))
        grid.addWidget(p,0,1)
        p,x=panel('Yaw position','axis 03')
        r=QHBoxLayout();self.yaw_target=QDoubleSpinBox();self.yaw_target.setRange(-30,30)
        self.yaw_target.setDecimals(2);self.yaw_target.setSuffix('°');r.addWidget(self.yaw_target)
        b=button('Move yaw',self.move_yaw,'primary');self.motion_buttons.append(b);r.addWidget(b);x.addLayout(r)
        self.add_motion_row(x,[('◀ −1°',lambda:self.send_yaw(-1,relative=True)),
                               ('+1° ▶',lambda:self.send_y(1,relative=True))])
        x.addWidget(label('A requested angle does not prove physical alignment.','hint',True))
        grid.addWidget(p,1,0)
        p,x=panel('Deployment servos','actuators')
        self.add_motion_row(x,[('Reset',lambda:self.command(Action.SERVO_RESET)),
                               ('Arm',lambda:self.command(Action.SERVO_ARM))])
        b=button('Fire servo pulse',self.manual_fire,'danger');x.addWidget(b)
        self.motion_buttons.append(b)
        x.addWidget(label('Raw servo fire does not perform a full puck deployment.','hint',True))
        grid.addWidget(p,1,1);v.addLayout(grid);v.addStretch();return page

    def deployment_page(self):
        page,v=self.new_page('Deployment sequencing','Each puck requires an explicit, firmware-requested firing confirmation.')
        p,x=panel('Choose deployment','sequence control')
        x.addWidget(label('Use a physical power disconnect. Never assume a USB software stop is sufficient.','hint',True))
        r=QHBoxLayout()
        a=button('Start one barrel · G',lambda:self.start_deployment(Action.DEPLOY_BARREL),'primary')
        b=button('Start full FMS · Q',lambda:self.start_deployment(Action.DEPLOY_FMS))
        self.deployment_buttons=[a,b]
        r.addWidget(a);r.addWidget(b);r.addStretch();x.addLayout(r);v.addWidget(p)
        p,x=panel('Operator confirmation','firmware handshake')
        self.sequence_label=label('No active deployment','subheading');x.addWidget(self.sequence_label)
        self.sequence_details=label('Waiting for a sequence','hint',True);x.addWidget(self.sequence_details)
        r=QHBoxLayout()
        self.fire_button=button('Confirm & FIRE puck',self.confirm_fire,'primary')
        self.cancel_button=button('Cancel sequence',self.cancel_sequence,'danger')
        r.addWidget(self.fire_button);r.addWidget(self.cancel_button);r.addStretch();x.addLayout(r)
        r=QHBoxLayout();r.addWidget(label('Adjust yaw at prompt'))
        self.prompt_yaw=QDoubleSpinBox();self.prompt_yaw.setRange(-30,30);self.prompt_yaw.setSuffix('°')
        r.addWidget(self.prompt_yaw)
        self.prompt_yaw_button=button('Move yaw',self.adjust_prompt_yaw)
        r.addWidget(self.prompt_yaw_button);r.addStretch();x.addLayout(r)
        x.addWidget(label('Only Y/R/Z or an empty Enter are safe at the firing prompt; other commands cancel the sequence.','hint',True))
        v.addWidget(p)
        p,x=panel('Completion and recovery','operator verification')
        x.addWidget(label('Current firmware has no definitive G success event. The safe recovery path disconnects and requires human verification, rather than sending a status command into a possible firing prompt.','hint',True))
        self.finish_button=button('Verify G finished and disconnect',self.finish_g)
        x.addWidget(self.finish_button);v.addWidget(p);v.addStretch();return page

    def diagnostics_page(self):
        page,v=self.new_page('Diagnostics','Advanced controls are separated from routine deployment.')
        p,x=panel('Firmware diagnostics','service')
        self.diag_buttons=[
          button('Request status · C',lambda:self.command(Action.STATUS),'primary'),
          button('Lead TMC5160 SPI check · T',lambda:self.command(Action.TEST_SPI)),
          button('Command list · ?',lambda:self.command(Action.HELP))]
        for b in self.diag_buttons:x.addWidget(b)
        x.addWidget(label('115200 baud; newline-terminated ASCII. Log entries display raw device responses.','hint',True))
        v.addWidget(p)
        p,x=panel('Calibration and advanced motion','maintenance')
        b=button('Set current yaw as zero · Z',self.zero_yaw);self.motion_buttons.append(b);x.addWidget(b)
        b=button('Full-travel descent · B',self.full_down);self.motion_buttons.append(b);x.addWidget(b)
        x.addWidget(label('B ignores the tracked bottom position. Burnwire U is intentionally excluded because its 12-second blocking output needs a separate safety workflow.','hint',True))
        v.addWidget(p);v.addStretch();return page

    def switch_page(self,index):
        self.pages.setCurrentIndex(index)
        for i,b in enumerate(self.nav_buttons):
            b.setProperty('active',i==index)
            b.style().unpolish(b);b.style().polish(b)

    def refresh_ports(self):
        previous=self.port.currentData()
        self.port.clear()
        for name,desc in SerialLink.available_ports():
            self.port.addItem(name+' · '+desc,name)
        if previous:
            i=self.port.findData(previous)
            if i>=0:self.port.setCurrentIndex(i)

    def toggle_connection(self):
        if self.link.connected:
            if self.ctrl.tracker.mode and not self.ask('Disconnect during deployment?','The motor may keep moving after USB disconnect.'):
                return
            self.link.disconnect_device()
        elif self.link.connect_device(self.port.currentData() or ''):
            self.log_message('SYSTEM','Port opened; allowing Nano reset before requesting status.')
            QTimer.singleShot(2600,self.initial_status)

    def initial_status(self):
        if self.link.connected and not self.ctrl.tracker.mode:self.command(Action.STATUS)

    def on_connection(self,connected,name):
        self.ctrl.set_connected(connected)
        self.log_message('SYSTEM','Connected to '+name if connected else 'Disconnected. Cached positions cleared.')
        self.refresh_ui()

    def on_fault(self,message):
        self.log_message('FAULT',message);self.refresh_ui()

    def on_line(self,line):
        if not line.strip():return
        self.log_message('DEVICE',line)
        event=self.ctrl.observe(line)
        if event==SequencePhase.FINISHED:
            self.ctrl.ready_for_motion=False
            self.log_message('SYSTEM','Terminal deployment event. Requesting fresh status.')
            self.send(self.ctrl.protocol.encode(Action.STATUS),'status-after-sequence')
        self.refresh_ui()

    def log_message(self,kind,text):
        self.log.appendPlainText(datetime.datetime.now().strftime('%H:%M:%S')+'  ['+kind+']  '+text)

    def send(self,data,desc):
        if not self.link.send(data):return False
        self.log_message('TX',desc+'  '+repr(data));return True

    def command(self,action):
        try:data=self.ctrl.prepare(action)
        except (RuntimeError,ValueError) as exc:self.notice(str(exc));return
        if not self.send(data,action.value):
            if action in (Action.DEPLOY_BARREL,Action.DEPLOY_FMS):self.ctrl.tracker.mark_uncertain()
            self.refresh_ui();return
        if action not in (Action.STATUS,Action.HELP,Action.TEST_SPI,Action.DEPLOY_BARREL,Action.DEPLOY_FMS):
            self.send(self.ctrl.protocol.encode(Action.STATUS),'status-after-motion')
        self.refresh_ui()

    def move_barrel(self):
        try:data=self.ctrl.prepare_barrel_index(self.barrel_index.value())
        except (RuntimeError,ValueError) as exc:self.notice(str(exc));return
        if self.send(data,'barrel index'):
            self.send(self.ctrl.protocol.encode(Action.STATUS),'status-after-motion')

    def move_yaw(self):self.send_yaw(self.yaw_target.value())
    def adjust_prompt_yaw(self):self.send_yaw(self.prompt_yaw.value(),during_prompt=True)
    def send_yaw(self,value,relative=False,during_prompt=False):
        try:data=self.ctrl.prepare_yaw(value,relative,during_prompt)
        except (RuntimeError,ValueError) as exc:self.notice(str(exc));return
        if self.send(data,'yaw relative' if relative else 'yaw absolute'):
            if during_prompt:self.ctrl.tracker.yaw_adjustment_sent()
            else:self.send(self.ctrl.protocol.encode(Action.STATUS),'status-after-motion')
        else:
            if during_prompt:self.ctrl.tracker.mark_uncertain()
        self.refresh_ui()

    def start_deployment(self,action):
        if self.ask('Start deployment?','This moves hardware and launches pucks. Confirm each fire only when the area is clear.'):
            self.command(action)

    def confirm_fire(self):
        if not self.ask('Confirm fire?','A puck will be deployed. Confirm the physical area is clear.'):return
        try:data=self.ctrl.prepare_confirm()
        except RuntimeError as exc:self.notice(str(exc));return
        if not self.send(data,'fire confirmation'):self.ctrl.tracker.mark_uncertain()
        self.refresh_ui()

    def cancel_sequence(self):
        if not self.ask('Cancel deployment?','This cancels the entire run at the firing prompt.'):return
        try:data=self.ctrl.prepare_cancel()
        except RuntimeError as exc:self.notice(str(exc));return
        if not self.send(data,'cancel at prompt'):self.ctrl.tracker.mark_uncertain()
        self.refresh_ui()

    def stop_request(self):
        if not self.link.connected:return
        if self.ctrl.tracker.phase==SequencePhase.AWAIT_FIRE:
            try:data=self.ctrl.prepare_cancel()
            except RuntimeError as exc:self.notice(str(exc));return
            self.send(data,'cancel-at-prompt')
            self.ctrl.stop_latched=True;self.ctrl.ready_for_motion=False
        else:
            try:data=self.ctrl.prepare_stop()
            except RuntimeError as exc:self.notice(str(exc));return
            self.send(data,'interrupt-request')
        self.log_message('SAFETY','Software stop sent; this is NOT a guaranteed physical emergency stop.')
        self.refresh_ui()

    def finish_g(self):
        if self.ctrl.tracker.mode!=Action.DEPLOY_BARREL or self.ctrl.tracker.phase==SequencePhase.AWAIT_FIRE:
            self.notice('Only a non-prompt G run may be closed here.');return
        if not self.ask('Operator verification','Only continue if the elevator returned and all motion is stopped. Disconnecting is not a hardware stop.'):
            return
        self.link.disconnect_device()
        self.notice('Disconnected. Verify hardware, reconnect, and re-establish all references before moving.')

    def home_lead(self):
        if self.ask('Return elevator to bottom?','Without a wired bottom switch the Nano uses a software-tracked reference.'):
            self.command(Action.LEAD_HOME)
    def manual_fire(self):
        if self.ask('Pulse both servos?','This bypasses the G/Q puck-sequencing workflow.'):
            self.command(Action.SERVO_FIRE)
    def zero_yaw(self):
        if self.ask('Set current yaw as zero?','This changes the tracked reference; it does not prove physical alignment.'):
            self.command(Action.YAW_ZERO)
    def full_down(self):
        if self.ask('Full travel descent?','B ignores the tracked bottom reference. Mechanical clearance and a hardware stop are essential.'):
            self.command(Action.LEAD_FULL_DOWN)

    def notice(self,message):
        QMessageBox.information(self,'GLEE Motor Studio',message)
    def ask(self,title,message):
        dlg=QMessageBox(self);dlg.setWindowTitle(title);dlg.setText(title)
        dlg.setInformativeText(message);dlg.setIcon(QMessageBox.Warning)
        dlg.setStandardButtons(QMessageBox.Ok|QMessageBox.Cancel)
        dlg.setDefaultButton(QMessageBox.Cancel)
        return dlg.exec()==QMessageBox.Ok

    def refresh_ui(self):
        connected=self.link.connected
        self.connection_label.setText('● CONNECTED' if connected else '● OFFLINE')
        self.connection_label.setObjectName('connected' if connected else 'disconnected')
        self.connection_label.style().unpolish(self.connection_label)
        self.connection_label.style().polish(self.connection_label)
        self.connect_button.setText('Disconnect' if connected else 'Connect')
        self.port.setEnabled(not connected)
        self.stop_button.setEnabled(connected and not self.ctrl.stop_latched)
        busy=bool(self.ctrl.tracker.mode)
        ready=connected and self.ctrl.ready_for_motion and not busy and not self.ctrl.stop_latched
        for b in self.motion_buttons+self.deployment_buttons:b.setEnabled(ready)
        for b in [self.status_button]+self.diag_buttons:b.setEnabled(connected and not busy)
        prompt=connected and self.ctrl.tracker.phase==SequencePhase.AWAIT_FIRE and not self.ctrl.stop_latched
        for b in (self.fire_button,self.cancel_button,self.prompt_yaw,self.prompt_yaw_button):
            b.setEnabled(prompt)
        self.finish_button.setEnabled(connected and self.ctrl.tracker.mode==Action.DEPLOY_BARREL and not prompt)
        s=self.ctrl.status
        self.metrics[0].setText('—' if s.lead_mm is None else f'{s.lead_mm:.2f} mm')
        self.metrics[1].setText('—' if s.barrel_index is None else f'{s.barrel_index} / {s.barrel_count or 6}')
        self.metrics[2].setText('—' if s.yaw_degrees is None else f'{s.yaw_degrees:+.2f}°')
        self.readiness.setText('Stop requested — verify the mechanism before reconnecting' if self.ctrl.stop_latched else
           'Deployment in progress — normal commands locked' if busy else
           'Connected · status received · controls enabled' if ready else
           'Connected · awaiting status' if connected else 'Offline')
        self.puck_level.setText('Puck level: '+('unknown' if s.puck_level_unknown else '—' if s.puck_level is None else str(s.puck_level)+' / '+str(s.puck_count or 17)))
        self.switches.setText('Switches: bottom '+(s.bottom_switch or 'not reported')+' · puck '+(s.puck_switch or 'not reported'))
        tr=self.ctrl.tracker
        self.sequence_label.setText('Ready to FIRE or Cancel' if prompt else 'Deployment active' if busy else 'No active deployment')
        self.sequence_details.setText(
            ('Confirmation prompts: '+str(tr.prompt_count)+' · approved fires: '+str(tr.confirmed_count)) if busy
            else 'No deployment is running.')
        if s.barrel_count:self.barrel_index.setRange(0,s.barrel_count-1)
        if s.yaw_min is not None and s.yaw_max is not None:
            self.yaw_target.setRange(s.yaw_min,s.yaw_max)
            self.prompt_yaw.setRange(s.yaw_min,s.yaw_max)

def run():
    app=QApplication(sys.argv);app.setApplicationDisplayName('GLEE Motor Studio')
    win=MainWindow();win.show()
    sys.exit(app.exec())
