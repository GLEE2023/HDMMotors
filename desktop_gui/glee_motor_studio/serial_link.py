from PySide6.QtCore import QObject, Signal
from PySide6.QtSerialPort import QSerialPort, QSerialPortInfo
class SerialLink(QObject):
    line_received=Signal(str);connection_changed=Signal(bool,str);fault=Signal(str)
    def __init__(self,parent=None):
        super().__init__(parent)
        self.port=QSerialPort(self);self.port.setBaudRate(115200)
        self.port.readyRead.connect(self._read);self.port.errorOccurred.connect(self._error)
        self.buffer=bytearray()
    @staticmethod
    def available_ports():
        return [(p.portName(),p.description() or 'USB serial device') for p in QSerialPortInfo.availablePorts()]
    @property
    def connected(self):return self.port.isOpen()
    def connect_device(self,name):
        self.disconnect_device()
        if not name:self.fault.emit('Select a serial port');return False
        self.port.setPortName(name)
        if not self.port.open(QSerialPort.ReadWrite):
            self.fault.emit(self.port.errorString());return False
        self.port.setDataTerminalReady(True);self.buffer.clear()
        self.connection_changed.emit(True,name);return True
    def disconnect_device(self):
        was=self.port.isOpen()
        if was:self.port.close()
        self.buffer.clear()
        if was:self.connection_changed.emit(False,'')
    def send(self,data):
        if not self.connected:self.fault.emit('Device is not connected');return False
        if self.port.write(data)!=len(data):
            self.fault.emit('Failed to queue serial command');return False
        return True
    def _read(self):
        self.buffer.extend(bytes(self.port.readAll()))
        if len(self.buffer)>8192:
            self.buffer.clear();self.fault.emit('Serial input line too long');return
        while b'\n' in self.buffer:
            raw,_,rest=self.buffer.partition(b'\n');self.buffer=bytearray(rest)
            self.line_received.emit(raw.rstrip(b'\r').decode('utf-8',errors='replace'))
    def _error(self,e):
        if e==QSerialPort.NoError:return
        msg=self.port.errorString()
        if e==QSerialPort.ResourceError:self.disconnect_device()
        self.fault.emit(msg)
