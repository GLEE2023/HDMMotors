import { Suspense, useEffect, useMemo, useRef, useState } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, useGLTF, Html } from '@react-three/drei';
import * as THREE from 'three';
import { RotateCcw, Expand, Eye, EyeOff } from 'lucide-react';
import { MetalFx } from 'metal-fx';
import type { RuntimeState } from '../types';

function PhysicalMechanism({exploded,selected,preview}: {exploded:boolean; selected:string; preview:boolean}){
  const {scene}=useGLTF('/models/glee-mechanism.glb');
  const upper=useRef<THREE.Group>(null);
  const motor=useRef<THREE.Group>(null);
  const objects=useMemo(()=>{
    const base:THREE.Object3D[]=[];const top:THREE.Object3D[]=[];
    scene.traverse(obj=>{
      if(!(obj instanceof THREE.Mesh))return;
      const clone=obj.clone();clone.geometry=obj.geometry;
      const isTop=obj.name.startsWith('upper_');
      clone.material=new THREE.MeshStandardMaterial({color: isTop?'#b9c6d7':'#788ca8',metalness:.62,roughness:.34,flatShading:false,side:THREE.DoubleSide});
      (isTop?top:base).push(clone);
    });return {base,top};
  },[scene]);
  useEffect(()=>{
    for(const [bucket,items] of Object.entries(objects)){
      const focused=selected==='all'||(selected==='upper'?bucket==='top':bucket==='base');
      for(const item of items){const mesh=item as THREE.Mesh;const material=mesh.material as THREE.MeshStandardMaterial;material.transparent=!focused;material.opacity=focused?1:.15;material.depthWrite=focused;}
    }
  },[objects,selected]);
  useFrame(({clock},delta)=>{
    if(!upper.current || !motor.current)return;
    const t=clock.elapsedTime;
    upper.current.position.y=THREE.MathUtils.damp(upper.current.position.y, exploded?38:0,4,delta);
    upper.current.rotation.y=THREE.MathUtils.damp(upper.current.rotation.y,preview?Math.sin(t*.45)*.16:0,2,delta);
  });
  return <group position={[0,-155,0]} scale={.0085}>
    <group ref={motor}>{objects.base.map((o,i)=><primitive key={i} object={o} />)}</group>
    <group ref={upper}>{objects.top.map((o,i)=><primitive key={i} object={o} />)}</group>
    {selected!=='all' && <mesh position={[0,160,0]}><cylinderGeometry args={[116,116,5,48]}/><meshBasicMaterial color="#2c69ff" transparent opacity={.08} depthWrite={false}/></mesh>}
  </group>;
}
function MotionEnvelope({visible}:{visible:boolean}){
  if(!visible)return null;
  return <group>
    <mesh rotation={[-Math.PI/2,0,0]} position={[0,-1.58,0]}><ringGeometry args={[.88,.905,100]}/><meshBasicMaterial color="#54c9ff" transparent opacity={.35} side={THREE.DoubleSide}/></mesh>
    <mesh rotation={[-Math.PI/2,0,0]} position={[0,1.53,0]}><ringGeometry args={[.7,.704,100]}/><meshBasicMaterial color="#507dff" transparent opacity={.65} side={THREE.DoubleSide}/></mesh>
    <gridHelper args={[5.5,28,'#365a9b','#1b2d53']} position={[0,-1.6,0]}/>
    <axesHelper args={[.4]} position={[0,-1.5,0]}/>
  </group>;
}
function Scene({exploded,selected,preview,labels}:{exploded:boolean;selected:string;preview:boolean;labels:boolean}){
  const controls=useRef<any>(null);
  return <>
    <color attach="background" args={['#08101e']}/>
    <ambientLight intensity={1.6}/>
    <hemisphereLight intensity={2} color="#cbd9ff" groundColor="#0b1838"/>
    <directionalLight intensity={3} position={[4,7,5]} color="#d9e7ff"/>
    <directionalLight intensity={1.6} position={[-5,2,-4]} color="#3b72ff"/>
    <spotLight position={[3,10,4]} intensity={50} angle={.6} penumbra={.6} color="#b4e6ff"/>
    <Suspense fallback={<Html center><span className="model-loading">Loading CAD geometry…</span></Html>}>
      <PhysicalMechanism exploded={exploded} selected={selected} preview={preview}/>
    </Suspense>
    <MotionEnvelope visible={labels}/>
    <OrbitControls ref={controls} makeDefault enablePan={false} minDistance={2.35} maxDistance={8.8} minPolarAngle={.2} maxPolarAngle={Math.PI*.84} enableDamping dampingFactor={.075} target={[0,.12,0]} />
  </>;
}
export function AssemblyView({state}:{state:RuntimeState}){
  const [exploded,setExploded]=useState(false);
  const [preview,setPreview]=useState(false);
  const [labels,setLabels]=useState(true);
  const [selected,setSelected]=useState('all');
  return <section className="viewport surface" aria-label="CAD assembly viewer">
    <div className="viewport-shine" aria-hidden="true"/>
    <div className="viewport-meta"><span className="overline">HDM SYSTEM / 01</span><h2>Assembly visualization</h2><span className="caption">Original CAD · walls excluded · engineering preview</span></div>
    <div className="view-status"><span className="cyan-dot"/> CAD MODEL <span className="sep">/</span> REV. REFERENCE</div>
    <Canvas dpr={[1,1.7]} frameloop={exploded||preview?'always':'demand'} camera={{fov:36,position:[4.3,3.05,4.55],near:.1,far:40}} gl={{antialias:true,powerPreference:'high-performance'}}>
      <Scene exploded={exploded} selected={selected} preview={preview} labels={labels}/>
    </Canvas>
    <div className="viewport-side-label left">YAW CONTROL <span>±30°</span></div>
    <div className="viewport-side-label right">ELEVATOR <span>TRAVEL</span></div>
    <div className="view-selector" role="group" aria-label="Inspect model components">
      {[['all','ALL'],['motor','BASE'],['upper','UPPER']].map(([key,text])=><button key={key} className={selected===key?'active':''} onClick={()=>setSelected(key)}>{text}</button>)}
    </div>
    <div className="view-actions">
      <MetalFx preset="silver" strength={.35} paused={!exploded}><button title="Toggle exploded assembly view" className={exploded?'active':''} onClick={()=>setExploded(v=>!v)}><Expand size={15}/> Explode</button></MetalFx>
      <button title="Illustrative animation, not motor feedback" className={preview?'active':''} onClick={()=>setPreview(v=>!v)}><RotateCcw size={15}/> Preview motion</button>
      <button title="Toggle CAD guide overlays" onClick={()=>setLabels(v=>!v)}>{labels?<Eye size={15}/>:<EyeOff size={15}/>} Guides</button>
    </div>
    <div className="viewport-footer"><span className="mono">SOURCE CAD UNITS: MM · VIEW SCALED</span><span>{preview?'ILLUSTRATIVE ANIMATION ONLY':state.connected?'LIVE HARDWARE STATE SHOWN IN TELEMETRY':'NO LIVE HARDWARE DATA'}</span></div>
  </section>;
}
