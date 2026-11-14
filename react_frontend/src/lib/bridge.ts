import { invoke, isTauri } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';
import type { BridgeEvent, ControlAction, RuntimeState } from '../types';
import { offlineState } from '../types';
export const desktop = isTauri();
export async function listPorts():Promise<string[]>{return desktop?invoke<string[]>('list_ports'):[];}
export async function connectPort(port:string):Promise<void>{if(!desktop) throw Error('USB access requires the desktop app');await invoke('connect_port',{name:port});}
export async function disconnectPort():Promise<void>{if(!desktop)return;await invoke('disconnect_port');}
export async function getState():Promise<RuntimeState>{return desktop?invoke<RuntimeState>('get_state'):offlineState;}
export async function action(name:ControlAction,value?:number):Promise<void>{if(!desktop)throw Error('Commands are disabled in browser preview');await invoke('perform_action',{action:name,value:value??null});}
export async function subscribe(handler:(event:BridgeEvent)=>void):Promise<UnlistenFn>{return desktop?listen<BridgeEvent>('hdm:event',e=>handler(e.payload)):async()=>{};}
