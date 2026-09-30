#!/usr/bin/env python3
"""
biss_updater.py — Auto-update BISS keys for MultiCS from public SoftCam.Key source

Fetches the public SoftCam.Key from GitHub, extracts BISS (F) entries,
smart-merges them into the local MultiCS Softcam.cfg, and backs up the old file.

Usage:
    python3 biss_updater.py [--dry-run] [--verbose]
    python3 biss_updater.py --config-path /var/etc/Softcam.cfg --source-url URL

Cron (every 6 hours):
    0 */6 * * * root /usr/bin/python3 /var/etc/biss_updater.py >> /var/log/biss_updater.log 2>&1
"""

import argparse
import json
import os
import re
import shutil
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration defaults
# ---------------------------------------------------------------------------
DEFAULT_SOURCE_URL = "https://raw.githubusercontent.com/popking159/softcam/master/SoftCam.Key"
DEFAULT_CONFIG_PATH = "/var/etc/Softcam.cfg"
DEFAULT_STATE_PATH = "/var/etc/biss_updater.state"
DEFAULT_LOG_PATH = "/var/log/biss_updater.log"
DEFAULT_BAK_PATH = "/var/etc/Softcam.cfg.bak"
DEFAULT_META_PATH = "/var/etc/biss_updater.meta"

# ---------------------------------------------------------------------------
# SID mapping table
# ---------------------------------------------------------------------------
# SoftCam.Key identifiers don't always match the actual SID used by CCcam
# clients. This table maps known identifiers to correct SIDs.
# Format: "field1_hex_string" -> sid_int
SID_OVERRIDE = {
    "F30EE048": 0x17ED,   # 1+1 International (4.9E)
}

# ---------------------------------------------------------------------------
# Logging helper
# ---------------------------------------------------------------------------
class Logger:
    def __init__(self, verbose=False, log_path=None):
        self.verbose = verbose
        self.log_path = log_path

    def info(self, msg):
        line = f"[INFO]  {msg}"
        print(line)
        self._to_file(line)

    def warn(self, msg):
        line = f"[WARN]  {msg}"
        print(line, file=sys.stderr)
        self._to_file(line)

    def debug(self, msg):
        if self.verbose:
            line = f"[DEBUG] {msg}"
            print(line)
            self._to_file(line)

    def error(self, msg):
        line = f"[ERROR] {msg}"
        print(line, file=sys.stderr)
        self._to_file(line)

    def _to_file(self, line):
        if self.log_path:
            try:
                with open(self.log_path, "a", encoding="utf-8") as f:
                    f.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {line}\n")
            except OSError:
                pass


# ---------------------------------------------------------------------------
# Fetcher
# ---------------------------------------------------------------------------
def fetch_source(url, state_path, logger):
    """Download SoftCam.Key from URL. Use ETag to skip unchanged files."""
    headers = {}
    etag = None
    last_modified = None

    # Load cached state
    state = {}
    if os.path.exists(state_path):
        try:
            with open(state_path, "r", encoding="utf-8") as f:
                state = json.load(f)
            etag = state.get("etag")
            last_modified = state.get("last_modified")
        except (json.JSONDecodeError, OSError) as e:
            logger.warn(f"Could not read state file: {e}")

    req = urllib.request.Request(url)
    if etag:
        req.add_header("If-None-Match", etag)
    if last_modified:
        req.add_header("If-Modified-Since", last_modified)

    try:
        logger.debug(f"Fetching {url} ...")
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = resp.read().decode("utf-8", errors="replace")
            new_etag = resp.headers.get("ETag")
            new_modified = resp.headers.get("Last-Modified")

            # Save state
            state["etag"] = new_etag
            state["last_modified"] = new_modified
            state["last_fetch"] = time.time()
            try:
                with open(state_path, "w", encoding="utf-8") as f:
                    json.dump(state, f, indent=2)
            except OSError as e:
                logger.warn(f"Could not write state file: {e}")

            logger.info(f"Downloaded {len(data)} bytes from source.")
            return data

    except urllib.error.HTTPError as e:
        if e.code == 304:
            logger.info("Source unchanged (304 Not Modified). Skipping update.")
            return None
        logger.error(f"HTTP error {e.code}: {e.reason}")
        raise
    except urllib.error.URLError as e:
        logger.error(f"Network error: {e.reason}")
        raise


# ---------------------------------------------------------------------------
# Parser (reuses softcam_convert.py logic for BISS entries)
# ---------------------------------------------------------------------------
def parse_biss_entries(raw_text, logger):
    """Parse SoftCam.Key text and return dict of BISS entries keyed by (caid, provid, sid)."""
    entries = {}
    errors = []
    line_no = 0

    for raw_line in raw_text.splitlines():
        line_no += 1
        line = raw_line.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue

        # Remove inline comment
        if ";" in line:
            line = line[:line.index(";")].strip()

        # Match: F <field1> <cw_type> <key_data>  or  F <field1> <key_data>
        m = re.match(r"^F\s+(\S+)\s+(\S+)\s+(\S+)\s*$", line)
        if not m:
            m = re.match(r"^F\s+(\S+)\s+(\S+)\s*$", line)
            if not m:
                continue
            field1 = m.group(1)
            cw_type = ""
            key_data = m.group(2)
        else:
            field1 = m.group(1)
            cw_type = m.group(2)
            key_data = m.group(3)

        # Clean hex
        key_data = key_data.replace(" ", "").replace("\t", "")
        if not key_data:
            continue

        # Validate hex
        try:
            key_bytes = bytes.fromhex(key_data)
        except ValueError:
            errors.append(f"Line {line_no}: invalid hex")
            continue

        # Skip all-zero keys
        if all(b == 0 for b in key_bytes):
            continue

        # Extract SID from field1 with smarter logic:
        # 1. Try hardcoded override mapping first
        # 2. If field1 ends with '1FFF', it's a CCcam key -> first 4 chars = SID
        # 3. Otherwise fall back to last 4 chars
        try:
            field1_upper = field1.upper()
            if field1_upper in SID_OVERRIDE:
                sid = SID_OVERRIDE[field1_upper]
                logger.debug(f"Mapped {field1} -> SID {sid:04x} via override")
            elif field1_upper.endswith("1FFF"):
                sid = int(field1[-8:-4], 16)  # First 4 chars of 8-char identifier
            else:
                sid = int(field1[-4:], 16)   # Last 4 chars
        except ValueError:
            errors.append(f"Line {line_no}: invalid SID field: {field1}")
            continue

        caid = 0x2600
        provid = 0

        # Validate CW length: 8 bytes (half) or 16 bytes (full)
        if len(key_bytes) == 8:
            # Single half-CW; will duplicate later
            pass
        elif len(key_bytes) == 16:
            # Full CW
            pass
        else:
            errors.append(f"Line {line_no}: unexpected key length {len(key_bytes)} bytes")
            continue

        key = (caid, provid, sid)
        if key not in entries:
            entries[key] = {"cw0": None, "cw1": None}

        if cw_type in ("00", "0"):
            entries[key]["cw0"] = key_bytes
        elif cw_type in ("01", "1"):
            entries[key]["cw1"] = key_bytes
        else:
            # No type or unrecognized: treat as cw0
            entries[key]["cw0"] = key_bytes

    if errors and len(errors) <= 5:
        for e in errors:
            logger.debug(e)
    elif errors:
        logger.debug(f"{len(errors)} parse warnings suppressed")

    # Merge even/odd halves into full 16-byte CW
    result = {}
    for key, halves in entries.items():
        cw0 = halves.get("cw0")
        cw1 = halves.get("cw1")

        if cw0 is None and cw1 is not None:
            cw0 = cw1
        if cw1 is None and cw0 is not None:
            cw1 = cw0

        if cw0 is None or cw1 is None:
            continue

        # Ensure both are 8 bytes
        if len(cw0) != 8 or len(cw1) != 8:
            continue

        full_cw = cw0 + cw1
        result[key] = full_cw.hex().upper()

    logger.info(f"Parsed {len(result)} BISS entries from source.")
    return result


# ---------------------------------------------------------------------------
# Local config loader
# ---------------------------------------------------------------------------
def load_local_entries(config_path, logger):
    """Load existing Softcam.cfg entries. Returns (entries_dict, header_lines, manual_lines, manual_keys)."""
    entries = {}
    header_lines = []
    manual_lines = []
    manual_keys = set()
    in_manual_section = False

    if not os.path.exists(config_path):
        logger.info(f"Local config not found: {config_path}")
        return entries, header_lines, manual_lines, manual_keys

    with open(config_path, "r", encoding="utf-8") as f:
        for line in f:
            stripped = line.strip()

            # Detect manual-section marker (lines we preserve as manual entries)
            if stripped.startswith("# [MANUAL]") or stripped.startswith("# Manual entries"):
                in_manual_section = True
                manual_lines.append(line)
                continue

            if in_manual_section:
                manual_lines.append(line)
                # Parse entry lines for merging and track as manual
                m = re.match(r"^([0-9a-fA-F]{4}):([0-9a-fA-F]{6}):([0-9a-fA-F]{4}):([0-9a-fA-F]{32})\b", stripped)
                if m:
                    caid = int(m.group(1), 16)
                    provid = int(m.group(2), 16)
                    sid = int(m.group(3), 16)
                    cw = m.group(4).upper()
                    key = (caid, provid, sid)
                    entries[key] = cw
                    manual_keys.add(key)
                continue

            # Header / comment lines before first entry
            m = re.match(r"^([0-9a-fA-F]{4}):([0-9a-fA-F]{6}):([0-9a-fA-F]{4}):([0-9a-fA-F]{32})\b", stripped)
            if not m:
                header_lines.append(line)
                continue

            caid = int(m.group(1), 16)
            provid = int(m.group(2), 16)
            sid = int(m.group(3), 16)
            cw = m.group(4).upper()
            entries[(caid, provid, sid)] = cw

    logger.info(f"Loaded {len(entries)} entries from local config ({len(manual_keys)} manual).")
    return entries, header_lines, manual_lines, manual_keys


# ---------------------------------------------------------------------------
# Merger
# ---------------------------------------------------------------------------
def smart_merge(local_entries, remote_entries, manual_keys, logger):
    """
    Smart merge:
    - Keep local-only entries (manual)
    - Add new remote entries
    - Update changed remote entries (except manual keys)
    - Preserve unchanged entries
    Returns: (merged_dict, report)
    """
    merged = dict(local_entries)  # Start with local (includes manual)
    added = []
    updated = []
    unchanged = []
    protected = []

    for key, remote_cw in remote_entries.items():
        if key in manual_keys:
            # Never overwrite manual entries with remote data
            protected.append(key)
            if key not in merged:
                # This shouldn't happen, but handle gracefully
                merged[key] = remote_cw
        elif key not in merged:
            merged[key] = remote_cw
            added.append(key)
        elif merged[key] != remote_cw:
            merged[key] = remote_cw
            updated.append(key)
        else:
            unchanged.append(key)

    # Keys that exist locally but not remotely are kept automatically (manual)
    local_only = [k for k in local_entries if k not in remote_entries]

    report = {
        "added": added,
        "updated": updated,
        "unchanged": unchanged,
        "local_only": local_only,
        "protected": protected,
        "total": len(merged),
    }

    logger.info(
        f"Merge report: added={len(added)}, updated={len(updated)}, "
        f"unchanged={len(unchanged)}, protected={len(protected)}, "
        f"local_only={len(local_only)}, total={len(merged)}"
    )
    return merged, report


# ---------------------------------------------------------------------------
# Writer
# ---------------------------------------------------------------------------
def write_softcam_cfg(path, merged_entries, header_lines, manual_lines, report, logger, dry_run=False):
    """Write merged entries back to Softcam.cfg with backup."""
    if dry_run:
        logger.info("[DRY-RUN] Would write the following entries:")
        for key in sorted(merged_entries.keys(), key=lambda k: (k[0], k[1], k[2])):
            caid, provid, sid = key
            cw = merged_entries[key]
            logger.info(f"  {caid:04x}:{provid:06x}:{sid:04x}:{cw}")
        return True

    # Backup existing file
    if os.path.exists(path):
        bak_path = path + ".bak"
        try:
            shutil.copy2(path, bak_path)
            logger.info(f"Backup created: {bak_path}")
        except OSError as e:
            logger.error(f"Failed to create backup: {e}")
            return False

    # Build output
    lines = []
    lines.append(f"# Auto-updated by biss_updater.py")
    lines.append(f"# Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S %Z')}")
    lines.append(f"# Source: {DEFAULT_SOURCE_URL}")
    lines.append(f"# Changes: added={len(report['added'])}, updated={len(report['updated'])}, unchanged={len(report['unchanged'])}, protected={len(report.get('protected', []))}, manual={len(report['local_only'])}")
    lines.append("")

    # Preserve original header comments (up to first blank line or entry)
    for hl in header_lines:
        if hl.strip() and not hl.startswith("# Auto-updated"):
            lines.append(hl.rstrip("\n"))

    lines.append("")
    lines.append("# BISS keys from public source")
    lines.append("")

    # Sort and write merged entries (exclude manual ones - they go in their own section)
    sorted_keys = sorted(merged_entries.keys(), key=lambda k: (k[0], k[1], k[2]))
    current_caid = None
    for key in sorted_keys:
        # Skip manual keys - they'll be written in the manual section
        if key in report.get("manual_keys", set()):
            continue

        caid, provid, sid = key
        cw = merged_entries[key]

        if caid != current_caid:
            caid_name = "BISS" if caid == 0x2600 else f"CAID {caid:04x}"
            lines.append(f"# {'-' * 45}")
            lines.append(f"# {caid_name}")
            lines.append(f"# {'-' * 45}")
            current_caid = caid

        lines.append(f"{caid:04x}:{provid:06x}:{sid:04x}:{cw}")

    # Append manual section if any
    if manual_lines:
        lines.append("")
        lines.append("# [MANUAL] — entries not in remote source (preserved forever)")
        for ml in manual_lines:
            lines.append(ml.rstrip("\n"))

    lines.append("")

    # Atomic write: tmp -> rename
    tmp_path = path + ".tmp"
    try:
        with open(tmp_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        os.replace(tmp_path, path)
        logger.info(f"Config written: {path} ({len(merged_entries)} entries)")
        return True
    except OSError as e:
        logger.error(f"Failed to write config: {e}")
        return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Auto-update BISS keys for MultiCS")
    parser.add_argument("--config-path", default=DEFAULT_CONFIG_PATH, help="Path to Softcam.cfg")
    parser.add_argument("--source-url", default=DEFAULT_SOURCE_URL, help="URL to SoftCam.Key")
    parser.add_argument("--state-path", default=DEFAULT_STATE_PATH, help="Path to state/ETag cache")
    parser.add_argument("--log-path", default=DEFAULT_LOG_PATH, help="Path to log file")
    parser.add_argument("--dry-run", action="store_true", help="Show changes without writing")
    parser.add_argument("--verbose", action="store_true", help="Verbose debug output")
    parser.add_argument("--force", action="store_true", help="Force update even if source unchanged")

    args = parser.parse_args()

    logger = Logger(verbose=args.verbose, log_path=args.log_path)
    logger.info("=" * 50)
    logger.info("biss_updater.py started")

    try:
        # 1. Fetch source
        if args.force:
            # Bypass etag check by removing state
            if os.path.exists(args.state_path):
                os.remove(args.state_path)

        raw_text = fetch_source(args.source_url, args.state_path, logger)

        # Load local entries regardless (needed for metadata total count)
        local_entries, header_lines, manual_lines, manual_keys = load_local_entries(args.config_path, logger)
        report = {"added": [], "updated": [], "unchanged": [], "local_only": [], "total": len(local_entries), "manual_keys": manual_keys}
        keys_changed = False

        if raw_text is not None:
            # 2. Parse remote entries
            remote_entries = parse_biss_entries(raw_text, logger)
            if not remote_entries:
                logger.warn("No BISS entries found in source. Aborting.")
                return 1

            # 3. Merge
            merged, report = smart_merge(local_entries, remote_entries, manual_keys, logger)

            if report["added"] or report["updated"]:
                # 4. Write
                ok = write_softcam_cfg(
                    args.config_path, merged, header_lines, manual_lines, report, logger, dry_run=args.dry_run
                )

                if ok and not args.dry_run:
                    keys_changed = True
                    logger.info("Update complete. Restart MultiCS to reload keys.")
                elif args.dry_run:
                    logger.info("Dry-run complete. No files modified.")
                else:
                    logger.error("Update failed.")
                    return 1
            else:
                logger.info("No new or changed keys. Local config is up to date.")
        else:
            logger.info("No changes from source.")

        # 5. Write metadata for Emulator page (always on success)
        meta = {
            "last_check": int(time.time()),
            "last_update": int(time.time()) if keys_changed else None,
            "added": len(report["added"]),
            "updated": len(report["updated"]),
            "total": report["total"],
        }

        # Preserve last_update from existing meta if no change this run
        if not keys_changed and os.path.exists(DEFAULT_META_PATH):
            try:
                with open(DEFAULT_META_PATH, "r", encoding="utf-8") as f:
                    old_meta = json.load(f)
                if old_meta.get("last_update"):
                    meta["last_update"] = old_meta["last_update"]
            except (json.JSONDecodeError, OSError):
                pass

        try:
            with open(DEFAULT_META_PATH, "w", encoding="utf-8") as f:
                json.dump(meta, f, indent=2)
        except OSError as e:
            logger.warn(f"Could not write meta file: {e}")

        return 0

    except Exception as e:
        logger.error(f"Unhandled exception: {e}")
        import traceback
        logger.debug(traceback.format_exc())
        return 1


if __name__ == "__main__":
    sys.exit(main())
