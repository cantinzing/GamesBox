#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <bcrypt.h>

namespace Licensing
{
    // Collect this machine's machine code (must match what the developer-side
    // code generator uses when producing an activation code).
    std::wstring GetMachineCode();

    // Normalize an activation code: strip separators, uppercase.
    std::wstring NormalizeCode(std::wstring const& code);

    // Base32 (RFC4648, alphabet A-Z2-7, no padding, uppercase) decode, used to
    // turn an activation code back into raw signature bytes.
    std::vector<BYTE> Base32Decode(std::wstring const& in);

    // Verify an activation code: decode it to an ECDSA-P256 signature and check
    // whether it signs the SHA-256 digest of this machine's machine code.
    // Only the public key is used for verification; the private key is never
    // shipped, so an attacker cannot forge activation codes (no keygen).
    bool VerifyActivation(std::wstring const& normalizedCode);
}
