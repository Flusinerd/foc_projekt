// Hall-Zustaende der laufenden Anwendung (CPU1) ueber JTAG beobachten, waehrend die Welle von Hand gedreht wird.
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_watch_hall.js
// Ablauf wie ccs_read_app_vars.js (Neustart ab code_start, inkl. ADC-Offsetkalibrierung), danach WATCH_SEC lang
// alle STEP_MS: kurz anhalten, Hall-Variablen lesen, weiterlaufen lassen. Nur Aenderungen werden ausgegeben.
// Gueltig sind die Zustaende 1..6; 0 und 7 = Sensor fehlt/Versorgung/Verdrahtung.
const OUT = "C:/Users/Student/Desktop/foc-framework/foc-framework/matlab/app_speed_ert_rtw/_out/app_speed.out";
const WATCH_SEC = 30, STEP_MS = 200;
const EXPR = ["app_speed_DW.GlobalHallState", "app_speed_DW.HallA", "app_speed_DW.HallB", "app_speed_DW.HallC",
              "app_speed_DW.GlobalSpeedValidity"];
const ds = initScripting();
ds.setScriptingTimeout(WATCH_SEC * 1000 + 30000);
ds.configure("C:/Program Files/MATLAB/R2023a/toolbox/c2b/tic2000/CCS_Config/f28388D.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
s.symbols.load(OUT);
s.registers.write("PC", s.expressions.evaluate("code_start"));
s.target.run(false);
const wait = (ms) => { const t = Date.now(); while (Date.now() - t < ms) { /* warten */ } };
wait(3000);
console.log("Jetzt die Welle langsam von Hand drehen (" + WATCH_SEC + " s) ...");
const seen = {};
let last = "";
const t0 = Date.now();
while (Date.now() - t0 < WATCH_SEC * 1000) {
  s.target.halt();
  const v = EXPR.map((e) => { try { return String(s.expressions.evaluate(e)); } catch (err) { return "?"; } });
  s.target.run(false);
  const line = "State=" + v[0] + "  A/B/C=" + v[1] + "/" + v[2] + "/" + v[3] + "  valid=" + v[4];
  if (line !== last) { console.log(((Date.now() - t0) / 1000).toFixed(1).padStart(5) + " s  " + line); last = line; }
  seen[v[0]] = (seen[v[0]] || 0) + 1;
  wait(STEP_MS);
}
console.log("Gesehene Zustaende: " + Object.keys(seen).sort().join(", ") + "  (erwartet: alle sechs von 1..6)");
s.target.disconnect();
ds.shutdown();
