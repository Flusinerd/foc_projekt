// IPC-Steuerworte CPU1<->CM ohne Reset lesen (Fehlersuche MW_IPC_Write-Warteschleife).
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_ipc_noreset.js
// Kanal k: Basis + 12*k Worte (8 Steuerworte + 2 Zeiger), Wort 3 = mutex. CM->CPU1 ab 0x38000
// (schreibt nur der CM), CPU1->CM ab 0x39000 (schreibt CPU1).
const CCXML = "C:/Users/Student/Desktop/foc-framework/foc-framework/host/tools/ccs/f28388D_nogel.ccxml";
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure(CCXML);
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
console.log("PC=0x" + BigInt(s.registers.read("PC")).toString(16));
const names = ["head", "tail", "full", "mutex", "overwr", "lastRd", "bufLen", "dtBytes"];
for (const [label, base] of [["CM->CPU1", 0x38000n], ["CPU1->CM", 0x39000n]]) {
  console.log("--- " + label + " ---");
  for (let k = 0n; k < 8n; ++k) {
    const w = s.memory.read(base + 12n * k, 8, 16).map(Number);
    console.log("Kanal " + k + ": " + w.map((v, i) => names[i] + "=0x" + v.toString(16)).join(" "));
  }
}
s.target.run(false);
s.target.disconnect();
ds.shutdown();
