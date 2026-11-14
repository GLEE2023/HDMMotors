//! Local-only GLEE hardware service.
//! React communicates through Tauri IPC; USB serial is owned by this actor.
//! No HTTP listener, arbitrary serial-write API, or firmware changes are needed.
use serde::Serialize;
use std::{
    io::{Read, Write},
    sync::{atomic::{AtomicU64, Ordering}, mpsc::{self, Receiver, Sender, TryRecvError}, Arc, Mutex},
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};
use tauri::{AppHandle, Emitter, Manager, State};

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct Telemetry {
    lead_mm: Option<f64>, lead_max_mm: Option<f64>, puck_level: Option<i32>,
    puck_count: Option<i32>, barrel_index: Option<i32>, barrel_count: Option<i32>,
    yaw_degrees: Option<f64>, yaw_min: f64, yaw_max: f64,
    bottom_switch: String, puck_switch: String, barrel_solenoid: String,
    updated_at: Option<u64>,
}
impl Default for Telemetry {
    fn default() -> Self {
        Self { lead_mm: None, lead_max_mm: None, puck_level: None, puck_count: None,
            barrel_index: None, barrel_count: None, yaw_degrees: None, yaw_min: -30.0, yaw_max: 30.0,
            bottom_switch: "NOT REPORTED".into(), puck_switch: "NOT REPORTED".into(),
            barrel_solenoid: "NOT REPORTED".into(), updated_at: None }
    }
}
#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct RuntimeState {
    connected: bool, port: Option<String>, phase: String, mode: Option<String>,
    telemetry: Telemetry, prompts: u32, confirmed: u32, device_verified: bool, fault: Option<String>,
}
impl Default for RuntimeState {
    fn default() -> Self {
        Self { connected: false, port: None, phase: "offline".into(), mode: None,
            telemetry: Telemetry::default(), prompts: 0, confirmed: 0, device_verified: false, fault: None }
    }
}
#[derive(Clone, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
enum FrontendEvent { State { state: RuntimeState }, Log { level: &'static str, message: String } }

enum WorkerMessage { Action(String, Option<f64>), Disconnect }
struct Shared { state: Mutex<RuntimeState>, sender: Mutex<Option<Sender<WorkerMessage>>>, generation: AtomicU64 }
impl Default for Shared {
    fn default() -> Self { Self { state: Mutex::new(RuntimeState::default()), sender: Mutex::new(None), generation: AtomicU64::new(0) } }
}
struct Runtime(Arc<Shared>);

fn emit_state(app: &AppHandle, shared: &Shared) {
    if let Ok(guard) = shared.state.lock() { let _ = app.emit("hdm:event", FrontendEvent::State { state: guard.clone() }); }
}
fn emit_log(app: &AppHandle, level: &'static str, message: impl Into<String>) {
    let _ = app.emit("hdm:event", FrontendEvent::Log { level, message: message.into() });
}
fn stamp() -> u64 { SystemTime::now().duration_since(UNIX_EPOCH).unwrap_or_default().as_millis() as u64 }
fn set_state<F: FnOnce(&mut RuntimeState)>(app: &AppHandle, shared: &Shared, f: F) {
    if let Ok(mut s) = shared.state.lock() { f(&mut s); }
    emit_state(app, shared);
}

#[tauri::command]
fn list_ports() -> Result<Vec<String>, String> {
    let mut out: Vec<_> = serialport::available_ports().map_err(|e|e.to_string())?.into_iter().map(|p|p.port_name).collect();
    out.sort(); Ok(out)
}
#[tauri::command]
fn get_state(runtime: State<'_, Runtime>) -> Result<RuntimeState, String> {
    runtime.0.state.lock().map(|s|s.clone()).map_err(|_| "State lock failed".into())
}
#[tauri::command]
fn connect_port(name: String, app: AppHandle, runtime: State<'_, Runtime>) -> Result<(), String> {
    if name.is_empty() || name.len()>255 { return Err("Invalid port name".into()); }
    let shared=runtime.0.clone();
    if shared.state.lock().map_err(|_| "State unavailable")?.connected { return Err("Disconnect before choosing a new port".into()); }
    let port=serialport::new(&name, 115_200).timeout(Duration::from_millis(25)).open().map_err(|e|format!("Could not open {name}: {e}"))?;
    let (tx,rx)=mpsc::channel();
    *shared.sender.lock().map_err(|_|"Serial sender unavailable")?=Some(tx);
    let generation=shared.generation.fetch_add(1,Ordering::SeqCst)+1;
    set_state(&app,&shared,|s| { *s=RuntimeState { connected:true, port:Some(name.clone()), phase:"initializing".into(), ..Default::default() }; });
    emit_log(&app,"system",format!("Serial port opened: {name}. Waiting for Nano startup."));
    thread::spawn(move||serial_worker(app,shared,rx,port,generation));
    Ok(())
}
#[tauri::command]
fn disconnect_port(app: AppHandle, runtime: State<'_, Runtime>) -> Result<(), String> {
    let shared=&runtime.0;
    shared.generation.fetch_add(1,Ordering::SeqCst);
    if let Ok(mut tx)=shared.sender.lock() { if let Some(sender)=tx.take() { let _=sender.send(WorkerMessage::Disconnect); } }
    set_state(&app,shared,|s| *s=RuntimeState::default());
    emit_log(&app,"system","Connection closed. Hardware motion may continue after USB disconnect.");
    Ok(())
}
#[tauri::command]
fn perform_action(action: String, value: Option<f64>, runtime: State<'_, Runtime>) -> Result<(), String> {
    // Explicit action list: no generic raw command execution can cross the IPC boundary.
    const ALLOWED:&[&str]=&["status","help","spi_test","lead_up","lead_down","lead_plus_mm",
        "lead_minus_mm","home","barrel_next","barrel_previous","barrel_index","yaw_absolute",
        "yaw_relative","yaw_zero","servo_reset","servo_arm","servo_fire",
        "deploy_barrel","deploy_fms","confirm_fire","cancel","stop"];
    if !ALLOWED.contains(&action.as_str()) { return Err("Unsupported control action".into()); }
    let shared=&runtime.0;
    if !shared.state.lock().map_err(|_|"State unavailable")?.connected { return Err("Not connected".into()); }
    shared.sender.lock().map_err(|_|"Serial channel unavailable")?.as_ref()
        .ok_or("Serial channel unavailable")?.send(WorkerMessage::Action(action,value))
        .map_err(|_|"Serial actor is not running".into())
}
fn parse_measure(line: &str, prefix: &str) -> Option<f64> {
    let rest=line.strip_prefix(prefix)?.trim().split_whitespace().next()?;
    rest.parse::<f64>().ok().filter(|v|v.is_finite())
}
fn parse_status(s: &mut RuntimeState, line: &str) {
    if let Some(rest)=line.strip_prefix("Lead screw: ").or_else(||line.strip_prefix("Lead position: ")) {
        let numbers:Vec<_>=rest.split(|c:char| !(c.is_ascii_digit()||c=='.'||c=='-')).filter_map(|x|x.parse::<f64>().ok()).collect();
        if numbers.len()>=2 { s.telemetry.lead_mm=Some(numbers[0]);s.telemetry.lead_max_mm=Some(numbers[1]); }
    }
    if let Some(rest)=line.strip_prefix("Lead puck level: ") {
        s.telemetry.puck_level=rest.split_whitespace().next().and_then(|v|v.parse().ok());
        s.telemetry.puck_count=rest.split('/').nth(1).and_then(|v|v.trim().parse().ok());
    }
    if let Some(rest)=line.strip_prefix("Barrel index: ") {
        s.telemetry.barrel_index=rest.split_whitespace().next().and_then(|v|v.parse().ok());
        s.telemetry.barrel_count=rest.split(" of ").nth(1).and_then(|v|v.trim().parse().ok());
    }
    if let Some(v)=parse_measure(line,"Yaw: ") { s.telemetry.yaw_degrees=Some(v); }
    if let Some(rest)=line.strip_prefix("Yaw allowed range: ") {
        if let Some((low,high))=rest.split_once(" to ") {
            if let Ok(v)=low.trim().parse::<f64>(){s.telemetry.yaw_min=v;}
            if let Some(v)=high.trim().trim_start_matches('+').split_whitespace().next().and_then(|x|x.parse::<f64>().ok()){s.telemetry.yaw_max=v;}
        }
    }
    if let Some(v)=line.strip_prefix("Elevator Bottom Switch: ") {s.telemetry.bottom_switch=v.into();}
    if let Some(v)=line.strip_prefix("Puck in Barrel Switch: ") {s.telemetry.puck_switch=v.into();}
    if let Some(v)=line.strip_prefix("Barrel Solenoid: ") {s.telemetry.barrel_solenoid=v.into();}
}
fn finished_motion(line:&str)->bool {
    let lower=line.to_ascii_lowercase();
    ["lead position:","lead puck level:","barrel index movement complete",
     "barrel movement stopped","yaw movement complete","yaw movement stopped","both servos ",
     "current yaw position set","all configured pucks","lead screw is already",
     "lead screw reached the physical bottom","bottom switch did not trip",
     "lead screw stopped early","lead-screw movement stopped","lead-screw movement ended",
     "unable to","configured puck stack","invalid barrel calibration","barrel index must be"]
        .iter().any(|p|lower.starts_with(p))
}
fn write_line(port:&mut dyn serialport::SerialPort, line:&str) -> Result<(),String> {
    port.write_all(line.as_bytes()).map_err(|e|format!("Serial write failed: {e}"))
}
fn dispatch(app:&AppHandle,shared:&Shared,port:&mut dyn serialport::SerialPort,
    action:&str,value:Option<f64>,pending_motion:&mut bool,pending_status:&mut bool)->Result<(),String> {
    let current=shared.state.lock().map_err(|_|"State lock unavailable")?.clone();
    let phase=current.phase.as_str();
    let at_prompt=phase=="await_fire";
    let routine=phase=="ready" && !*pending_motion && !*pending_status;
    let (command, new_phase, new_mode):(String,&str,Option<&str>)=match action {
        "stop" => {if phase=="stop_requested" {return Err("Stop already requested".into());}
            (if at_prompt {"N\n"} else {"x\n"}.into(),"stop_requested",None)}
        "confirm_fire" if at_prompt => ("\n".into(),"deployment",current.mode.as_deref()),
        "cancel" if at_prompt => ("N\n".into(),"deployment",current.mode.as_deref()),
        "status" if routine || phase=="initializing" => ("C\n".into(),"initializing",None),
        "help" if routine => ("?\n".into(),phase,None),
        "spi_test" if routine => ("T\n".into(),phase,None),
        "deploy_barrel" if routine => ("G\n".into(),"deployment",Some("G")),
        "deploy_fms" if routine => ("Q\n".into(),"deployment",Some("Q")),
        "lead_up" if routine => ("W\n".into(),"moving",None),
        "lead_down" if routine => ("S\n".into(),"moving",None),
        "lead_plus_mm" if routine => ("+\n".into(),"moving",None),
        "lead_minus_mm" if routine => ("-\n".into(),"moving",None),
        "home" if routine => ("H\n".into(),"moving",None),
        "barrel_next" if routine => ("N\n".into(),"moving",None),
        "barrel_previous" if routine => ("P\n".into(),"moving",None),
        "servo_reset" if routine => ("w\n".into(),"moving",None),
        "servo_arm" if routine => ("e\n".into(),"moving",None),
        "servo_fire" if routine => ("M\n".into(),"moving",None),
        "yaw_zero" if routine => ("Z\n".into(),"moving",None),
        "barrel_index" if routine => {
            let i=value.ok_or("Barrel index is required")?;
            if !i.is_finite() || i.fract()!=0.0 || i<0.0 || i>=current.telemetry.barrel_count.unwrap_or(6) as f64 {
                return Err("Barrel index outside allowed range".into()); }
            (format!("I{}\n",i as i32),"moving",None)
        }
        "yaw_absolute" if routine => {
            let deg=value.ok_or("Yaw target is required")?;
            if !deg.is_finite() || deg<current.telemetry.yaw_min || deg>current.telemetry.yaw_max {
                return Err("Yaw target outside allowed range".into()); }
            (format!("Y{deg:.3}\n"),"moving",None)
        }
        "yaw_relative" if routine => {
            let delta=value.ok_or("Yaw delta is required")?;
            let start=current.telemetry.yaw_degrees.ok_or("Yaw reference unknown")?;
            if !delta.is_finite() || start+delta<current.telemetry.yaw_min || start+delta>current.telemetry.yaw_max {
                return Err("Relative yaw outside allowed range".into()); }
            (format!("R{delta:.3}\n"),"moving",None)
        }
        _ => return Err("Action blocked by current controller state".into()),
    };
    // The serial interrupt is the only command permitted during motion;
    // never put an ordinary status query in front of x/X in the Nano buffer.
    write_line(port,&command)?;
    if action=="stop" {*pending_motion=false;*pending_status=false;}
    else if action=="status" {*pending_status=true;}
    else if new_phase=="moving" {*pending_motion=true;}
    set_state(app,shared,|s|{
        s.phase=new_phase.into();
        if let Some(mode)=new_mode {s.mode=Some(mode.into());}
        if action=="deploy_barrel"||action=="deploy_fms" {s.prompts=0;s.confirmed=0;}
        if action=="confirm_fire" {s.confirmed+=1;}
        if action=="stop" {s.device_verified=false;}
    });
    emit_log(app,"tx",format!("Action queued: {action}"));
    Ok(())
}
fn received_line(app:&AppHandle,shared:&Shared,line:&str,pending_motion:&mut bool,pending_status:&mut bool,port:&mut dyn serialport::SerialPort){
    let line=line.trim();if line.is_empty(){return;}
    emit_log(app,"rx",line.to_string());
    let lower=line.to_ascii_lowercase();
    let is_prompt=lower.contains("press enter to fire");
    let completed=lower.contains("fms deployment complete") || lower.contains("deployment cancelled")
        || lower.contains("fms stopped") || lower.contains("full deployment stopped")
        || lower.contains("barrel deployment did not complete") || lower.contains("barrel indexing failed")
        || lower.contains("full deployment finished, but");
    set_state(app,shared,|s|{
        parse_status(s,line);
        if is_prompt && s.mode.is_some() {
            if s.phase!="await_fire" {s.prompts+=1;}
            s.phase="await_fire".into();
        }
        if lower.contains("emergency stop") {s.phase="stop_requested".into();s.device_verified=false;}
        if completed {s.phase="initializing".into();s.mode=None;s.device_verified=false;}
        if *pending_status && lower.starts_with("barrel solenoid:") {
            *pending_status=false;
            s.telemetry.updated_at=Some(stamp());
            s.device_verified=true; // A complete STATUS frame, not physical encoder verification.
            if s.mode.is_none() && s.phase!="stop_requested" && s.phase!="uncertain" {s.phase="ready".into();}
        }
    });
    if completed && !*pending_status {
        if write_line(port,"C\n").is_ok(){*pending_status=true;emit_log(app,"system","Deployment terminal event; requesting status.");}
    }
    if *pending_motion && finished_motion(line) && !*pending_status {
        *pending_motion=false;
        if write_line(port,"C\n").is_ok(){*pending_status=true;emit_log(app,"system","Motion ended; requesting status.");}
    }
}
fn serial_worker(app: AppHandle,shared: Arc<Shared>,rx: Receiver<WorkerMessage>,mut port: Box<dyn serialport::SerialPort>,generation:u64){
    let mut pending_status=false;let mut pending_motion=false;
    let mut line:Vec<u8>=Vec::with_capacity(128);
    let mut started=Instant::now();let mut initialized=false;
    loop {
        if shared.generation.load(Ordering::SeqCst)!=generation {break;}
        loop {match rx.try_recv() {
            Ok(WorkerMessage::Disconnect)=>return,
            Ok(WorkerMessage::Action(name,value))=>{
                if let Err(err)=dispatch(&app,&shared,&mut *port,&name,value,&mut pending_motion,&mut pending_status){emit_log(&app,"warning",err);}
            },
            Err(TryRecvError::Empty)=>break,
            Err(TryRecvError::Disconnected)=>return,
        }}
        if !initialized && started.elapsed()>=Duration::from_millis(2500) {
            initialized=true;
            if write_line(&mut *port,"C\n").is_ok(){pending_status=true;emit_log(&app,"system","Requesting initial controller status.");}
        }
        let mut buf=[0u8;512];
        match port.read(&mut buf){
            Ok(n)=>{
                for &b in &buf[..n] {
                    if b==b'\r' {continue;}
                    if b==b'\n' {
                        let text=String::from_utf8_lossy(&line).into_owned();line.clear();
                        received_line(&app,&shared,&text,&mut pending_motion,&mut pending_status,&mut *port);
                    } else if line.len()<1024 {line.push(b);} else {line.clear();emit_log(&app,"warning","Oversized serial line dropped.");}
                }
                started=Instant::now(); // Timer retained only for startup; no periodic status polling.
            }
            Err(e) if e.kind()==std::io::ErrorKind::TimedOut=>{}
            Err(e)=>{
                emit_log(&app,"warning",format!("Serial connection lost: {e}"));
                set_state(&app,&shared,|s|{s.phase="uncertain".into();s.device_verified=false;s.fault=Some(e.to_string());});
                break;
            }
        }
    }
    if shared.generation.load(Ordering::SeqCst)==generation {
        set_state(&app,&shared,|s|{s.connected=false;s.phase="offline".into();s.device_verified=false;});
        if let Ok(mut sender)=shared.sender.lock(){*sender=None;}
    }
}

pub fn run() {
    tauri::Builder::default()
        .manage(Runtime(Arc::new(Shared::default())))
        .invoke_handler(tauri::generate_handler![list_ports,get_state,connect_port,disconnect_port,perform_action])
        .run(tauri::generate_context!())
        .expect("GLEE desktop application failed to start");
}
