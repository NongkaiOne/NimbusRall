// ภาพรวม: กฎตรวจข้อมูล/คำสั่ง/เหตุการณ์ฝนของ Blaze รุ่นเดิม แยกไว้ให้ทดสอบโดยไม่เรียก Firebase
"use strict";
const ACTIONS = new Set(["MEASURE", "EXTEND", "RETRACT", "STOP", "MODE AUTO", "MODE MANUAL"]);
const DEVICE_ID = /^sl-[a-f0-9]{6}$/;
const id = (value) => typeof value === "string" && DEVICE_ID.test(value);
function numeric(value, min, max, nullable = false) {
  if (value === null && nullable) return null;
  if (typeof value !== "number" || !Number.isFinite(value) || value < min || value > max) throw new Error("Invalid numeric field");
  return value;
}
function text(value, max = 100) {
  if (typeof value !== "string" || value.length > max) throw new Error("Invalid text field");
  return value;
}
function telemetry(value) {
  if (!value || typeof value !== "object") throw new Error("Missing telemetry");
  const out = {};
  for (const [key,min,max] of [["temp",-40,85],["hum",0,100],["press",300,1100],["lux",0,200000]]) out[key] = numeric(value[key],min,max,true);
  for (const [key,min,max] of [["p_rain",0,1],["seq",0,4294967294],["uptime",0,4294967295],["t",0,9999999999],["measurement_age_s",0,4294967295],["retract_threshold",0,1],["extend_threshold",0,1],["rssi",-127,0]]) out[key] = numeric(value[key],min,max);
  if (!Number.isInteger(out.seq)) throw new Error("Invalid sequence");
  for (const key of ["sensor_err","fallback","servo_enabled"]) {
    if (typeof value[key] !== "boolean") throw new Error("Invalid flag");
    out[key] = value[key];
  }
  for (const key of ["line","mode","model","fw"]) out[key] = text(value[key]);
  if (!["EXTENDED","RETRACTED","MOVING_OUT","MOVING_IN","UNKNOWN","FAULT_TIMEOUT"].includes(out.line)) throw new Error("Invalid line state");
  if (!["AUTO","MANUAL"].includes(out.mode)) throw new Error("Invalid mode");
  out.fault = value.fault == null ? null : text(value.fault);
  return out;
}
function logRecord(value) {
  if (!value || typeof value !== "object") throw new Error("Invalid log");
  const out = {};
  for (const [key,min,max] of [["seq",1,4294967294],["epoch",0,9999999999],["uptime",0,4294967295],["p_rain",0,1],["flags",0,255]]) out[key]=numeric(value[key],min,max);
  if (!Number.isInteger(out.seq)) throw new Error("Invalid log sequence");
  for (const [key,min,max] of [["temp",-40,85],["hum",0,100],["press",300,1100],["lux",0,200000]]) out[key]=numeric(value[key],min,max,true);
  out.line=text(value.line); return out;
}
function createCommand(action, state, now, commandId) {
  if (!ACTIONS.has(action)) throw new Error("Unsupported command");
  if (!state || !Number.isFinite(state.receivedAt) || now-state.receivedAt > 30000 || now<state.receivedAt) throw new Error("Device is offline");
  if (["EXTEND","RETRACT","STOP","MODE AUTO","MODE MANUAL"].includes(action) && state.servo_enabled!==true) throw new Error("Servo is disabled in firmware");
  return {id:commandId,action,createdAt:now,expiresAt:now+30000,status:"pending"};
}
function rainCandidate(before, after) {
  if (!after || after.sensor_err || !Number.isFinite(after.p_rain) || after.seq<1 || after.measurement_age_s>300) return false;
  if (before && after.seq<=before.seq) return false;
  const high=after.p_rain>=after.retract_threshold;
  const wasHigh=before && !before.sensor_err && Number.isFinite(before.p_rain) && before.p_rain>=before.retract_threshold;
  return high && !wasHigh;
}
function canAcknowledge(control, ack) {
  return !!control && !!ack && control.id===ack.id && typeof ack.ok==="boolean" && typeof ack.msg==="string" && ack.msg.length<=200;
}
module.exports={ACTIONS,id,telemetry,logRecord,createCommand,rainCandidate,canAcknowledge};
