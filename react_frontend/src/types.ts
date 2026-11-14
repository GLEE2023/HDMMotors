export type Page = 'overview' | 'control' | 'sequence' | 'diagnostics';
export type Phase = 'offline' | 'initializing' | 'ready' | 'moving' | 'await_fire' | 'deployment' | 'stop_requested' | 'uncertain';
export interface Telemetry {
  leadMm: number | null;
  leadMaxMm: number | null;
  puckLevel: number | null;
  puckCount: number | null;
  barrelIndex: number | null;
  barrelCount: number | null;
  yawDegrees: number | null;
  yawMin: number;
  yawMax: number;
  bottomSwitch: string;
  puckSwitch: string;
  barrelSolenoid: string;
  updatedAt: number | null;
}
export interface RuntimeState { connected:boolean; port:string|null; phase:Phase; mode:'G'|'Q'|null; telemetry:Telemetry; prompts:number; confirmed:number; deviceVerified:boolean; fault:string|null; }
export type BridgeEvent={type:'state';state:RuntimeState}|{type:'log';level:'system'|'tx'|'rx'|'warning';message:string};
export type ControlAction = 'status' | 'help' | 'spi_test' | 'lead_up' | 'lead_down' | 'lead_plus_mm' | 'lead_minus_mm' | 'home' | 'barrel_next' | 'barrel_previous' | 'barrel_index' | 'yaw_absolute' | 'yaw_relative' | 'servo_reset' | 'servo_arm' | 'servo_fire' | 'yaw_zero' | 'deploy_barrel' | 'deploy_fms' | 'confirm_fire' | 'cancel' | 'stop';
export const emptyTelemetry: Telemetry={leadMm:null,leadMaxMm:null,puckLevel:null,puckCount:null,barrelIndex:null,barrelCount:null,yawDegrees:null,yawMin:-30,yawMax:30,bottomSwitch:'NOT REPORTED',puckSwitch:'NOT REPORTED',barrelSolenoid:'NOT REPORTED',updatedAt:null};
export const offlineState: RuntimeState={connected:false,port:null,phase:'offline',mode:null,telemetry:emptyTelemetry,prompts:0,confirmed:0,deviceVerified:false,fault:null};
