// CAN-A-Register der laufenden Anwendung lesen, ohne Reset (f28388D_nogel.ccxml), CPU nur kurz angehalten.
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_can_noreset.js
// CANA_BASE 0x48000 (F2838x): CTL +0, ES +4, ERRC +8, BTR +0xC (je 32 Bit, Adressen in 16-Bit-Worten).
// CTL.Init (Bit 0) = 1: Modul im Init (z. B. nach Bus-Off ohne automatische Wiederaufnahme).
// ES: LEC Bit 0..2, TxOk 3, RxOk 4, EPass 5, EWarn 6, BOff 7, PER 8. ERRC: TEC 0..7, REC 8..14, RP 15.
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Users/Student/Desktop/foc-framework/foc-framework/host/tools/ccs/f28388D_nogel.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
const w = s.memory.read(0x48000n, 16, 16);
s.target.run(false);
s.target.disconnect();
ds.shutdown();
const r32 = (i) => (Number(w[i + 1]) << 16 | Number(w[i])) >>> 0;
const ctl = r32(0), es = r32(4), errc = r32(8), btr = r32(12);
const hex = (v) => "0x" + v.toString(16).toUpperCase().padStart(8, "0");
console.log("CAN_CTL  " + hex(ctl) + "  Init=" + (ctl & 1) + " ABO=" + ((ctl >> 9) & 1));
console.log("CAN_ES   " + hex(es) + "  LEC=" + (es & 7) + " TxOk=" + ((es >> 3) & 1) + " RxOk=" + ((es >> 4) & 1) +
            " EPass=" + ((es >> 5) & 1) + " EWarn=" + ((es >> 6) & 1) + " BOff=" + ((es >> 7) & 1));
console.log("CAN_ERRC " + hex(errc) + "  TEC=" + (errc & 0xFF) + " REC=" + ((errc >> 8) & 0x7F));
console.log("CAN_BTR  " + hex(btr));
