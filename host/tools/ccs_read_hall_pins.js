// Hall-Pins GPIO24/25/26 (A/B/C) direkt aus GPADAT lesen (unabhaengig von der Firmware-Logik).
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_hall_pins.js
// Haelt CPU1 nur kurz pro Lesung an; die Anwendung laeuft dazwischen weiter (kein Neustart).
const N = 10, STEP_MS = 500;
const GPADAT = 0x7F00n;   // GpioDataRegs.GPADAT (F2838x, 32 bit)
const ds = initScripting();
ds.setScriptingTimeout(60000);
ds.configure("C:/Program Files/MATLAB/R2023a/toolbox/c2b/tic2000/CCS_Config/f28388D.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
s.memory.memMap.enable(false);
const wait = (ms) => { const t = Date.now(); while (Date.now() - t < ms) { /* warten */ } };
for (let i = 0; i < N; i++) {
  const halted = s.target.isHalted();
  if (!halted) s.target.halt();
  const w = s.memory.read(GPADAT, 2, 16).map(Number);
  s.target.run(false);
  const v = (w[1] << 16) | w[0];
  const a = (v >> 24) & 1, b = (v >> 25) & 1, c = (v >> 26) & 1;
  console.log("GPADAT=0x" + (v >>> 0).toString(16).padStart(8, "0") + "  Hall A/B/C = " + a + "/" + b + "/" + c +
              "  -> Zustand " + (a * 4 + b * 2 + c) + " (C*4+B*2+A = " + (c * 4 + b * 2 + a) + ")");
  wait(STEP_MS);
}
s.target.disconnect();
ds.shutdown();
