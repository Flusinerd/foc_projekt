// Variablen der laufenden Anwendung (CPU1) per CCS-21-Scripting lesen (JTAG/XDS100).
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_app_vars.js
// Ablauf wie ccs_read_can_status.js: verbinden (CPU1 steht im Boot-ROM), Symbole aus der .out laden,
// PC = code_start, RUN_SEC laufen lassen (Anwendung startet neu, inkl. ADC-Offsetkalibrierung),
// anhalten, Ausdruecke lesen, weiterlaufen lassen. Achtung: startet die Anwendung neu.
const OUT = "C:/Users/Student/Desktop/foc-framework/foc-framework/matlab/app_speed_ert_rtw/_out/app_speed.out";
const RUN_SEC = 4;
const EXPR = [
  "app_speed_DW.IaOffset", "app_speed_DW.IbOffset",
  "app_speed_DW.GlobalHallState", "app_speed_DW.HallA", "app_speed_DW.HallB", "app_speed_DW.HallC",
  "app_speed_DW.GlobalSpeedValidity",
];
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Program Files/MATLAB/R2023a/toolbox/c2b/tic2000/CCS_Config/f28388D.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
s.symbols.load(OUT);
s.registers.write("PC", s.expressions.evaluate("code_start"));
s.target.run(false);
const t0 = Date.now(); while (Date.now() - t0 < RUN_SEC * 1000) { /* warten */ }
s.target.halt();
console.log("PC = 0x" + BigInt(s.registers.read("PC")).toString(16) + " nach " + RUN_SEC + " s");
for (const e of EXPR) {
  try { console.log(e.padEnd(36) + " = " + s.expressions.evaluate(e)); }
  catch (err) { console.log(e.padEnd(36) + " : " + String(err.message).slice(0, 80)); }
}
s.memory.memMap.enable(false);
const es = s.memory.read(0x48004n, 2, 16).map(Number), errc = s.memory.read(0x48008n, 2, 16).map(Number);
console.log("CAN-A ES=0x" + es[0].toString(16) + " (BOff=" + ((es[0] >> 7) & 1) + ", LEC=" + (es[0] & 7) + ")  TEC=" + (errc[0] & 0xFF) + " REC=" + ((errc[0] >> 8) & 0x7F));
s.target.run(false);
s.target.disconnect();
ds.shutdown();
