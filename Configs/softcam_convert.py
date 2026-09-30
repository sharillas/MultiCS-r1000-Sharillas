#!/usr/bin/env python3
"""
softcam_convert.py — Convert SoftCam.Key to MultiCS Softcam.cfg

Usage:
    python softcam_convert.py input.SoftCam.Key [-o Softcam.cfg] [--tandberg-caid 1010]

Converts BISS (F) and Tandberg (T) constant-CW entries from traditional
SoftCam.Key format to MultiCS CAID:PROVID:SID:CW format.
Skips Viaccess (V), Nagravision (N), PowerVU (P) decryption keys.
"""

import re
import sys
import argparse

# Default CAID for Tandberg entries
DEFAULT_TANDBERG_CAID = 0x1010


def parse_key(line):
    """Parse a SoftCam.Key line into (prefix, field1, cw_type, key_data, comment) or None."""
    line = line.strip()
    if not line or line.startswith("#"):
        return None

    # Remove comment
    comment = ""
    if ";" in line:
        idx = line.index(";")
        comment = line[idx + 1 :].strip()
        line = line[:idx].strip()

    # Match: PREFIX FIELD1 TYPE KEY_DATA  or  PREFIX FIELD1 KEY_DATA
    m = re.match(
        r"^([VNTFP])\s+(\S+)\s+(\S+)\s+(\S+)\s*$", line
    )
    if m:
        return {
            "prefix": m.group(1),
            "field1": m.group(2),
            "cw_type": m.group(3),  # 00=even, 01=odd, or GROUP/EMM/etc
            "key_data": m.group(4),
            "comment": comment,
        }

    # Try 3-field format: PREFIX FIELD1 KEY_DATA (no cw_type)
    m = re.match(r"^([VNTFP])\s+(\S+)\s+(\S+)\s*$", line)
    if m:
        return {
            "prefix": m.group(1),
            "field1": m.group(2),
            "cw_type": "",
            "key_data": m.group(3),
            "comment": comment,
        }

    return None


def hex_to_bin(s):
    """Convert hex string to bytes, ignoring whitespace."""
    s = s.replace(" ", "").replace("\t", "")
    try:
        return bytes.fromhex(s)
    except ValueError:
        return None


def is_all_zeros(s):
    """Check if hex string is all zeros."""
    return all(c in "0 \t" for c in s)


def convert_softcam_key(inpath, outpath, tandberg_caid):
    """Convert SoftCam.Key to MultiCS Softcam.cfg."""
    entries = []
    errors = []

    with open(inpath, "r", encoding="latin-1") as f:
        for lineno, raw_line in enumerate(f, 1):
            parsed = parse_key(raw_line)
            if parsed is None:
                continue

            prefix = parsed["prefix"]
            field1 = parsed["field1"]
            cw_type = parsed["cw_type"]
            key_data = parsed["key_data"]
            comment = parsed["comment"]

            # Skip non-CW entry types
            if cw_type in ("GROUP",) or prefix in ("V", "N"):
                continue

            # Skip PowerVU EMM keys (6-hex key_data)
            if prefix == "P" and len(key_data.replace(" ", "")) >= 14:
                # P 0056 004042D8 D70A0BD206B274 = EMM key, skip
                continue

            # Skip entries with all-zero key data
            if is_all_zeros(key_data):
                continue

            key_bytes = hex_to_bin(key_data)
            if key_bytes is None:
                errors.append(f"Line {lineno}: invalid hex: {raw_line.strip()}")
                continue

            if prefix == "F":
                # BISS: field1 = TSIDSID (8 hex), SID = last 4 hex
                try:
                    sid = int(field1[-4:], 16)
                except ValueError:
                    errors.append(f"Line {lineno}: invalid SID in TSIDSID: {field1}")
                    continue
                caid = 0x2600
                provid = 0

            elif prefix == "T":
                # Tandberg: field1 = SID (4 hex)
                try:
                    sid = int(field1, 16)
                except ValueError:
                    errors.append(f"Line {lineno}: invalid SID: {field1}")
                    continue
                caid = tandberg_caid
                provid = 0

            elif prefix == "P":
                # PowerVU: extract SID from field1
                # Field1 is CAID+SID combined (8 hex) or just CAID (4 hex)
                if len(field1) == 8:
                    try:
                        caid = int(field1[:4], 16) | 0x0100  # PowerVU style
                        sid = int(field1[4:], 16)
                    except ValueError:
                        errors.append(f"Line {lineno}: invalid field: {field1}")
                        continue
                else:
                    try:
                        caid = int(field1, 16)
                        sid = 0
                    except ValueError:
                        continue
                provid = 0

                # PowerVU keys are 7 bytes (14 hex), not 8-byte CW halves
                # These need special handling — skip for now
                continue

            else:
                continue

            # Store the entry
            entries.append(
                {
                    "caid": caid,
                    "provid": provid,
                    "sid": sid,
                    "cw_type": cw_type,
                    "key_bytes": key_bytes,
                    "comment": comment,
                    "raw": raw_line.strip(),
                }
            )

    # Merge even/odd CW pairs
    merged = {}  # key: (caid, provid, sid) -> {cw0, cw1, comment}
    for e in entries:
        key = (e["caid"], e["provid"], e["sid"])
        if key not in merged:
            merged[key] = {"cw0": None, "cw1": None, "comment": e["comment"]}
        if e["cw_type"] == "00" or e["cw_type"] == "0":
            merged[key]["cw0"] = e["key_bytes"]
        elif e["cw_type"] == "01" or e["cw_type"] == "1":
            merged[key]["cw1"] = e["key_bytes"]
        else:
            # No type => store as cw0 (will be duplicated)
            merged[key]["cw0"] = e["key_bytes"]
        if e["comment"] and not merged[key]["comment"]:
            merged[key]["comment"] = e["comment"]

    # Write output
    output_lines = []
    output_lines.append("# Converted from SoftCam.Key by softcam_convert.py")
    output_lines.append(f"# Source: {inpath}")
    output_lines.append(f"# Format: CAID:PROVID:SID:CW_32hex")
    output_lines.append("")

    # Sort by caid, provid, sid
    sorted_keys = sorted(merged.keys(), key=lambda k: (k[0], k[1], k[2]))

    current_caid = None
    for caid, provid, sid in sorted_keys:
        m = merged[(caid, provid, sid)]

        # Section header
        if caid != current_caid:
            caid_name = {0x2600: "BISS", tandberg_caid: "Tandberg"}.get(caid, f"CAID {caid:04x}")
            output_lines.append(f"# {'-' * 45}")
            output_lines.append(f"# {caid_name}")
            output_lines.append(f"# {'-' * 45}")
            current_caid = caid

        # Build CW
        cw0 = m.get("cw0")
        cw1 = m.get("cw1")

        if cw0 is None and cw1 is not None:
            cw0 = cw1  # duplicate odd -> even
        if cw1 is None and cw0 is not None:
            cw1 = cw0  # duplicate even -> odd

        if cw0 is None or cw1 is None:
            continue

        full_cw = cw0 + cw1
        cw_hex = full_cw.hex().upper()

        # 8-byte half + 8-byte half = 32 hex chars
        if len(cw_hex) != 32:
            continue

        comment_str = f"  ;{m['comment']}" if m["comment"] else ""
        output_lines.append(f"{caid:04x}:{provid:06x}:{sid:04x}:{cw_hex}{comment_str}")

    output_lines.append("")

    with open(outpath, "w", encoding="utf-8") as f:
        f.write("\n".join(output_lines))

    # Summary
    converted = len(sorted_keys)
    print(f"Converted:  {converted} constant-CW entries")
    print(f"Output:     {outpath}")
    if errors:
        print(f"\nWarnings ({len(errors)}):")
        for e in errors:
            print(f"  {e}")

    return converted


def main():
    parser = argparse.ArgumentParser(
        description="Convert SoftCam.Key to MultiCS Softcam.cfg"
    )
    parser.add_argument("input", help="Path to SoftCam.Key file")
    parser.add_argument("-o", "--output", default=None, help="Output path (default: Softcam.cfg)")
    parser.add_argument(
        "--tandberg-caid",
        type=lambda x: int(x, 16),
        default=DEFAULT_TANDBERG_CAID,
        help=f"CAID for Tandberg entries (hex, default: {DEFAULT_TANDBERG_CAID:04x})",
    )
    parser.add_argument("--list", action="store_true", help="List all entries without writing")

    args = parser.parse_args()

    outpath = args.output or "Softcam.cfg"

    count = convert_softcam_key(args.input, outpath, args.tandberg_caid)

    if count == 0:
        print("No convertible entries found.")
        sys.exit(1)


if __name__ == "__main__":
    main()
