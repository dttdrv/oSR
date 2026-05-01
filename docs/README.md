# oSR Documentation

This folder is the practical project manual. The root-level files stay short and
high-signal; deeper operational detail lives here.

## Start Here

- [Project status](PROJECT_STATUS.md): what exists, what works, what is not done,
  and how to read the current maturity level.
- [Harness user guide](HARNESS_USER_GUIDE.md): how to run the Windows DX12
  harness, the portable Linux-safe harness, manual visual checks, and metric
  gates.
- [Capture packs](CAPTURE_PACKS.md): output layout, file meanings, metric
  interpretation, and analysis workflows.
- [Linux development](LINUX.md): elementaryOS setup, Linux build target split,
  and the current portable harness path.
- [Development guide](DEVELOPMENT.md): build options, target map, test commands,
  logging/state discipline, and contribution rules.
- [Research wiki](wiki/README.md): source-backed SR knowledge base and research
  backlog.

## Current Practical Path

For a Linux-safe quality/capture run:

```bash
bash tools/run_portable_wind_tunnel.sh
```

For Windows GPU-backed harness work:

```powershell
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

For all portable tests:

```bash
bash tools/run_linux_core_tests.sh
```

For the current Windows/manual suite:

```powershell
$env:OSR_NO_PAUSE='1'
tools\run_manual_tests.bat
```

## Documentation Rules

- Keep `LOG.md` append-only.
- Keep `STATE.yaml` current when capability, build status, assumptions, or next
  actions change.
- Add links in `docs/wiki/source-index.md` before making new research-backed
  implementation claims.
- Do not call a feature finished because one screenshot looks good. Prefer a
  capture pack, metric gate, and a short note about remaining risk.
