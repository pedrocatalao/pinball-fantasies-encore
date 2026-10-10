# The game files

The engine reads the original's own data, and it takes it from fixed addresses inside the
1994 MS-DOS release, so it needs **exactly** that release: re-releases and cracked copies
differ, and a folder holding one is refused rather than played wrongly. A cracked copy in
particular has a different `INTRO.PRG`, `TABLE1.PRG` and `TABLE2.PRG`.

Every file below is checked against its SHA-256 sum when the game starts:

```
619723e39acc003c64ae5f10159ae9da6192a28642c348f455bac447a1184967  INTRO.PRG
3b897533f11163934b8e4da038143e8f7339224421803ab4108c9d10f0a7bb4a  TABLE1.PRG
37019f7bd41d896a8f5a6383a2dffc3b3e4fc63fdf1aec1cc15db99110e581eb  TABLE2.PRG
da83ef5a7a471e6a6ad759126907076c81e92ffde6dec8e3de8e6052c6a98858  TABLE3.PRG
88f63edd4c7b50bd057397016d7aa962f0ed1c858f4a746f1ccf976f67494ebf  TABLE4.PRG
aa5003c275b494062f37f44e8c77105b8a420555f4bd6ff53d7698f89c540f21  MOD2.MOD
a0877e4372abe64b70d9e361bf257ea5a84c948771f0eace3433d5f6399060b5  TABLE1.MOD
728629c54311386781271308e181ac0435f0582e90870accff0a42270d467529  TABLE2.MOD
fb7bfd1c96a462cb03999d2e6f843a20d3de69ba05fcbd384a9f1c131b9a563a  TABLE3.MOD
31ad7e671ae77c07c3d075e2f1fecd3d918fd921fa23acd9a1b0b6fc07fbbcea  TABLE4.MOD
```

`INTRO.MOD` is needed as well but is not checked: the DOS game rewrites it as part of its
copy protection, so no two copies agree.

Where the files go is in the [README](../README.md); to point the game at another folder,
`--data <dir>` ([building.md](building.md#command-line)).
