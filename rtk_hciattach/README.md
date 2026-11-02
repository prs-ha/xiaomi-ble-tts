
# RTL8723BS Bluetooth H5 – OpenLumi

Scop: corectarea inițializării Bluetooth pe Xiaomi DGNWG05LM.

## Comportament observat

- HCI Version: 0x06
- HCI Revision: 0x000b
- LMP Subversion înainte de firmware: 0x8723
- LMP Subversion după firmware: 0xe40e
- Firmware Version: 0x1e3ee40e

Utilitarul original recunoaște RTL8723BS cu 0x8723,
dar raportează IC: NONE cu 0xe40e.

## Obiectiv

Păstrarea inițializării normale pentru 0x8723 și
gestionarea sigură a stării cu firmware deja încărcat.

Nu se modifică executabilul original de pe Xiaomi.
