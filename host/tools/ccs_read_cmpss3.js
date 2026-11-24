// CMPSS3-Register der laufenden Anwendung lesen, ohne Reset (f28388D_nogel.ccxml), CPU nur kurz angehalten.
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_cmpss3.js
// CMPSS3_BASE 0x5CC0 (F2838x, hw_memmap.h). COMPSTS (+2): COMPHSTS Bit 0, COMPHLATCH Bit 1,
// COMPLSTS Bit 8, COMPLLATCH Bit 9. Die Latch-Bits halten, ob der gefilterte Komparatorausgang
// seit dem letzten Loeschen (COMPSTSCLR) einmal aktiv war -> zeigt, welche Seite ausgeloest hat.
const BASE = 0x5CC0n;
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Users/Student/Desktop/foc-framework/foc-framework/host/tools/ccs/f28388D_nogel.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
const w = s.memory.read(BASE, 32, 16).map(Number);
s.target.run(false);
s.target.disconnect();
ds.shutdown();
const hex = (v) => "0x" + v.toString(16).toUpperCase().padStart(4, "0");
const sts = w[2];
console.log("COMPCTL   " + hex(w[0]));
console.log("COMPSTS   " + hex(sts) + "  COMPHSTS=" + (sts & 1) + " COMPHLATCH=" + ((sts >> 1) & 1) +
            " COMPLSTS=" + ((sts >> 8) & 1) + " COMPLLATCH=" + ((sts >> 9) & 1));
console.log("COMPDACCTL " + hex(w[4]));
console.log("DACHVALS  " + w[6] + "   DACHVALA " + w[7] + "   DACLVALS " + w[0x12] + "   DACLVALA " + w[0x13]);
console.log("CTRIPHFILCTL " + hex(w[0x18]) + "  CTRIPHFILCLKCTL " + hex(w[0x19]) +
            "  CTRIPLFILCTL " + hex(w[0x16]) + "  CTRIPLFILCLKCTL " + hex(w[0x17]) + "  COMPLOCK " + hex(w[0x1A]));
