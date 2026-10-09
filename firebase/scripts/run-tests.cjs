// ภาพรวม: เปิด Firebase Emulator และรันทดสอบกับ demo-smartline ไม่ใช้ฐานข้อมูลจริง
const {spawnSync}=require("node:child_process");
const {resolve}=require("node:path");
const mode=process.argv[2];
if(!["rules","integration","all"].includes(mode))throw new Error("Choose rules, integration or all");
const files=mode==="all"?"tests/rules.test.cjs tests/integration.test.cjs tests/push.test.cjs":`tests/${mode}.test.cjs`;
const result=spawnSync(process.execPath,[resolve(__dirname,"../node_modules/firebase-tools/lib/bin/firebase.js"),
 "emulators:exec","--project","demo-smartline","--only",mode==="rules"?"database":"auth,database",
 `node --test --test-concurrency=1 ${files}`],{cwd:resolve(__dirname,".."),stdio:"inherit",env:process.env});
process.exit(result.status??1);
