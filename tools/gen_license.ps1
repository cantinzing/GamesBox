<#
.SYNOPSIS
    Offline activation-code generator (developer side, PowerShell version, no Python needed).

.PARAMETER MachineCode
    The machine code shown on the client "Settings -> Software Activation" page (Windows MachineGuid).

.EXAMPLE
    .\gen_license.ps1 7b271c8a-d803-4d0d-952a-fae99d39eb8b
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$MachineCode
)

Add-Type @'
using System;
using System.Security.Cryptography;
using System.Text;
public static class Act {
  const string Secret = "GameLibrary-Offline-Activation-7f3a9c2e1b8d4f60a5c71e3b9d2f48a0";
  static string B32(byte[] data){
    const string a = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    long buffer = 0; int bits = 0; var sb = new StringBuilder();
    foreach (byte b in data) { buffer = (buffer << 8) | b; bits += 8; while (bits >= 5) { bits -= 5; sb.Append(a[(int)((buffer >> bits) & 31)]); } }
    if (bits > 0) { sb.Append(a[(int)((buffer << (5 - bits)) & 31)]); }
    return sb.ToString();
  }
  public static string Gen(string mc){
    var key = Encoding.UTF8.GetBytes(Secret);
    var msg = Encoding.UTF8.GetBytes(mc);
    using (var h = new HMACSHA256(key)) {
      var d = h.ComputeHash(msg);
      var raw = B32(d).Substring(0, 25);
      var r = new StringBuilder();
      for (int i = 0; i < raw.Length; i++) { if (i > 0 && i % 5 == 0) r.Append('-'); r.Append(raw[i]); }
      return r.ToString();
    }
  }
}
'@

if ([string]::IsNullOrWhiteSpace($MachineCode)) {
    Write-Error "Machine code is empty"
    exit 1
}

[Act]::Gen($MachineCode.Trim())
