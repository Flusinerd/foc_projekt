// CAN-A-Status der F2838x-CPU1 per CCS-21-Scripting lesen (JTAG/XDS100).
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_can_status.js
// Ablauf: verbinden (GEL setzt CPU1 in den Boot-ROM), Symbole aus foc_can_probe.out laden, PC = code_start,
// 4 s laufen lassen, anhalten, CAN-A-Register (CTL/ES/ERRC/BTR) lesen, weiterlaufen lassen. Unter dem Debugger
// springt der Boot-ROM nicht von selbst in den Flash (Emulation Boot). GEL-Speicherkarte sperrt CAN-A -> memMap aus.
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Program Files/MATLAB/R2023a/toolbox/c2b/tic2000/CCS_Config/f28388D.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
let wasHalted = s.target.isHalted ? s.target.isHalted() : false;
console.log("Nach connect angehalten: " + wasHalted + ", PC = 0x" + BigInt(s.registers.read("PC")).toString(16));
// Anwendung laufen lassen (Boot aus Flash, CAN-Init, Senden), dann anhalten und lesen
const runSec = 4;
// Unter dem Debugger bleibt der Boot-ROM stehen (Emulation Boot) -> direkt in die Anwendung springen
s.symbols.load("C:/Users/Student/Desktop/foc-framework/foc-framework/matlab/foc_can_probe_ert_rtw/_out/foc_can_probe.out");
const entry = s.expressions.evaluate("code_start");
console.log("code_start = 0x" + BigInt(entry).toString(16));
s.registers.write("PC", entry);
s.target.run(false);
const t0 = Date.now(); while (Date.now() - t0 < runSec * 1000) { /* warten */ }
s.target.halt();
wasHalted = false;
function hex(v, w) { return "0x" + BigInt(v).toString(16).toUpperCase().padStart(w, "0"); }
function dump(base, count, label) {
  const v = s.memory.read(BigInt(base), count, 16);
  console.log(`--- ${label} (${hex(base, 5)}, ${count} x 16 bit)`);
  for (let i = 0; i < count; i += 8) {
    console.log(hex(base + i, 5) + ": " + v.slice(i, i + 8).map(x => hex(x, 4)).join(" "));
  }
  return v.map(x => Number(x));
}
s.memory.memMap.enable(false);   // GEL-Speicherkarte sperrt CAN-A, nur fuer das Lesen abschalten
const pc = s.registers.read("PC");
console.log("PC = " + hex(pc, 6) + (wasHalted ? " (war schon angehalten)" : " (angehalten fuer das Lesen)"));
const r = dump(0x48000, 0x20, "CAN-A Steuer-/Statusregister");
const q = dump(0x48080, 0x50, "CAN-A TXRQ/NDAT/IPEN/MVAL");
// Kerne/Takt/Zuordnung: PCLKCR10 (CpuSysRegs 0x5D300 + 0x3A), CPUSEL8 (DevCfgRegs 0x5D000 + 0x?) nur roh
try { dump(0x5D300 + 0x22, 0x20, "CpuSysRegs PCLKCRx (ab 0x5D322)"); } catch (e) { console.log("PCLKCR nicht lesbar: " + e.message.slice(0, 120)); }
for (const ex of ["foc_can_probe_DW.Output_DSTATE", "foc_can_probe_M->Timing.clockTick1", "foc_can_probe_B.Rx_1F1_o2[0]"]) {
  try { console.log(ex + " = " + s.expressions.evaluate(ex)); } catch (e) { console.log(ex + ": " + e.message.slice(0, 80)); }
}
// Interpretation fuer beide moeglichen Offset-Einheiten (Wort/Byte)
function word32(arr, off) { return (arr[off] | (arr[off + 1] << 16)) >>> 0; }
for (const unit of [1, 0.5]) {
  const o = (b) => Math.round(b * unit);
  const ctl = word32(r, o(0x0)), es = word32(r, o(0x4)), errc = word32(r, o(0x8)), btr = word32(r, o(0xC));
  console.log(`\n[Offsets als ${unit === 1 ? "16-bit-Worte" : "Bytes"}] CTL=${hex(ctl, 8)} ES=${hex(es, 8)} ERRC=${hex(errc, 8)} BTR=${hex(btr, 8)}`);
  console.log(`  CTL: Init=${ctl & 1} IE0=${(ctl >> 1) & 1} DAR=${(ctl >> 5) & 1} CCE=${(ctl >> 6) & 1} Test=${(ctl >> 7) & 1} ABO=${(ctl >> 9) & 1}`);
  console.log(`  ES : LEC=${es & 7} TxOk=${(es >> 3) & 1} RxOk=${(es >> 4) & 1} EPass=${(es >> 5) & 1} EWarn=${(es >> 6) & 1} BOff=${(es >> 7) & 1} PER=${(es >> 8) & 1}`);
  console.log(`  ERRC: TEC=${errc & 0xFF} REC=${(errc >> 8) & 0x7F} RP=${(errc >> 15) & 1}`);
  console.log(`  BTR: BRP=${(btr & 0x3F) + (((btr >> 16) & 0xF) << 6)} SJW=${(btr >> 6) & 3} TSEG1=${(btr >> 8) & 0xF} TSEG2=${(btr >> 12) & 7}`);
}
s.target.run(false);
console.log("\nCPU1 laeuft wieder: " + !(s.target.isHalted ? s.target.isHalted() : false));
s.target.disconnect();
ds.shutdown();
