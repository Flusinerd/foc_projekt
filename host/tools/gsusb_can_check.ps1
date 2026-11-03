<#
.SYNOPSIS
  Minimaler CAN-Mitleser fuer CANable/candleLight (gs_usb ueber WinUSB), ohne Zusatzsoftware.

.DESCRIPTION
  Stellt die Bitrate ein (Standard 1 Mbit/s), startet Kanal 0, liest Rahmen fuer -ListenSec Sekunden,
  sendet optional einen Testrahmen und liest danach weiter. Gedacht fuer den CAN-Grundtest mit
  matlab/models/test/foc_can_probe.slx (Tx 0x1F0 Zaehler, Echo 0x1F1 -> 0x1F2).

  gs_usb: Vendor-Control-Requests HOST_FORMAT(0), BITTIMING(1), MODE(2), BT_CONST(4);
  Bulk IN 0x81 / OUT 0x02, Rahmen 20 Byte: echo_id u32, can_id u32, dlc u8, channel u8,
  flags u8, reserved u8, data[8].

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File host\tools\gsusb_can_check.ps1 -SendId 0x1F1 -SendData "34 12"
#>
param(
    [int]$Bitrate = 1000000,
    [double]$ListenSec = 3,
    [int]$SendId = -1,
    [string]$SendData = '',          # Hex-Bytes, z. B. "34 12"
    [double]$AfterSendSec = 2,
    [string]$InterfaceGuid = '{c15b4308-04d3-11e6-b3ea-6057189e6443}'
)

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class GsUsb {
  [StructLayout(LayoutKind.Sequential)] public struct SP_DEVICE_INTERFACE_DATA { public int cbSize; public Guid g; public int Flags; public IntPtr Reserved; }
  [StructLayout(LayoutKind.Sequential, Pack = 1)] public struct SETUP { public byte RequestType; public byte Request; public ushort Value; public ushort Index; public ushort Length; }
  [DllImport("setupapi.dll", SetLastError = true)] static extern IntPtr SetupDiGetClassDevs(ref Guid g, IntPtr e, IntPtr h, int f);
  [DllImport("setupapi.dll", SetLastError = true)] static extern bool SetupDiEnumDeviceInterfaces(IntPtr s, IntPtr d, ref Guid g, int i, ref SP_DEVICE_INTERFACE_DATA did);
  [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr s, ref SP_DEVICE_INTERFACE_DATA did, IntPtr detail, int size, out int req, IntPtr di);
  [DllImport("setupapi.dll")] static extern bool SetupDiDestroyDeviceInfoList(IntPtr s);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern SafeFileHandle CreateFile(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("winusb.dll", SetLastError = true)] static extern bool WinUsb_Initialize(SafeFileHandle h, out IntPtr w);
  [DllImport("winusb.dll", SetLastError = true)] static extern bool WinUsb_Free(IntPtr w);
  [DllImport("winusb.dll", SetLastError = true)] static extern bool WinUsb_ControlTransfer(IntPtr w, SETUP s, byte[] b, int len, out int done, IntPtr o);
  [DllImport("winusb.dll", SetLastError = true)] static extern bool WinUsb_ReadPipe(IntPtr w, byte pipe, byte[] b, int len, out int done, IntPtr o);
  [DllImport("winusb.dll", SetLastError = true)] static extern bool WinUsb_WritePipe(IntPtr w, byte pipe, byte[] b, int len, out int done, IntPtr o);
  [DllImport("winusb.dll", SetLastError = true)] static extern bool WinUsb_SetPipePolicy(IntPtr w, byte pipe, int policy, int len, ref int val);

  static SafeFileHandle file; static IntPtr usb;

  public static string Open(string guid) {
    Guid g = new Guid(guid);
    IntPtr set = SetupDiGetClassDevs(ref g, IntPtr.Zero, IntPtr.Zero, 0x12); // PRESENT | DEVICEINTERFACE
    var did = new SP_DEVICE_INTERFACE_DATA(); did.cbSize = Marshal.SizeOf(did);
    if (!SetupDiEnumDeviceInterfaces(set, IntPtr.Zero, ref g, 0, ref did)) throw new Exception("Kein Geraet mit Interface-GUID " + guid);
    int req; SetupDiGetDeviceInterfaceDetail(set, ref did, IntPtr.Zero, 0, out req, IntPtr.Zero);
    IntPtr buf = Marshal.AllocHGlobal(req);
    Marshal.WriteInt32(buf, IntPtr.Size == 8 ? 8 : 6);
    if (!SetupDiGetDeviceInterfaceDetail(set, ref did, buf, req, out req, IntPtr.Zero)) throw new Exception("InterfaceDetail " + Marshal.GetLastWin32Error());
    string path = Marshal.PtrToStringUni(new IntPtr(buf.ToInt64() + 4));
    Marshal.FreeHGlobal(buf); SetupDiDestroyDeviceInfoList(set);
    file = CreateFile(path, 0xC0000000, 3, IntPtr.Zero, 3, 0x40000080, IntPtr.Zero); // RW, share RW, OPEN_EXISTING, OVERLAPPED|NORMAL
    if (file.IsInvalid) throw new Exception("CreateFile " + Marshal.GetLastWin32Error());
    if (!WinUsb_Initialize(file, out usb)) throw new Exception("WinUsb_Initialize " + Marshal.GetLastWin32Error());
    return path;
  }
  public static void Close() { if (usb != IntPtr.Zero) WinUsb_Free(usb); if (file != null) file.Dispose(); }
  public static byte[] Ctrl(bool toHost, byte req, ushort value, byte[] data, int len) {
    var s = new SETUP(); s.RequestType = (byte)(toHost ? 0xC1 : 0x41); s.Request = req; s.Value = value; s.Index = 0; s.Length = (ushort)len;
    byte[] b = data ?? new byte[len]; int done;
    if (!WinUsb_ControlTransfer(usb, s, b, len, out done, IntPtr.Zero)) throw new Exception("Control " + req + ": " + Marshal.GetLastWin32Error());
    return b;
  }
  public static void SetTimeout(int ms) { int v = ms; WinUsb_SetPipePolicy(usb, 0x81, 0x03, 4, ref v); } // PIPE_TRANSFER_TIMEOUT
  public static byte[] Read() { byte[] b = new byte[64]; int done; if (!WinUsb_ReadPipe(usb, 0x81, b, b.Length, out done, IntPtr.Zero)) return null; byte[] r = new byte[done]; Array.Copy(b, r, done); return r; }
  public static void Write(byte[] b) { int done; if (!WinUsb_WritePipe(usb, 0x02, b, b.Length, out done, IntPtr.Zero)) throw new Exception("WritePipe " + Marshal.GetLastWin32Error()); }
}
'@

$SendBytes = [byte[]]@($SendData -split '[\s,]+' | Where-Object { $_ } | ForEach-Object { [Convert]::ToByte(($_ -replace '^0x', ''), 16) })
function U32([uint32[]]$v) { $b = New-Object byte[] (4 * $v.Count); for ($i = 0; $i -lt $v.Count; $i++) { [BitConverter]::GetBytes($v[$i]).CopyTo($b, 4 * $i) }; ,$b }
function Fmt($f) {
    $id = [BitConverter]::ToUInt32($f, 4); $dlc = $f[8]; $echo = [BitConverter]::ToUInt32($f, 0)
    $flags = @(); if ($id -band 0x20000000) { $flags += 'ERR' }; if ($id -band 0x40000000) { $flags += 'RTR' }
    $data = ($f[12..(11 + [Math]::Min($dlc, 8))] | ForEach-Object { '{0:X2}' -f $_ }) -join ' '
    # Fehlerrahmen (CAN_ERR_FLAG): ID-Bits = Fehlerklasse (0x20 ACK, 0x04 Controller, 0x08 Protokoll),
    # data[1] = Controllerstatus (0x04/0x08 Warnung RX/TX, 0x10/0x20 passiv RX/TX)
    $kind = if ($id -band 0x20000000) { 'ERROR  ' } elseif ($echo -ne 0xFFFFFFFF) { 'TX-Echo' } else { 'RX     ' }
    '{0} ID=0x{1:X3} DLC={2} [{3}] {4}' -f $kind, ($id -band 0x1FFFFFFF), $dlc, $data, ($flags -join ',')
}

try {
    $path = [GsUsb]::Open($InterfaceGuid)
    "Geraet: $path"
    [void][GsUsb]::Ctrl($false, 0, 1, (U32 0x0000BEEF), 4)                     # HOST_FORMAT
    $bt = [GsUsb]::Ctrl($true, 4, 0, $null, 40)                               # BT_CONST
    $fclk = [BitConverter]::ToUInt32($bt, 4)
    $tq = 16; $brp = [int]($fclk / ($Bitrate * $tq))
    if ($brp * $tq * $Bitrate -ne $fclk) { $tq = 10; $brp = [int]($fclk / ($Bitrate * $tq)) }
    $prop = 1; $ps2 = [int][Math]::Max(1, [Math]::Round($tq * 0.125)); $ps1 = $tq - 1 - $prop - $ps2
    "Adaptertakt {0} Hz -> BRP={1}, Tq={2} (prop {3}, ps1 {4}, ps2 {5}), Abtastpunkt {6:P1}" -f $fclk, $brp, $tq, $prop, $ps1, $ps2, ((1 + $prop + $ps1) / $tq)
    [void][GsUsb]::Ctrl($false, 1, 0, (U32 $prop, $ps1, $ps2, 1, $brp), 20)  # BITTIMING
    [void][GsUsb]::Ctrl($false, 2, 0, (U32 1, 0), 8)                         # MODE start, normal
    [GsUsb]::SetTimeout(200)

    $stats = @{}
    function Listen([double]$sec) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt $sec) {
            $f = [GsUsb]::Read(); if ($f -eq $null -or $f.Length -lt 20) { continue }
            $id = [BitConverter]::ToUInt32($f, 4) -band 0x1FFFFFFF
            $stats[$id] = 1 + [int]$stats[$id]
            '{0,7:F3} s  {1}' -f $sw.Elapsed.TotalSeconds, (Fmt $f)
        }
    }
    "--- mitlesen {0} s ---" -f $ListenSec
    Listen $ListenSec
    if ($SendId -ge 0) {
        $fr = New-Object byte[] 20
        [BitConverter]::GetBytes([uint32]0).CopyTo($fr, 0); [BitConverter]::GetBytes([uint32]$SendId).CopyTo($fr, 4)
        $fr[8] = [byte]$SendBytes.Count; for ($i = 0; $i -lt $SendBytes.Count; $i++) { $fr[12 + $i] = $SendBytes[$i] }
        [GsUsb]::Write($fr)
        "--- gesendet ID=0x{0:X3} [{1}], mitlesen {2} s ---" -f $SendId, (($SendBytes | ForEach-Object { '{0:X2}' -f $_ }) -join ' '), $AfterSendSec
        Listen $AfterSendSec
    }
    "--- Zusammenfassung ---"
    if ($stats.Count -eq 0) { "keine Rahmen empfangen" } else { $stats.GetEnumerator() | Sort-Object Name | ForEach-Object { 'ID 0x{0:X3}: {1} Rahmen' -f $_.Name, $_.Value } }
    [void][GsUsb]::Ctrl($false, 2, 0, (U32 0, 0), 8)                         # MODE reset
}
finally { [GsUsb]::Close() }
