// CPU1 ohne GEL-Datei verbinden (kein GEL_Reset beim Verbinden) und den PC der laufenden bzw.
// haengenden Anwendung lesen. Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_pc_noreset.js
// Die Standard-Konfiguration f28388D.ccxml ruft in OnTargetConnect GEL_Reset() auf und loescht
// damit den Zustand; host/tools/ccs/f28388D_nogel.ccxml ist dieselbe Konfiguration ohne GEL-Dateien.
const N = 8;
const CCXML = "C:/Users/Student/Desktop/foc-framework/foc-framework/host/tools/ccs/f28388D_nogel.ccxml";
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure(CCXML);
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
const hex = (v) => "0x" + BigInt(v).toString(16);
const wait = (ms) => { const t = Date.now(); while (Date.now() - t < ms) { /* warten */ } };
console.log("beim Verbinden angehalten: " + s.target.isHalted());
for (let i = 0; i < N; ++i) {
  if (!s.target.isHalted()) s.target.halt();
  const r = ["PC", "RPC", "SP", "IER", "IFR", "ST1"].map((n) => { try { return n + "=" + hex(s.registers.read(n)); } catch (e) { return n + "=?"; } });
  console.log(r.join("  "));
  s.target.run(false);
  wait(20);
}
s.target.disconnect();
ds.shutdown();
