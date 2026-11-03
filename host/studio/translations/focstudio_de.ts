<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de">
<context>
    <name>AboutDialog</name>
    <message>
        <location filename="../AboutDialog.cpp" line="19"/>
        <source>About FOC Studio</source>
        <translation>Über FOC Studio</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="35"/>
        <source>Operating and commissioning tool for the FOC framework. Project CarpeDiem, Laboratory for Actuators, Hochschule Bochum.</source>
        <translation>Bedien- und Inbetriebnahmewerkzeug für das FOC-Framework. Projekt CarpeDiem, Labor für Aktorik, Hochschule Bochum.</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="39"/>
        <source>Program version:</source>
        <translation>Programmversion:</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="40"/>
        <source>Protocol version (FOCP):</source>
        <translation>Protokollversion (FOCP):</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="41"/>
        <source>%1 (%2 parameters, %3 signals)</source>
        <translation>%1 (%2 Parameter, %3 Signale)</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="43"/>
        <source>Drive firmware:</source>
        <translation>Antriebs-Firmware:</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="45"/>
        <source>%1.%2, board 0x%3, protocol %4</source>
        <translation>%1.%2, Platine 0x%3, Protokoll %4</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="48"/>
        <source>not connected</source>
        <translation>nicht verbunden</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="49"/>
        <source>Qt version:</source>
        <translation>Qt-Version:</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="50"/>
        <source>&amp;Program</source>
        <translation>&amp;Programm</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="56"/>
        <source>&amp;Licenses</source>
        <translation>&amp;Lizenzen</translation>
    </message>
    <message>
        <location filename="../AboutDialog.cpp" line="60"/>
        <source>About &amp;Qt</source>
        <translation>Über &amp;Qt</translation>
    </message>
</context>
<context>
    <name>CommissioningPage</name>
    <message>
        <location filename="../CommissioningPage.cpp" line="31"/>
        <source>Position sensor</source>
        <translation>Lagegeber</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="35"/>
        <source>&amp;Sensor:</source>
        <translation>&amp;Geber:</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="36"/>
        <source>Sensorless operation needs a minimum speed (obs.n_switch). Hybrid starts with Hall and switches to the observer above that speed.</source>
        <translation>Sensorloser Betrieb braucht eine Mindestdrehzahl (obs.n_switch). Hybrid startet mit Hall und wechselt oberhalb dieser Drehzahl auf den Beobachter.</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="40"/>
        <source>Calibration</source>
        <translation>Kalibrierung</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="42"/>
        <source>Measure &amp;current offset</source>
        <translation>&amp;Stromoffset messen</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="44"/>
        <source>Power stage off, motor at standstill. Measures the zero point of the current sensors.</source>
        <translation>Leistungsstufe aus, Motor im Stillstand. Misst den Nullpunkt der Stromsensoren.</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="45"/>
        <source>Calibrate &amp;Hall offset</source>
        <translation>&amp;Hall-Offset kalibrieren</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="47"/>
        <source>The rotor turns slowly in open loop with ol.I, forwards and backwards. The shaft must turn freely! The result is written to hall.offset.</source>
        <translation>Der Rotor dreht im Open-Loop mit ol.I langsam vorwärts und rückwärts. Die Welle muss frei drehen können! Das Ergebnis wird in hall.offset geschrieben.</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="51"/>
        <source>&amp;Identify motor</source>
        <translation>&amp;Motor identifizieren</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="53"/>
        <source>Measures Rs, Ls, flux and inertia (phase P4, not yet available in the firmware).</source>
        <translation>Misst Rs, Ls, Fluss und Trägheit (Phase P4, in der Firmware noch nicht verfügbar).</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="85"/>
        <source>hall.offset = %1°</source>
        <translation>hall.offset = %1°</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="87"/>
        <source>Not connected.</source>
        <translation>Nicht verbunden.</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="89"/>
        <source>Calibration running… The result appears in the event log.</source>
        <translation>Kalibrierung läuft … Das Ergebnis erscheint im Ereignisprotokoll.</translation>
    </message>
    <message>
        <location filename="../CommissioningPage.cpp" line="90"/>
        <source>Calibrations are only possible in state Ready (now: %1).</source>
        <translation>Kalibrierungen sind nur im Zustand „Bereit“ möglich (jetzt: %1).</translation>
    </message>
</context>
<context>
    <name>ConnectionDialog</name>
    <message>
        <location filename="../ConnectionDialog.cpp" line="28"/>
        <source>Connect to drive</source>
        <translation>Mit Antrieb verbinden</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="34"/>
        <source>Virtual drive (simulation)</source>
        <translation>Virtueller Antrieb (Simulation)</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="35"/>
        <source>CANable / candleLight (USB)</source>
        <translation>CANable / candleLight (USB)</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="39"/>
        <source>&amp;Interface:</source>
        <translation>&amp;Schnittstelle:</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="43"/>
        <source>&amp;Search</source>
        <translation>S&amp;uchen</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="49"/>
        <source>&amp;Adapter:</source>
        <translation>&amp;Adapter:</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="54"/>
        <source>&amp;Channel:</source>
        <translation>Ka&amp;nal:</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="60"/>
        <source>%1 Mbit/s</source>
        <translation>%1 Mbit/s</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="60"/>
        <source>%1 kbit/s</source>
        <translation>%1 kbit/s</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="61"/>
        <source>&amp;Bitrate:</source>
        <translation>&amp;Bitrate:</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="67"/>
        <source>&amp;Node:</source>
        <translation>&amp;Knoten:</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="74"/>
        <source>Co&amp;nnect</source>
        <translation>&amp;Verbinden</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="75"/>
        <source>&amp;Test connection</source>
        <translation>Verbindung &amp;testen</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="121"/>
        <source>%1 (#%2)</source>
        <translation>%1 (#%2)</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="123"/>
        <source>No adapter found</source>
        <translation>Kein Adapter gefunden</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="148"/>
        <source>Testing…</source>
        <translation>Teste …</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="155"/>
        <source>Interface could not be opened: %1</source>
        <translation>Schnittstelle lässt sich nicht öffnen: %1</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="165"/>
        <source>Drive answers: firmware %1.%2, protocol v%3.</source>
        <translation>Antrieb antwortet: Firmware %1.%2, Protokoll v%3.</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="166"/>
        <source>Interface open, but node %1 does not answer. Check bitrate, node number, wiring and termination.</source>
        <translation>Schnittstelle offen, aber Knoten %1 antwortet nicht. Bitrate, Knotennummer, Verkabelung und Abschlusswiderstand prüfen.</translation>
    </message>
    <message>
        <location filename="../ConnectionDialog.cpp" line="169"/>
        <source>Protocol version differs from this program (v%1).</source>
        <translation>Die Protokollversion weicht von diesem Programm ab (v%1).</translation>
    </message>
</context>
<context>
    <name>ControlPanel</name>
    <message>
        <location filename="../ControlPanel.cpp" line="30"/>
        <source>rpm</source>
        <translation>min⁻¹</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="37"/>
        <source>Operation</source>
        <translation>Bedienung</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="41"/>
        <source>&amp;Enable</source>
        <translation>&amp;Freigeben</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="46"/>
        <source>Switches the power stage on. Only possible in state Ready.</source>
        <translation>Schaltet die Leistungsstufe ein. Nur im Zustand „Bereit“ möglich.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="47"/>
        <source>&amp;Acknowledge fault</source>
        <translation>Fehler &amp;quittieren</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="56"/>
        <source>Can only be changed while the drive is not running.</source>
        <translation>Nur änderbar, solange der Antrieb nicht läuft.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="57"/>
        <source>Control &amp;mode:</source>
        <translation>Regel&amp;modus:</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="62"/>
        <source>&amp;Setpoint:</source>
        <translation>&amp;Sollwert:</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="64"/>
        <source>0</source>
        <translation>0</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="65"/>
        <source>Set the setpoint to zero and send it.</source>
        <translation>Sollwert auf null setzen und senden.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="66"/>
        <source>Se&amp;nd</source>
        <translation>Se&amp;nden</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="74"/>
        <source>Setpoint slider</source>
        <translation>Sollwert-Schieberegler</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="129"/>
        <source>Not connected.</source>
        <translation>Nicht verbunden.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="130"/>
        <source>Reading parameters…</source>
        <translation>Parameter werden gelesen …</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="131"/>
        <source>Choose a control mode to enable the drive.</source>
        <translation>Zum Freigeben einen Regelmodus wählen.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="132"/>
        <source>Remove the cause shown under Active faults, then acknowledge.</source>
        <translation>Ursache unter „Aktive Fehler“ beheben, dann quittieren.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="133"/>
        <source>Setpoint changes are sent with Enter, Send or when releasing the slider.</source>
        <translation>Sollwertänderungen werden mit Enter, „Senden“ oder beim Loslassen des Schiebereglers gesendet.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="197"/>
        <source>Enable drive – safety check</source>
        <translation>Antrieb freigeben – Sicherheitsprüfung</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="199"/>
        <source>The motor will start turning. Confirm each point before the first enable:</source>
        <translation>Der Motor beginnt sich zu drehen. Vor der ersten Freigabe jeden Punkt bestätigen:</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="204"/>
        <source>&amp;1  The power supply is current-limited.</source>
        <translation>&amp;1  Das Netzteil ist strombegrenzt.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="205"/>
        <source>&amp;2  The motor shaft can turn freely; nothing and nobody is in the way.</source>
        <translation>&amp;2  Die Motorwelle kann frei drehen; nichts und niemand ist im Weg.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="206"/>
        <source>&amp;3  The safe-state button is within reach.</source>
        <translation>&amp;3  Der SafeState-Taster ist griffbereit.</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="207"/>
        <source>&amp;4  The supply voltage is being watched (braking can raise it).</source>
        <translation>&amp;4  Die Versorgungsspannung wird beobachtet (Bremsen kann sie anheben).</translation>
    </message>
    <message>
        <location filename="../ControlPanel.cpp" line="216"/>
        <source>Enable</source>
        <translation>Freigeben</translation>
    </message>
</context>
<context>
    <name>DashboardPage</name>
    <message>
        <location filename="../DashboardPage.cpp" line="24"/>
        <source>Drive state</source>
        <translation>Antriebszustand</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="28"/>
        <source>Speed</source>
        <translation>Drehzahl</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="29"/>
        <source>Supply voltage</source>
        <translation>Versorgungsspannung</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="30"/>
        <source>Temperature</source>
        <translation>Temperatur</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="31"/>
        <source>Control mode</source>
        <translation>Regelmodus</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="32"/>
        <source>Position sensor</source>
        <translation>Lagegeber</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="35"/>
        <source>Active faults</source>
        <translation>Aktive Fehler</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="37"/>
        <source>No active faults.</source>
        <translation>Keine aktiven Fehler.</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="73"/>
        <source>Not connected</source>
        <translation>Nicht verbunden</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="73"/>
        <source>Waiting for drive…</source>
        <translation>Warte auf Antrieb …</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="78"/>
        <source>%1 rpm</source>
        <translation>%1 min⁻¹</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="79"/>
        <source>%1 V</source>
        <translation>%1 V</translation>
    </message>
    <message>
        <location filename="../DashboardPage.cpp" line="80"/>
        <source>%1 °C</source>
        <translation>%1 °C</translation>
    </message>
</context>
<context>
    <name>DriveTexts</name>
    <message>
        <location filename="../FaultText.cpp" line="8"/>
        <source>Starting</source>
        <translation>Startet</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="9"/>
        <source>Calibrating current offset</source>
        <translation>Stromoffset wird kalibriert</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="10"/>
        <source>Ready</source>
        <translation>Bereit</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="11"/>
        <source>Running</source>
        <translation>In Betrieb</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="12"/>
        <source>Fault</source>
        <translation>Fehler</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="13"/>
        <source>Calibration running</source>
        <translation>Kalibrierung läuft</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="14"/>
        <source>Unknown state %1</source>
        <translation>Unbekannter Zustand %1</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="33"/>
        <source>Off</source>
        <translation>Aus</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="34"/>
        <source>Torque</source>
        <translation>Moment</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="35"/>
        <source>Speed</source>
        <translation>Drehzahl</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="36"/>
        <source>Position</source>
        <translation>Lage</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="37"/>
        <source>Open loop I/f</source>
        <translation>Open-Loop I/f</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="45"/>
        <source>Hall</source>
        <translation>Hall</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="46"/>
        <source>Sensorless</source>
        <translation>Sensorlos</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="47"/>
        <source>Hybrid (Hall + observer)</source>
        <translation>Hybrid (Hall + Beobachter)</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="59"/>
        <source>Hardware overcurrent trip</source>
        <translation>Überstromabschaltung (Hardware)</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="60"/>
        <source>Check motor cable and phases for short circuits. Acknowledge only after the cause is found.</source>
        <translation>Motorkabel und Phasen auf Kurzschluss prüfen. Erst quittieren, wenn die Ursache gefunden ist.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="63"/>
        <source>Overcurrent</source>
        <translation>Überstrom</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="64"/>
        <source>Phase current exceeded the limit. Reduce load or acceleration; if it repeats, have the current controller checked.</source>
        <translation>Der Phasenstrom hat die Grenze überschritten. Last oder Beschleunigung verringern; tritt es wieder auf, den Stromregler prüfen lassen.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="67"/>
        <source>Supply voltage too low</source>
        <translation>Versorgungsspannung zu niedrig</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="68"/>
        <source>Check the supply voltage and the current limit of the power supply, then acknowledge.</source>
        <translation>Versorgungsspannung und Strombegrenzung des Netzteils prüfen, dann quittieren.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="71"/>
        <source>Supply voltage too high</source>
        <translation>Versorgungsspannung zu hoch</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="72"/>
        <source>Check the supply voltage. Braking also feeds energy back into the supply, which a lab power supply cannot absorb: brake more gently or use a supply that can.</source>
        <translation>Versorgungsspannung prüfen. Beim Bremsen fließt außerdem Energie in die Versorgung zurück, die ein Labornetzteil nicht aufnehmen kann: sanfter bremsen oder eine rückspeisefähige Versorgung verwenden.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="75"/>
        <source>Power stage too hot</source>
        <translation>Leistungsstufe zu heiß</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="76"/>
        <source>Let the drive cool down and check cooling and load before acknowledging.</source>
        <translation>Antrieb abkühlen lassen und vor dem Quittieren Kühlung und Last prüfen.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="79"/>
        <source>Hall sensor error</source>
        <translation>Fehler Hall-Sensoren</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="80"/>
        <source>Check the Hall sensor cable and its supply. Afterwards run the Hall calibration.</source>
        <translation>Kabel und Versorgung der Hall-Sensoren prüfen. Danach die Hall-Kalibrierung ausführen.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="83"/>
        <source>Sensorless operation lost the rotor position</source>
        <translation>Sensorloser Betrieb hat die Rotorlage verloren</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="84"/>
        <source>Speed too low for sensorless operation or motor data wrong. Use Hall or hybrid mode.</source>
        <translation>Drehzahl zu niedrig für sensorlosen Betrieb oder Motordaten falsch. Hall- oder Hybridbetrieb verwenden.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="87"/>
        <source>Connection to the PC interrupted</source>
        <translation>Verbindung zum PC unterbrochen</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="88"/>
        <source>The drive missed the heartbeat. Check CAN cable, bitrate and node number, then acknowledge.</source>
        <translation>Der Antrieb hat kein Lebenszeichen mehr erhalten. CAN-Kabel, Bitrate und Knotennummer prüfen, dann quittieren.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="91"/>
        <source>Safe-state button pressed</source>
        <translation>SafeState-Taster betätigt</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="92"/>
        <source>Release the safe-state button and acknowledge the fault.</source>
        <translation>SafeState-Taster lösen und den Fehler quittieren.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="95"/>
        <source>Inconsistent parameters</source>
        <translation>Widersprüchliche Parameter</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="96"/>
        <source>The parameter set is not valid. Load a known parameter file or the factory defaults.</source>
        <translation>Der Parametersatz ist ungültig. Eine bekannte Parameterdatei oder die Werkseinstellungen laden.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="99"/>
        <source>Overspeed</source>
        <translation>Überdrehzahl</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="100"/>
        <source>Check the speed setpoint and whether the motor is being driven externally.</source>
        <translation>Drehzahl-Sollwert prüfen und ob der Motor von außen angetrieben wird.</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="103"/>
        <source>Unknown fault (bit 0x%1)</source>
        <translation>Unbekannter Fehler (Bit 0x%1)</translation>
    </message>
    <message>
        <location filename="../FaultText.cpp" line="104"/>
        <source>The drive firmware is newer than this program. Update FOC Studio.</source>
        <translation>Die Antriebs-Firmware ist neuer als dieses Programm. FOC Studio aktualisieren.</translation>
    </message>
</context>
<context>
    <name>EventLog</name>
    <message>
        <location filename="../EventLog.cpp" line="20"/>
        <source>current offset</source>
        <translation>Stromoffset</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="21"/>
        <source>Hall offset</source>
        <translation>Hall-Offset</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="22"/>
        <source>motor identification</source>
        <translation>Motoridentifikation</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="23"/>
        <source>calibration</source>
        <translation>Kalibrierung</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="42"/>
        <source>Error</source>
        <translation>Fehler</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="42"/>
        <source>Warning</source>
        <translation>Warnung</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="42"/>
        <source>Info</source>
        <translation>Info</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="49"/>
        <source>Connected via %1, node %2.</source>
        <translation>Verbunden über %1, Knoten %2.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="50"/>
        <source>Disconnected.</source>
        <translation>Verbindung getrennt.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="51"/>
        <source>Calibration started: %1.</source>
        <translation>Kalibrierung gestartet: %1.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="52"/>
        <source>Calibration finished: %1.</source>
        <translation>Kalibrierung abgeschlossen: %1.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="53"/>
        <source>Calibration failed or not available: %1.</source>
        <translation>Kalibrierung fehlgeschlagen oder nicht verfügbar: %1.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="54"/>
        <source>State: %1</source>
        <translation>Zustand: %1</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="57"/>
        <source>Fault</source>
        <translation>Fehler</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="57"/>
        <source>Fault: %1. %2</source>
        <translation>Fehler: %1. %2</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="61"/>
        <source>Acknowledged: %1</source>
        <translation>Quittiert: %1</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="63"/>
        <source>Parameters saved in the drive.</source>
        <translation>Parameter im Antrieb gespeichert.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="67"/>
        <source>read-only</source>
        <translation>nur lesbar</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="68"/>
        <source>value out of range</source>
        <translation>Wert außerhalb des Bereichs</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="69"/>
        <source>locked while running</source>
        <translation>im Betrieb gesperrt</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="70"/>
        <source>unknown to the drive</source>
        <translation>dem Antrieb unbekannt</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="71"/>
        <source>request rejected</source>
        <translation>Anfrage abgelehnt</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="73"/>
        <source>Parameter %1 not changed: %2.</source>
        <translation>Parameter %1 nicht geändert: %2.</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="76"/>
        <source>Answer for unknown parameter ID 0x%1 (drive firmware newer than this program?).</source>
        <translation>Antwort für unbekannte Parameter-ID 0x%1 (Antriebs-Firmware neuer als dieses Programm?).</translation>
    </message>
    <message>
        <location filename="../EventLog.cpp" line="79"/>
        <source>Protocol or table version of drive and program differ. Update firmware or FOC Studio.</source>
        <translation>Protokoll- oder Tabellenversion von Antrieb und Programm unterscheiden sich. Firmware oder FOC Studio aktualisieren.</translation>
    </message>
</context>
<context>
    <name>EventsPage</name>
    <message>
        <location filename="../EventsPage.cpp" line="32"/>
        <source>All events</source>
        <translation>Alle Ereignisse</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="33"/>
        <source>Warnings and errors</source>
        <translation>Warnungen und Fehler</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="34"/>
        <source>Errors only</source>
        <translation>Nur Fehler</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="35"/>
        <source>&amp;Show:</source>
        <translation>&amp;Anzeigen:</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="37"/>
        <source>E&amp;xport…</source>
        <translation>E&amp;xportieren …</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="38"/>
        <source>C&amp;lear view</source>
        <translation>Ansicht &amp;leeren</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="39"/>
        <source>Clears this list. The log file is kept.</source>
        <translation>Leert diese Liste. Die Logdatei bleibt erhalten.</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="40"/>
        <source>Open log &amp;folder</source>
        <translation>Log-&amp;Ordner öffnen</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="50"/>
        <location filename="../EventsPage.cpp" line="133"/>
        <source>Time</source>
        <translation>Zeit</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="50"/>
        <location filename="../EventsPage.cpp" line="133"/>
        <source>Level</source>
        <translation>Stufe</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="50"/>
        <location filename="../EventsPage.cpp" line="133"/>
        <source>Message</source>
        <translation>Meldung</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="61"/>
        <source>Log file: %1</source>
        <translation>Logdatei: %1</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="120"/>
        <location filename="../EventsPage.cpp" line="127"/>
        <source>Export events</source>
        <translation>Ereignisse exportieren</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="123"/>
        <source>CSV files (*.csv);;Text files (*.txt)</source>
        <translation>CSV-Dateien (*.csv);;Textdateien (*.txt)</translation>
    </message>
    <message>
        <location filename="../EventsPage.cpp" line="127"/>
        <source>Cannot write %1:
%2</source>
        <translation>%1 kann nicht geschrieben werden:
%2</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../MainWindow.cpp" line="32"/>
        <location filename="../MainWindow.cpp" line="175"/>
        <source>FOC Studio</source>
        <translation>FOC Studio</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="36"/>
        <source>&amp;Connect…</source>
        <translation>&amp;Verbinden …</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="37"/>
        <source>Ctrl+K</source>
        <translation>Ctrl+K</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="39"/>
        <source>&amp;Disconnect</source>
        <translation>&amp;Trennen</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="41"/>
        <source>Start &amp;demo (virtual drive)</source>
        <translation>&amp;Demo starten (virtueller Antrieb)</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="43"/>
        <source>&amp;Quit</source>
        <translation>&amp;Beenden</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="47"/>
        <source>&amp;Drive</source>
        <translation>&amp;Antrieb</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="55"/>
        <source>&amp;View</source>
        <translation>A&amp;nsicht</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="56"/>
        <source>&amp;Expert mode</source>
        <translation>&amp;Expertenmodus</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="62"/>
        <source>&amp;Language</source>
        <translation>&amp;Sprache</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="66"/>
        <source>&amp;System default</source>
        <translation>&amp;Systemsprache</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="77"/>
        <source>Language</source>
        <translation>Sprache</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="77"/>
        <source>The new language is used after restarting FOC Studio.</source>
        <translation>Die neue Sprache gilt nach einem Neustart von FOC Studio.</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="81"/>
        <source>&amp;Help</source>
        <translation>&amp;Hilfe</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="82"/>
        <source>&amp;About FOC Studio…</source>
        <translation>&amp;Über FOC Studio …</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="85"/>
        <source>Drive</source>
        <translation>Antrieb</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="93"/>
        <source>STOP  (Space / Esc)</source>
        <translation>STOPP  (Leertaste / Esc)</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="101"/>
        <source>Switches the power stage off immediately.</source>
        <translation>Schaltet die Leistungsstufe sofort ab.</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="124"/>
        <source>&amp;Overview</source>
        <translation>Ü&amp;bersicht</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="125"/>
        <source>&amp;Parameters</source>
        <translation>&amp;Parameter</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="126"/>
        <source>E&amp;vents</source>
        <translation>&amp;Ereignisse</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="147"/>
        <source>Expert mode</source>
        <translation>Expertenmodus</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="148"/>
        <source>Expert mode allows changing controller and motor parameters, running calibrations and recording raw signals.

Wrong values can make the drive unstable or damage motor and power stage. Continue?</source>
        <translation>Im Expertenmodus lassen sich Regler- und Motorparameter ändern, Kalibrierungen ausführen und Rohsignale aufzeichnen.

Falsche Werte können den Antrieb instabil machen oder Motor und Leistungsstufe beschädigen. Fortfahren?</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="169"/>
        <source>&amp;Scope</source>
        <translation>&amp;Scope</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="170"/>
        <source>&amp;Commissioning</source>
        <translation>&amp;Inbetriebnahme</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="175"/>
        <source>FOC Studio – expert mode</source>
        <translation>FOC Studio – Expertenmodus</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="186"/>
        <source>Connection failed</source>
        <translation>Verbindung fehlgeschlagen</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="192"/>
        <source>Demo</source>
        <translation>Demo</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="211"/>
        <source>Not connected</source>
        <translation>Nicht verbunden</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="215"/>
        <source>Connected: %1, node %2, %3 messages</source>
        <translation>Verbunden: %1, Knoten %2, %3 Nachrichten</translation>
    </message>
    <message>
        <location filename="../MainWindow.cpp" line="220"/>
        <source>Firmware %1.%2, protocol v%3</source>
        <translation>Firmware %1.%2, Protokoll v%3</translation>
    </message>
</context>
<context>
    <name>ParamFile</name>
    <message>
        <location filename="../ParamFile.cpp" line="62"/>
        <source>The file is not valid JSON (%1).</source>
        <translation>Die Datei ist kein gültiges JSON (%1).</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="67"/>
        <source>The file is not a FOC Studio parameter file.</source>
        <translation>Die Datei ist keine FOC-Studio-Parameterdatei.</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="72"/>
        <source>File format version %1 is not supported by this program.</source>
        <translation>Dateiformat-Version %1 wird von diesem Programm nicht unterstützt.</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="77"/>
        <source>The file was written for protocol version %1, this program uses version %2.</source>
        <translation>Die Datei wurde für Protokollversion %1 geschrieben, dieses Programm verwendet Version %2.</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="91"/>
        <source>%1 (%2): unknown parameter, skipped.</source>
        <translation>%1 (%2): unbekannter Parameter, übersprungen.</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="96"/>
        <source>%1: name in file is &quot;%2&quot;, expected &quot;%3&quot;; skipped.</source>
        <translation>%1: Name in der Datei ist „%2“, erwartet „%3“; übersprungen.</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="104"/>
        <source>%1: value missing, skipped.</source>
        <translation>%1: Wert fehlt, übersprungen.</translation>
    </message>
    <message>
        <location filename="../ParamFile.cpp" line="109"/>
        <source>%1 = %2 is outside %3 … %4, skipped.</source>
        <translation>%1 = %2 liegt außerhalb %3 … %4, übersprungen.</translation>
    </message>
</context>
<context>
    <name>ParametersPage</name>
    <message>
        <location filename="../ParametersPage.cpp" line="29"/>
        <source>Motor</source>
        <translation>Motor</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="30"/>
        <source>Power stage</source>
        <translation>Leistungsstufe</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="31"/>
        <source>Control</source>
        <translation>Regelung</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="32"/>
        <source>Current controller</source>
        <translation>Stromregler</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="33"/>
        <source>Speed controller</source>
        <translation>Drehzahlregler</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="34"/>
        <source>Position controller</source>
        <translation>Lageregler</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="35"/>
        <source>Open loop</source>
        <translation>Open-Loop</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="36"/>
        <source>Hall sensors</source>
        <translation>Hall-Sensoren</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="37"/>
        <source>Observer (sensorless)</source>
        <translation>Beobachter (sensorlos)</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="38"/>
        <source>Communication</source>
        <translation>Kommunikation</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="39"/>
        <source>Virtual drive</source>
        <translation>Virtueller Antrieb</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="40"/>
        <source>System</source>
        <translation>System</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="49"/>
        <source>The drive does not know this parameter.</source>
        <translation>Der Antrieb kennt diesen Parameter nicht.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="50"/>
        <source>Read-only parameter.</source>
        <translation>Nur-Lese-Parameter.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="51"/>
        <source>Value outside the allowed range; the drive kept the old value.</source>
        <translation>Wert außerhalb des zulässigen Bereichs; der Antrieb hat den alten Wert behalten.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="52"/>
        <location filename="../ParametersPage.cpp" line="137"/>
        <source>Can only be changed while the drive is not running.</source>
        <translation>Nur änderbar, solange der Antrieb nicht läuft.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="53"/>
        <source>Request rejected by the drive.</source>
        <translation>Anfrage vom Antrieb abgelehnt.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="66"/>
        <source>&amp;Read all</source>
        <translation>Alle &amp;lesen</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="67"/>
        <location filename="../ParametersPage.cpp" line="227"/>
        <source>A&amp;pply</source>
        <translation>Ü&amp;bernehmen</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="68"/>
        <source>Disc&amp;ard</source>
        <translation>&amp;Verwerfen</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="69"/>
        <source>E&amp;xport…</source>
        <translation>E&amp;xportieren …</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="70"/>
        <source>&amp;Import…</source>
        <translation>&amp;Importieren …</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="71"/>
        <source>Sa&amp;ve in drive</source>
        <translation>Im Antrieb &amp;speichern</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="72"/>
        <source>&amp;Factory defaults…</source>
        <translation>&amp;Werkseinstellungen …</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="73"/>
        <source>Write all changed values to the drive.</source>
        <translation>Alle geänderten Werte in den Antrieb schreiben.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="74"/>
        <source>Store the current parameters permanently in the drive.</source>
        <translation>Aktuelle Parameter dauerhaft im Antrieb speichern.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="76"/>
        <source>Filter (name or description)</source>
        <translation>Filter (Name oder Beschreibung)</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="85"/>
        <source>Parameter</source>
        <translation>Parameter</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="85"/>
        <source>Value</source>
        <translation>Wert</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="85"/>
        <source>Unit</source>
        <translation>Einheit</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="85"/>
        <source>Range</source>
        <translation>Bereich</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="85"/>
        <source>ID</source>
        <translation>ID</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="138"/>
        <source>Read-only.</source>
        <translation>Nur lesbar.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="139"/>
        <source>Default: %1 %2</source>
        <translation>Standard: %1 %2</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="227"/>
        <source>A&amp;pply (%1)</source>
        <translation>Ü&amp;bernehmen (%1)</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="235"/>
        <source>Not connected. The table shows the parameter list of this program.</source>
        <translation>Nicht verbunden. Die Tabelle zeigt die Parameterliste dieses Programms.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="236"/>
        <source>Values are read-only. Switch to expert mode (View menu) to change them.</source>
        <translation>Werte nur lesbar. Zum Ändern in den Expertenmodus wechseln (Menü Ansicht).</translation>
    </message>
    <message numerus="yes">
        <location filename="../ParametersPage.cpp" line="237"/>
        <source>%n change(s) not yet written. Apply writes them to the drive.</source>
        <translation>
            <numerusform>%n Änderung noch nicht geschrieben. „Übernehmen“ schreibt sie in den Antrieb.</numerusform>
            <numerusform>%n Änderungen noch nicht geschrieben. „Übernehmen“ schreibt sie in den Antrieb.</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="238"/>
        <source>Double-click a value to change it. Changes take effect after Apply.</source>
        <translation>Zum Ändern doppelt auf einen Wert klicken. Änderungen wirken erst nach „Übernehmen“.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="239"/>
        <source>Red values were rejected by the drive (see tooltip).</source>
        <translation>Rote Werte hat der Antrieb abgelehnt (siehe Tooltip).</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="254"/>
        <source>Invalid value</source>
        <translation>Ungültiger Wert</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="255"/>
        <source>%1 must be a number between %2 and %3.</source>
        <translation>%1 muss eine Zahl zwischen %2 und %3 sein.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="282"/>
        <location filename="../ParametersPage.cpp" line="292"/>
        <source>Export parameters</source>
        <translation>Parameter exportieren</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="284"/>
        <location filename="../ParametersPage.cpp" line="302"/>
        <source>Parameter files (*.json)</source>
        <translation>Parameterdateien (*.json)</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="292"/>
        <source>Cannot write %1:
%2</source>
        <translation>%1 kann nicht geschrieben werden:
%2</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="295"/>
        <source>Parameters exported to %1.</source>
        <translation>Parameter nach %1 exportiert.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="300"/>
        <location filename="../ParametersPage.cpp" line="306"/>
        <location filename="../ParametersPage.cpp" line="311"/>
        <location filename="../ParametersPage.cpp" line="324"/>
        <source>Import parameters</source>
        <translation>Parameter importieren</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="306"/>
        <source>Cannot read %1:
%2</source>
        <translation>%1 kann nicht gelesen werden:
%2</translation>
    </message>
    <message numerus="yes">
        <location filename="../ParametersPage.cpp" line="321"/>
        <source>%n value(s) differ from the drive and are marked. Check them and press Apply.</source>
        <translation>
            <numerusform>%n Wert weicht vom Antrieb ab und ist markiert. Prüfen und „Übernehmen“ drücken.</numerusform>
            <numerusform>%n Werte weichen vom Antrieb ab und sind markiert. Prüfen und „Übernehmen“ drücken.</numerusform>
        </translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="323"/>
        <source>Skipped:</source>
        <translation>Übersprungen:</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="331"/>
        <source>Save in drive</source>
        <translation>Im Antrieb speichern</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="332"/>
        <source>There are changes that have not been applied. Save the drive&apos;s current values anyway?</source>
        <translation>Es gibt noch nicht übernommene Änderungen. Trotzdem die aktuellen Werte des Antriebs speichern?</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="336"/>
        <source>Save command sent. The drive reports the result in the event log.</source>
        <translation>Speicherbefehl gesendet. Das Ergebnis meldet der Antrieb im Ereignisprotokoll.</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="341"/>
        <source>Factory defaults</source>
        <translation>Werkseinstellungen</translation>
    </message>
    <message>
        <location filename="../ParametersPage.cpp" line="342"/>
        <source>All parameters of the drive are reset to their defaults, including motor data and calibration results. Continue?</source>
        <translation>Alle Parameter des Antriebs werden auf die Werkseinstellungen zurückgesetzt, auch Motordaten und Kalibrierergebnisse. Fortfahren?</translation>
    </message>
</context>
<context>
    <name>ScopePage</name>
    <message>
        <location filename="../ScopePage.cpp" line="28"/>
        <source>&amp;Signals (max. %1):</source>
        <translation>&amp;Signale (max. %1):</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="49"/>
        <source>Every n-th control cycle is sampled. On CAN the bus load limits the rate: each sample needs ceil(n/3) frames; about 3000 frames/s are realistic at 500 kbit/s.</source>
        <translation>Jeder n-te Regeltakt wird abgetastet. Auf CAN begrenzt die Buslast die Rate: Jede Abtastung braucht ceil(n/3) Rahmen; bei 500 kbit/s sind etwa 3000 Rahmen/s realistisch.</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="51"/>
        <source>&amp;Divider:</source>
        <translation>&amp;Teiler:</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="53"/>
        <source>Sample rate:</source>
        <translation>Abtastrate:</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="57"/>
        <source> s</source>
        <translation> s</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="59"/>
        <source>&amp;Time window:</source>
        <translation>&amp;Zeitfenster:</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="62"/>
        <source>&amp;Pause display</source>
        <translation>Anzeige &amp;anhalten</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="64"/>
        <source>C&amp;lear</source>
        <translation>&amp;Löschen</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="65"/>
        <source>Export &amp;CSV…</source>
        <translation>&amp;CSV exportieren …</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="91"/>
        <source>Lost samples: %1</source>
        <translation>Verlorene Abtastwerte: %1</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="128"/>
        <source>%1 Hz</source>
        <translation>%1 Hz</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="134"/>
        <location filename="../ScopePage.cpp" line="142"/>
        <source>Export scope data</source>
        <translation>Scope-Daten exportieren</translation>
    </message>
    <message>
        <location filename="../ScopePage.cpp" line="137"/>
        <source>CSV files (*.csv)</source>
        <translation>CSV-Dateien (*.csv)</translation>
    </message>
</context>
<context>
    <name>ScopeWidget</name>
    <message>
        <location filename="../ScopeWidget.cpp" line="88"/>
        <location filename="../ScopeWidget.cpp" line="96"/>
        <source>Not connected.</source>
        <translation>Nicht verbunden.</translation>
    </message>
    <message>
        <location filename="../ScopeWidget.cpp" line="88"/>
        <source>No signal selected or streamed.</source>
        <translation>Kein Signal ausgewählt oder übertragen.</translation>
    </message>
</context>
</TS>
