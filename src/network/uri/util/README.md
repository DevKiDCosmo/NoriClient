# Magic and Purpose Phrasing

Speficied purposes are after a magic number. They are decoded as hex and are escaped as `%C4%B1%B6%8A%81%D5%42%65` through
uri handling. After this the purpose can be found.

Examlpe. 

```logs
[2026-04-26 13:33:32] [API] Received nori-api URI: nori-api://127.0.0.1:48002/approve/3f1f90efc1be1d68058aebb47e6a7996%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-26 13:33:32] [SOCKET] nori-api path (safe decoded): /approve/3f1f90efc1be1d68058aebb47e6a7996%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-26 13:33:32] [API] Unknown nori-api path: /approve/3f1f90efc1be1d68058aebb47e6a7996%C4%B1%B6%8A%81%D5%42%65approval
[2026-04-26 13:33:32] [API] Magic number found in nori-api path with purpose: 'approval'
```