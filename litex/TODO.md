# LiteX Shell — Open Issues

## Status update — 2026-04-25

Bitstream **does build and route successfully** on the current branch
(see `litex/build/hallycon_m2sdr_platform/gateware/*.bit`).  Commits
`e453b91` (LiteX integration close-out) + `d08a537` (RX low-SNR fixes)
land cleanly on Vivado 2025.2 with positive timing slack, 79.5% LUT,
67.5% DSP, 89.3% BRAM utilisation.

**What still bothers us** (not blockers, but cleanup wishlist):

- The smartconnect `ctrl_xbar` is still in the BD.  The synth_ip-in-project-mode
  workaround works on this machine but isn't portable.  See the original
  rant below for the full diagnosis.
- 12 GB WSL is genuinely tight for full builds — AWS 32 GB is the
  recommended host.

**Pending work that's actually on the critical path**:

1. AD9361 register configuration (RF freq, gain, sample rate, filter BW)
2. AD9361 FPGA pin / timing constraints for board bring-up
3. PCIe ↔ host driver path (DMA, MSI, register window)
4. Multi-frame back-to-back regression (sync_detect re-arm path)
5. Variable-frame-size regression matrix (see `docs/REGRESSION_PLAN.md`)

The notes below are preserved for the smartconnect/`ctrl_xbar` debug
context — read as historical archive.

---

## (Historical) Eliminate smartconnect (ctrl_xbar) from BD — was BLOCKING 2026-04-17
- **Current state**: 2×5 AXI smartconnect routes MAC m_axi writes to tx_chain/ofdm_tx CSRs. 68 sub-IPs, VHDL dependencies, constant build headaches.
- **What MAC actually does**: writes ~4 registers per TX packet (modcod, n_syms, n_data_bytes, ap_start) to tx_chain and ofdm_tx via m_axi.
- **Fix**: Replace m_axi with direct ap_vld/ap_none wires. Requires HLS changes in 3 blocks:
  1. **ofdm_mac**: remove `m_axi phy_csr`, add direct output ports (tx_modcod, tx_n_syms, tx_n_data_bytes, tx_start_pulse)
  2. **tx_chain**: change params from `s_axilite` to `ap_none`, change `ap_ctrl_hs` to `ap_ctrl_none` with start wire
  3. **ofdm_tx**: same as tx_chain
- Then re-synth all 3, re-export IPs, update create_ofdm_bd.tcl (remove smartconnect, wire directly), update shell.py
- **Host CSR access**: route each block's s_axi_ctrl through LiteX wishbone instead of smartconnect S00
- **Priority**: High — this is the main build blocker for LiteX integration

### Evidence confirming this is the right priority (2026-04-17 session)
- LiteX's auto-generated tcl calls `synth_ip` in project mode. Vivado explicitly
  warns: `[Vivado 12-5447] synth_ip is not supported in project mode`.
- For monolithic IPs (xfft, pcie) this works accidentally. For smartconnects
  with sub-IPs, `synth_ip` produces a 47 KB stub-only DCP instead of a 1 MB
  full DCP (old OOC build's ctrl_xbar DCP is 1 MB, proving the proper flow
  produces a working artifact).
- Downstream effect: main synth_design links the stub; opt_design's DRC fires
  `[DRC INBB-3] Black Box: ctrl_xbar/inst of type bd_4852 has undefined
  contents`. No bitstream possible.
- `launch_runs ofdm_chain_ctrl_xbar_0_synth_1` is the project-mode-correct
  alternative (the old `vivado/ofdm_impl/` OOC build used this and produced
  a working DCP). Injecting this into LiteX's auto-generated tcl is untested.
- Attempted workarounds that DON'T work (save time for future):
  * `set_property generate_synth_checkpoint false` — read-only on smartconnects
  * `set_property synth_checkpoint_mode None` — also read-only
  * Commenting out the `synth_ip` line — xbar module then not found by synth_design
  * `read_checkpoint -cell` to graft old working DCP — fails because ctrl_xbar
    is not a black box (only its interior `inst` is)

### Other findings worth capturing
- `ofdm_mac_0/Data_m_axi_csr_master` has NO reachable targets. `assign_bd_address`
  excluded tx_chain and ofdm_tx slaves because of a `register` vs `memory` usage
  mismatch. The MAC cannot autonomously configure the TX chain over AXI today.
  Vivado's fix: add `include_bd_addr_seg` after each `assign_bd_address` for
  that space — but this is moot if the smartconnect is being eliminated.
- `sync_detect_0/n_syms_fb_vld` input is tied to 0 (unconnected). Check whether
  sync_detect gates the feedback on `vld`; if so, RX feedback is dead.
- Width mismatches (benign, matches shell.py bridge logic):
  * `sync_detect iq_in(48) ← adc_fifo M_AXIS(40)` — upper 8b padding discarded
  * `ctrl_xbar S01_awlock(1) ← mac m_axi_csr AWLOCK(2)` — AXI3 vs AXI4 bit

### Memory envelope (on 12 GB WSL, this box)
- Per-IP synth_ip peaks: xfft 3.7 GB, pcie_s7 3.9 GB (cache hit 1 min).
- Main synth_design with BLACK-BOXED xbar peaks at 5.4 GB (route would be higher).
- Main synth_design with REAL xbar inline: untested (never got there).
- Route_design untested. Probably 7–10 GB for this design size.
- **Moral: this needs more RAM headroom than a 12 GB WSL can reliably give.
  AWS with 32 GB is the right move for stable builds while debugging this.**

## RF IQ width: reduce TX side from 48-bit to 40-bit
- **Current state**: RX side is 40-bit (via 5-byte adc_input_fifo), TX side is 48-bit (raw ofdm_tx iq_out)
- **Goal**: Both sides should be 40-bit
- **Why**: iq_t `{i:16, q:16, last:1}` pads to 48-bit (6 bytes) by HLS, but the `last` + pad bits [47:32] are unused on the RF interface. 40-bit (5 bytes) carries only i/q + minimal padding.
- **Options**:
  1. Add a 6→5 byte axis_data_fifo or axis_subset_converter on ofdm_tx iq_out in create_ofdm_bd.tcl (strips upper 8 bits)
  2. Change HLS iq_t to `ap_axiu<32,0,0,0>` with sideband TLAST (clean 32-bit TDATA) — requires re-synth of ofdm_tx + ofdm_rx + sync_detect
- **Priority**: Low — do alongside smartconnect elimination since tx_chain/ofdm_tx re-synth is needed anyway
