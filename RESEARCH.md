# oSR Research Direction

Research has moved into the project wiki at [docs/wiki/README.md](docs/wiki/README.md).

Use that directory as the living LLM wiki:

- [source-index.md](docs/wiki/source-index.md) records primary sources and local code evidence.
- [sr-input-contract.md](docs/wiki/sr-input-contract.md) tracks the common SR input contract.
- [fsr.md](docs/wiki/fsr.md), [xess.md](docs/wiki/xess.md), and [dlss-streamline.md](docs/wiki/dlss-streamline.md) split vendor research.
- [temporal-reconstruction.md](docs/wiki/temporal-reconstruction.md) captures the current theory.
- [borrowable-ideas.md](docs/wiki/borrowable-ideas.md) separates usable concepts from licensing/proprietary boundaries.
- [research-backlog.md](docs/wiki/research-backlog.md) is the next experiment queue.

Current thesis:

```text
oSR is not a sharper scaler. oSR is a low-cost temporal evidence system.
```

The credible breakthrough target for Radeon 760M-class hardware is deterministic sparse temporal attention: validate renderer inputs, score a small set of plausible temporal candidates, route extra work only to risky tiles, and expose every confidence decision in logs/captures. This is inspired by the public direction of modern neural SR, but avoids heavy inference as the default path.
