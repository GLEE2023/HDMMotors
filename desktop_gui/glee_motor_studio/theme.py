STYLE = """
QMainWindow, QWidget { background:#090F1B; color:#EEF4FB; font-family:"Segoe UI", Arial; font-size:13px; }
QFrame#rail { background:#0C1422; border-right:1px solid #26354A; }
QFrame#panel { background:#111B2C; border:1px solid #26354A; border-radius:13px; }
QFrame#strip { background:#0C1422; border-bottom:1px solid #26354A; }
QLabel#brand { font-size:22px; font-weight:800; color:#E6B65B; }
QLabel#heading { font-size:23px; font-weight:750; }
QLabel#subheading { font-size:16px; font-weight:650; }
QLabel#muted, QLabel#hint { color:#91A1B5; }
QLabel#eyebrow { color:#72C5DB; font-weight:750; font-size:10px; }
QLabel#metric { font-size:30px; font-weight:750; }
QLabel#connected { color:#82D4B1; font-weight:700; }
QLabel#disconnected { color:#F3BA73; font-weight:700; }
QPushButton { background:#203047; border:1px solid #34475F; border-radius:8px; padding:10px 16px; color:#EEF4FB; font-weight:650; min-height:20px; }
QPushButton:hover { background:#2C405A; border-color:#6C8CA9; }
QPushButton:pressed { background:#15273A; }
QPushButton:disabled { background:#172334; color:#66788D; border-color:#26354A; }
QPushButton#primary { background:#E6B65B; color:#17212D; border-color:#E6B65B; }
QPushButton#primary:hover { background:#FFD184; }
QPushButton#danger { background:#B94450; color:white; border-color:#E26B72; }
QPushButton#danger:hover { background:#D65B66; }
QPushButton#nav { border:0; border-radius:8px; background:transparent; text-align:left; padding:13px 16px; font-weight:550; }
QPushButton#nav:hover { background:#203047; }
QPushButton#nav[active="true"] { color:#E6B65B; background:#273047; border-left:3px solid #E6B65B; }
QComboBox, QSpinBox, QDoubleSpinBox { background:#0B1524; border:1px solid #39506A; border-radius:8px; padding:9px 11px; min-height:22px; }
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color:#72C5DB; }
QComboBox QAbstractItemView { background:#172437; color:#F4F7FC; selection-background-color:#344A64; }
QPlainTextEdit { background:#080F1A; color:#BBCBDD; border:1px solid #26354A; border-radius:9px; font-family:Consolas,monospace; font-size:11px; padding:9px; }
QScrollArea { background:transparent; border:0; }
QSlider::groove:horizontal { height:6px; background:#29364B; border-radius:3px; }
QSlider::handle:horizontal { background:#E6B65B; width:18px; margin:-7px 0; border-radius:9px; }
QToolTip { color:white; background:#26354A; padding:5px; }
"""