// Merkbits (COMPHLATCH/COMPLLATCH) von CMPSS3 loeschen, ohne Reset (f28388D_nogel.ccxml), und nachlesen.
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_clear_cmpss3_latch.js
// COMPSTSCLR (CMPSS3_BASE 0x5CC0 + 3): HLATCHCLR Bit 1, LLATCHCLR Bit 9. HSYNCCLREN/LSYNCCLREN
// (Bit 2/10) bleiben 0, wie vom Blockset konfiguriert. Aendert nur die Merkbits, nicht den Trip.
const BASE = 0x5CC0n;
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Users/Student/Desktop/foc-framework/foc-framework/host/tools/ccs/f28388D_nogel.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
const before = Number(s.memory.read(BASE + 2n, 1, 16)[0]);
s.memory.write(BASE + 3n, [0x0202], 16);
const after = Number(s.memory.read(BASE + 2n, 1, 16)[0]);
s.target.run(false);
s.target.disconnect();
ds.shutdown();
const fmt = (v) => "0x" + v.toString(16).toUpperCase().padStart(4, "0") + " COMPHLATCH=" + ((v >> 1) & 1) +
                   " COMPLLATCH=" + ((v >> 9) & 1);
console.log("COMPSTS vorher  " + fmt(before));
console.log("COMPSTS nachher " + fmt(after));
