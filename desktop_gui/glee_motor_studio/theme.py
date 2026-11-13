"""GLEE desktop visual system. Qt labels must remain transparent."""
STYLE = """
QMainWindow { background-color:#0C1421; color:#E8EDF5; }
QWidget { color:#E8EDF5; font-family:"Segoe UI"; font-size:13px; }
QWidget#root, QWidget#contentRoot { background-color:#0C1421; }
QFrame#rail { background-color:#111D2C; border-right:1px solid #2B3A4D; }
QFrame#strip { background-color:#111D2C; border-bottom:1px solid #2B3A4D; }
QFrame#panel { background-color:#162334; border:1px solid #2C3C51; border-radius:9px; }
QLabel { background:transparent; border:none; padding:0px; }
QLabel#brand { font-size:20px; font-weight:750; color:#E9BB6A; }
QLabel#appName { font-size:12px; font-weight:600; color:#B5C5D6; }
QLabel#heading { font-size:24px; font-weight:700; color:#F3F6FA; }
QLabel#subheading { font-size:15px; font-weight:650; }
QLabel#muted, QLabel#hint { color:#9AADC2; font-size:12px; }
QLabel#eyebrow { color:#9AB0C8; font-weight:700; font-size:10px; }
QLabel#metric { font-size:29px; font-weight:650; color:#F2F5FA; }
QLabel#connected { color:#85CFAF; font-weight:650; font-size:12px; }
QLabel#disconnected { color:#E9BB6A; font-weight:650; font-size:12px; }
QPushButton { background-color:#233349; border:1px solid #354A60; border-radius:6px; padding:9px 14px; color:#E5ECF5; font-weight:600; min-height:20px; }
QPushButton:hover { background-color:#2F4259; border-color:#5B7692; }
QPushButton:pressed { background-color:#17273A; }
QPushButton:disabled { background-color:#182536; color:#718299; border-color:#29384B; }
QPushButton#primary { background-color:#DEB66E; border-color:#DEB66E; color:#192636; }
QPushButton#primary:hover { background-color:#EFC883; }
QPushButton#danger { background-color:#963D49; color:#FFF; border-color:#B85A66; }
QPushButton#danger:hover { background-color:#B44A58; }
QPushButton#nav { border:0; border-left:2px solid transparent; border-radius:0; background:transparent; text-align:left; padding:11px 14px; font-weight:550; color:#ABBCD0; }
QPushButton#nav:hover { background-color:#1D2E42; }
QPushButton#nav[active="true"] { background-color:#23344B; border-left:2px solid #DEB66E; color:#F0C580; }
QComboBox, QSpinBox, QDoubleSpinBox { background-color:#101D2C; color:#EDF2F9; border:1px solid #40556D; border-radius:6px; padding:8px 10px; min-height:20px; }
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color:#DEB66E; }
QComboBox QAbstractItemView { background-color:#19283B; color:#E8EDF5; selection-background-color:#344A63; }
QPlainTextEdit { background-color:#0D1827; color:#BBCDE2; border:1px solid #2C3E54; border-radius:7px; font-family:Consolas,monospace; font-size:11px; padding:9px; }
QScrollArea { background:transparent; border:0; }
QScrollBar:vertical { background:transparent; width:8px; }
QScrollBar::handle:vertical { background:#405169; border-radius:4px; min-height:25px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:none; }
QToolTip { color:white; background-color:#26374C; border:1px solid #40546C; padding:5px; }
"""
