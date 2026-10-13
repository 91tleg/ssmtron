# Gauge hardware parts

What building the actual gauge unit needs, as opposed to the bench test
rig (see `firmware/hil/SETUP.md` for that). Items the repo pins exactly
are listed as such; items it doesn't (no part number in the codebase)
are flagged so you don't treat a guess as a spec.

## Core

| Part | Spec | Qty | Notes |
| --- | --- | --- | --- |
| nRF52840 DK | Nordic nRF52840-DK | 1 | The board every build in this repo targets (`nrf52840dk/nrf52840`). |
| Round display panel | GC9A01 driver, SPI, 240x240 | 1 | `firmware/lib/display/gc9a01` is written for this exact driver chip; "1.28 inch round LCD GC9A01" modules are the common form these ship in. None on the bench yet (see `bsp/nrf52840dk/board.overlay`'s comment). |

## ECU connection — **not pinned in the repo, needs selecting**

SSM1 is a plain **5V TTL, 2-wire full-duplex** serial connection —
separate transmit and receive pins, not a 12V ISO9141 K-line bus and not
single-wire. Confirmed against [Alcyone's Select Monitor connector page](https://www.alcyone.org.uk/ssm/ports.html),
which documents the actual connector pinout below. `bsp.h`'s "K-line via
the level shifter" comment refers to a plain 3.3V↔5V logic shift, not an
automotive K-line transceiver IC (those solve a different, higher-voltage
problem this protocol doesn't have).

Connector **B35** (yellow, 9-pin) — the Select Monitor connector, found
in the driver's side kick panel on Subaru models of this era:

| Pin | Function |
| --- | --- |
| 2 | Transmit Data (ECU → tool) |
| 3 | Receive Data (tool → ECU) |
| 5 | +12V (used here to power the unit from the car) |
| 9 | Ground |

The mating plug is the same as the 1984-97 Legacy radio connector —
sold in car audio shops as "Autoleads PC2-29-4", "Metra 70-1780", or
"Scosche SU02B".

| Part | Spec | Qty | Notes |
| --- | --- | --- | --- |
| Logic-level shifter (3.3V ↔ 5V) | Bidirectional, 2-channel (or two discrete stages) | 1 | Shifts the nRF52840's 3.3V UART1 to the ECU's 5V TTL lines: DK UART1 TX (P1.02) → level shift → connector pin 3 (car RX); connector pin 2 (car TX) → level shift → DK UART1 RX (P1.01). Plain TX/RX crossover, no single-wire multiplexing needed. |
| Mating connector | Autoleads PC2-29-4 / Metra 70-1780 / Scosche SU02B (car audio radio-harness connector) | 1 | Mates with the car's B35 Select Monitor connector. |

## Power — from the diagnostic port (pin 5), not USB

| Part | Spec | Qty | Notes |
| --- | --- | --- | --- |
| 12V-to-5V (or 3.3V, matching the DK's external-supply input) buck converter | Automotive-rated, reverse-polarity protected | 1 | Powers the DK from connector pin 5 instead of USB. Set the DK's power-source switch to external supply before connecting it (see the nRF52840 DK User Guide) — don't leave USB connected at the same time. |

## Wiring

```
 B35 connector (car)                Interface                      nRF52840 DK
┌───────────────────┐
│ Pin 2  TX (5V) ────┼───────────▶ level shifter A ───────────────▶ UART1 RX (P1.01)
│ Pin 3  RX (5V) ◀───┼──────────── level shifter B ◀─────────────── UART1 TX (P1.02)
│                    │
│ Pin 5  +12V ───────┼───────────▶ buck converter  ───────────────▶ external supply in
│                    │             (12V → 5V/3.3V,                 (power-source switch
│                    │              reverse-polarity                set to external;
│                    │              protected)                      see DK User Guide)
│                    │
│ Pin 9  GND ────────┼──────────────────┬──────────────────────────▶ GND
└───────────────────┘                   │
                          common ground: buck converter, both
                          level-shifter stages, DK, and the
                          display module
```

Each level shifter is one bidirectional 3.3V↔5V channel; two channels
(or two discrete stages) cover TX and RX. No single-wire multiplexing
needed — see the ECU connection section above.

## Enclosure

| Part | Spec | Qty | Notes |
| --- | --- | --- | --- |
| Case | Fits the DK (or a future custom board) + round display cutout | 1 | Not designed yet; dimensions depend on final board choice (DK vs. a custom PCB). |

## Explicitly not needed

- Extra push buttons: `gauge.c` currently drives NEXT/ACTION off the DK's own onboard buttons (`BSP_GPIO_NEXT`/`BSP_GPIO_ACTION` in `bsp.h`) — no separate buttons required unless/until this moves off the DK form factor.

## Estimated cost — custom PCB build (Raytac MDBT50Q-1MV2, off the DK)

Rough BOM estimate, not a quote — several parts here (level shifter,
regulator, enclosure) aren't chosen yet, so treat this as a working
number, not a budget.

| Item | Est. cost | Basis |
| --- | --- | --- |
| Raytac MDBT50Q-1MV2 (nRF52840 module) | $6.15 | Single-unit distributor price |
| GC9A01 round display module | $3–5 | Common hobbyist-market price range |
| Bare custom PCB (small 2-layer board, qty 5) | $2–5 | Typical low-cost prototype fab pricing at this size/layer count |
| 12V→3.3V regulator (buck or LDO) + passives | $1.50–3 | Not yet chosen; estimate for a small buck IC or LDO + caps/inductor |
| K-line level shifter (2-channel, 3.3V↔5V) | $0.50–1 | Not yet chosen; a small dual-channel IC or two discrete transistors |
| Connectors, headers, 2x tactile buttons, B35 mating pigtail | $6–10 | SWD header, button switches, harness connector + wire |
| Enclosure (3D-printed) | $8–15 | Not designed yet; rough estimate for a small case + round-display cutout |

**Total: roughly $28–44 per unit** at prototype quantities (1–10 units,
hand-assembled) — call it **~$35** as a working number.

At a small batch (50–100 units), the module and display drop maybe
20-30% with bulk pricing, but that's largely offset by outsourced PCB
assembly setup fees unless you hand-solder — don't expect much below
~$30/unit until well past 100 units.

Not included above, and worth tracking separately:

- Your own time (PCB layout, firmware, assembly).
- First-batch PCB fab minimums: a small flat fee for the first 5-10 bare
  boards (roughly $20-40 total including shipping) — one-time, not
  per-unit.
- No debug probe purchase needed: the existing nRF52840 DK can flash a
  standalone board over SWD as an external debugger (Nordic's "debug
  out" mode) — confirm that setup once the PCB exists.
- No certification (FCC/CE) costs — irrelevant for a personal project,
  only relevant if these are ever sold.
