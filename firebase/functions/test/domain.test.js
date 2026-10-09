const {test}=require("node:test");
const assert=require("node:assert/strict");
const d=require("../domain");
const state={receivedAt:1000,servo_enabled:true,seq:1,p_rain:.8,retract_threshold:.5,measurement_age_s:0,sensor_err:false};
test("manual commands reject stale devices and disabled servo",()=>{
 assert.throws(()=>d.createCommand("EXTEND",{...state,servo_enabled:false},2000,"id"));
 assert.throws(()=>d.createCommand("EXTEND",state,32000,"id"));
 assert.equal(d.createCommand("MEASURE",{...state,servo_enabled:false},2000,"id").expiresAt,32000);
 assert.throws(()=>d.createCommand("DELLOG",state,2000,"id"));
});
test("rain alerts require a fresh threshold crossing, not heartbeat, bad sensor or stale sample",()=>{
 assert.equal(d.rainCandidate(null,state),true);
 assert.equal(d.rainCandidate(state,state),false);
 assert.equal(d.rainCandidate(state,{...state,seq:2}),false);
 assert.equal(d.rainCandidate({...state,p_rain:.1},{...state,seq:2}),true);
 assert.equal(d.rainCandidate(null,{...state,sensor_err:true}),false);
 assert.equal(d.rainCandidate(null,{...state,measurement_age_s:301}),false);
 assert.equal(d.rainCandidate({...state,seq:10},{...state,seq:9}),false);
});
test("acknowledgements cannot complete another command",()=>{
 assert.equal(d.canAcknowledge({id:"a"},{id:"b",ok:true,msg:"ok"}),false);
 assert.equal(d.canAcknowledge({id:"a"},{id:"a",ok:true,msg:"ok"}),true);
});
test("log validation rejects nonfinite and out-of-range readings",()=>{
 const log={seq:1,epoch:0,uptime:5,p_rain:.5,flags:0,temp:30,hum:80,press:1000,lux:20,line:"UNKNOWN"};
 assert.equal(d.logRecord(log).seq,1);
 assert.throws(()=>d.logRecord({...log,temp:Infinity}));
 assert.throws(()=>d.logRecord({...log,hum:101}));
 assert.throws(()=>d.logRecord({...log,seq:1.5}));
});

test("commands reject telemetry without a valid server timestamp",()=>{
 for(const receivedAt of [undefined,null,NaN,"1000"]) assert.throws(()=>d.createCommand("MEASURE",{...state,receivedAt},2000,"id"));
});
