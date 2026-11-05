// Wie ccs_read_pc.js, startet die Anwendung aber vorher ab code_start (Anwendung laeuft neu an).
const START = true;
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_pc.js
// Liest N Proben: anhalten, PC/RPC/SP/IER/IFR lesen, 20 ms weiterlaufen lassen. Die Adressen
// werden ausserhalb mit app_speed.map den Funktionen zugeordnet.
const N = 8;
const OUT = "C:/Users/Student/Desktop/foc-framework/foc-framework/matlab/app_speed_ert_rtw/_out/app_speed.out";
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Program Files/MATLAB/R2023a/toolbox/c2b/tic2000/CCS_Config/f28388D.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
const hex = (v) => "0x" + BigInt(v).toString(16);
const wait = (ms) => { const t = Date.now(); while (Date.now() - t < ms) { /* warten */ } };
console.log("beim Verbinden angehalten: " + s.target.isHalted());
if (typeof START !== "undefined" && START) {
  if (!s.target.isHalted()) s.target.halt();
  s.symbols.load(OUT);
  s.registers.write("PC", s.expressions.evaluate("code_start"));
  s.target.run(false);
  wait(2000);
  console.log("Anwendung ab code_start gestartet, 2 s gelaufen");
}
for (let i = 0; i < N; ++i) {
  if (!s.target.isHalted()) s.target.halt();
  const r = ["PC", "RPC", "SP", "IER", "IFR", "ST1"].map((n) => { try { return n + "=" + hex(s.registers.read(n)); } catch (e) { return n + "=?"; } });
  console.log(r.join("  "));
  s.target.run(false);
  wait(20);
}
s.target.halt();
s.memory.memMap.enable(false);
const es = s.memory.read(0x48004n, 2, 16).map(Number), errc = s.memory.read(0x48008n, 2, 16).map(Number);
console.log("CAN-A ES=0x" + es[0].toString(16) + " TEC=" + (errc[0] & 0xFF) + " REC=" + ((errc[0] >> 8) & 0x7F));
const pie = s.memory.read(0xCE0n, 1, 16).map(Number);
console.log("PIECTRL=0x" + pie[0].toString(16));
s.target.run(false);
s.target.disconnect();
ds.shutdown();
