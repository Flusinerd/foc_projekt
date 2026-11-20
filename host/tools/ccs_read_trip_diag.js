// Trip-Diagnose aus app_speed lesen, ohne Reset (f28388D_nogel.ccxml), CPU nur kurz angehalten.
// Aufruf: C:/ti/ccs2101/ccs/scripting/run.bat host/tools/ccs_read_trip_diag.js
// Adressen aus matlab/app_speed_ert_rtw/app_speed.map (focp_c28x_boot_trips, focp_c28x_trip_diag);
// nach jedem Build pruefen.
const ADDR_BOOT = 0x16CE2n, ADDR_DIAG = 0x16EECn;
const NAMES = ["Grund (1 SW, 2 Summe)", "Zeit Trip ISR [ms]", "ia [A]", "ib [A]", "ic [A]", "Offset I_C [counts]",
               "FOCP aktiv", "erster Trip im Speed-Task [ms]", "Loeschen beim Scharfschalten [ms]",
               "max |i| vor dem Scharfschalten [A]"];
const ds = initScripting();
ds.setScriptingTimeout(30000);
ds.configure("C:/Users/Student/Desktop/foc-framework/foc-framework/host/tools/ccs/f28388D_nogel.ccxml");
const s = ds.openSession(/C28xx_CPU1/);
s.target.connect();
if (!s.target.isHalted()) s.target.halt();
const boot = s.memory.read(ADDR_BOOT, 1, 16)[0];
const w = s.memory.read(ADDR_DIAG, 20, 16);
s.target.run(false);
s.target.disconnect();
ds.shutdown();
console.log("focp_c28x_boot_trips = " + boot);
const buf = new DataView(new ArrayBuffer(4));
for (let i = 0; i < 10; ++i) {
  buf.setUint16(0, Number(w[2 * i + 1])); buf.setUint16(2, Number(w[2 * i]));   // C28x: low word first
  console.log("diag[" + i + "] " + NAMES[i] + " = " + buf.getFloat32(0).toPrecision(6));
}
